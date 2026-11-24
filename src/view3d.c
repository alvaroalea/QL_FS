/* view3d.c - motor 3D: horizonte, escenario y avion exterior
 *
 * Coordenadas de vista: X derecha, Y arriba, Z adelante.
 * Para no salirse de 16 bits, cada objeto elige su unidad: 1/8 m << U, con
 * U creciendo con la distancia. La perspectiva X/Z no depende de la unidad.
 */
#include "view3d.h"
#ifdef HOST_DEBUG
#include <stdio.h>
#endif

ViewStats vstats;

#define MAXV   96
#define MAXCLIP 24
#define MAXITEMS 256

static s16 rel[MAXV * 3];
static s32 vw[MAXV * 3];
static s32 near_z;
static int obj_u, light_step;           /* escala del objeto actual: lejos -> sin contornos */

void attitude_matrix(u16 hdg, s16 pit, s16 bnk, s16 *m)
{
    s32 sh = isin(hdg), ch = icos(hdg);
    s32 sp = isin((u16)pit), cp = icos((u16)pit);
    s32 sb = isin((u16)bnk), cb = icos((u16)bnk);
    s32 r0x = ch, r0y = -sh;                         /* derecha sin alabeo */
    s32 u0x = -((sh * sp) >> 14), u0y = -((ch * sp) >> 14), u0z = cp;
    m[6] = (s16)((sh * cp) >> 14);                   /* adelante */
    m[7] = (s16)((ch * cp) >> 14);
    m[8] = (s16)sp;
    m[0] = (s16)((r0x * cb - u0x * sb) >> 14);       /* derecha */
    m[1] = (s16)((r0y * cb - u0y * sb) >> 14);
    m[2] = (s16)((-u0z * sb) >> 14);
    m[3] = (s16)((u0x * cb + r0x * sb) >> 14);       /* arriba */
    m[4] = (s16)((u0y * cb + r0y * sb) >> 14);
    m[5] = (s16)((u0z * cb) >> 14);
}

void cam_set(Camera *c, s32 x, s32 y, s32 z, u16 hdg, s16 pit, s16 bnk)
{
    c->x = x; c->y = y; c->z = z;
    c->hdg = hdg; c->pit = pit; c->bnk = bnk;
    attitude_matrix(hdg, pit, bnk, c->m);
}

void cam_zoom(Camera *c, int z256)
{
    /* 70 grados de campo horizontal a zoom 1: fx = 240/tan(35) = 343 */
    c->fx = (s16)((343L * z256) >> 8);
    c->fy = (s16)((228L * z256) >> 8);      /* pixel QL 1:1.5 */
}

#ifndef USE_ASM_XFORM
void xform3(const s16 *m, const s16 *in, s32 *out, int n)
{
    s32 x, y, z;
    while (n--) {
        x = in[0]; y = in[1]; z = in[2];
        out[0] = (m[0] * x + m[1] * y + m[2] * z) >> 14;
        out[1] = (m[3] * x + m[4] * y + m[5] * z) >> 14;
        out[2] = (m[6] * x + m[7] * y + m[8] * z) >> 14;
        in += 3; out += 3;
    }
}
#endif

/* ------------------------------------------------ cielo y suelo */
/* rellena la fila y en tres tramos segun f(x) = A*(x-cx) + B:
 * f > 0 cielo, -K < f <= 0 bruma (suelo mas alla de la visibilidad), f <= -K suelo */
static void sky_row(Surface *s, int y, s32 A, s32 B, s32 K, Fill fs, Fill fh, Fill fg)
{
    s32 x0 = s->cx0, x1 = s->cx1, xa, xb, t;
    Fill lo, hi;
    if (ABS(A) < 8) {
        gfx_hspan(s, y, (int)x0, (int)x1, B > 0 ? fs : (B > -K ? fh : fg));
        return;
    }
    xa = VIEW_CX + (-K - B) / A;          /* f = -K */
    xb = VIEW_CX + (0 - B) / A;           /* f = 0  */
    if (A > 0) { lo = fg; hi = fs; }      /* f crece con x: suelo, bruma, cielo */
    else { lo = fs; hi = fg; t = xa; xa = xb; xb = t; }
    if (xa > x0) gfx_hspan(s, y, (int)x0, (int)MIN(xa, x1 + 1) - 1, lo);
    if (xb > xa && xb > x0 && xa <= x1)
        gfx_hspan(s, y, (int)MAX(xa, x0), (int)MIN(xb, x1 + 1) - 1, fh);
    if (xb <= x1) gfx_hspan(s, y, (int)MAX(xb, x0), (int)x1, hi);
}

void view_sky(Surface *s, const Camera *c)
{
    s32 ux = c->m[2] >> 6, uy = c->m[5] >> 6, uz = c->m[8] >> 6;   /* Q8: sin desbordar con zoom x8 */
    s32 A, B, K, cz = c->z >> 8, h, tq, ff;
    int y;
    Fill fs, fg, fh;

    switch (c->light) {
    case LIGHT_NIGHT: fs = fill_tab[F_BLACK]; fg = fill_tab[F_BLACK]; fh = fs; break;
    case LIGHT_DUSK:  fs = fill_tab[F_SKYDUSK]; fg = fill_tab[F_DKGREEN]; fh = fill_tab[F_DKRED]; break;
    default:          fs = fill_tab[F_WHITE]; fg = fill_tab[F_GREEN]; fh = fill_tab[F_HAZE]; break;
    }
    if (c->cloud_base) {
        if (cz >= c->cloud_base && cz <= c->cloud_top) {   /* dentro de la nube */
            gfx_clear(s, fill_tab[c->light == LIGHT_NIGHT ? F_DKGREY : F_LTGREY]);
            return;
        }
        if (cz < c->cloud_base) fs = fill_tab[c->light == LIGHT_NIGHT ? F_BLACK : F_GREY];
        else fg = fh = fill_tab[c->light == LIGHT_NIGHT ? F_DKGREY : F_LTGREY];
    }
    /* banda de bruma: depresion bajo el horizonte del suelo a la distancia de visibilidad */
    h = MAX(cz - (c->ground >> 8), 2);
    tq = h * 256 / MAX(c->vis, 1000);                  /* tan(depresion) Q8 */
    ff = (s32)c->fx * c->fy;
    K = MIN(tq, 0x7FFFFFFFL / MAX(ff, 1)) * ff;
    A = ux * c->fy;
    for (y = s->cy0; y <= s->cy1; y++) {
        B = uy * c->fx * (VIEW_CY - y) + uz * ff;
        sky_row(s, y, A, B, K, fs, fh, fg);
    }
}

/* ------------------------------------------------ utilidades de recorte */
static int nb(s32 v)
{
    int n = 0;
    if (v < 0) v = -v;
    while (v) { v >>= 1; n++; }
    return n;
}

static s32 muldiv(s32 a, s32 b, s32 d)       /* a*b/d sin desbordar */
{
    int k = 0;
    if (d == 0) return 0;
    while (nb(a) + nb(b) > 30) {
        if (ABS(b) > ABS(a)) { b >>= 1; d >>= 1; if (!d) return 0; }
        else { a >>= 1; k++; }
    }
    return ((a * b) / d) << k;
}

static void project(const Camera *c, s32 x, s32 y, s32 z, s32 *sx, s32 *sy)
{
    *sx = VIEW_CX + muldiv(x, c->fx, z);
    *sy = VIEW_CY - muldiv(y, c->fy, z);
}

/* recorta un poligono 3D contra el plano z = near_z y lo proyecta */
static int clip_project(const Camera *c, const u8 *idx, int n, s32 *out)
{
    s32 in[MAXCLIP * 3], cl[MAXCLIP * 3];
    int i, m = 0, j, ia, ib;
    s32 *a, *b, t;
    if (n > MAXCLIP - 4) n = MAXCLIP - 4;
    for (i = 0; i < n; i++) {
        in[i * 3] = vw[idx[i] * 3];
        in[i * 3 + 1] = vw[idx[i] * 3 + 1];
        in[i * 3 + 2] = vw[idx[i] * 3 + 2];
    }
    for (i = 0; i < n; i++) {
        j = (i + 1) % n;
        a = in + i * 3; b = in + j * 3;
        ia = a[2] >= near_z; ib = b[2] >= near_z;
        if (ia) { cl[m * 3] = a[0]; cl[m * 3 + 1] = a[1]; cl[m * 3 + 2] = a[2]; m++; }
        if (ia != ib) {
            t = b[2] - a[2];
            cl[m * 3] = a[0] + muldiv(b[0] - a[0], near_z - a[2], t);
            cl[m * 3 + 1] = a[1] + muldiv(b[1] - a[1], near_z - a[2], t);
            cl[m * 3 + 2] = near_z;
            m++;
        }
    }
    for (i = 0; i < m; i++) project(c, cl[i * 3], cl[i * 3 + 1], cl[i * 3 + 2], &out[i * 2], &out[i * 2 + 1]);
    return m;
}

void gfx_poly32(Surface *s, int n, const s32 *xy, Fill f);

/* area con signo (para ocultacion de caras traseras) */
static s32 area2(const s32 *p, int n)
{
    s32 mx = 0, a = 0, x0, y0, x1, y1;
    int i, sh = 0;
    for (i = 0; i < 2 * n; i++) if (ABS(p[i]) > mx) mx = ABS(p[i]);
    while ((mx >> sh) > 2047) sh++;
    for (i = 0; i < n; i++) {
        x0 = p[2 * i] >> sh; y0 = p[2 * i + 1] >> sh;
        x1 = p[2 * ((i + 1) % n)] >> sh; y1 = p[2 * ((i + 1) % n) + 1] >> sh;
        a += x0 * y1 - x1 * y0;
    }
    return a;
}

static void draw_poly(Surface *s, const Camera *c, const u8 *f, int cull)
{
    s32 p[MAXCLIP * 2];
    int n = f[0], a = f[1], m, i;
    m = clip_project(c, f + 2, n, p);
    if (m < 3) return;
    if (cull && area2(p, m) >= 0) return;
    vstats.faces++;
#ifdef HOST_DEBUG
    if (FA_FILL(a) == F_BROWN && !(a & FA_NOFILL)) {
        int q; printf("BROWN m=%d:", m); for (q = 0; q < m; q++) printf(" [%ld,%ld]", (long)p[2*q], (long)p[2*q+1]); printf("\n");
    }
#endif
    if (!(a & FA_NOFILL)) gfx_poly32(s, m, p, fill_tab[FA_FILL(a)]);
    if ((a & FA_NOFILL) || ((a & FA_OUTLINE) && obj_u < 1))
        for (i = 0; i < m; i++)
            gfx_line(s, p[2 * i], p[2 * i + 1], p[2 * ((i + 1) % m)], p[2 * ((i + 1) % m) + 1], FA_LCOL(a));
}

static void draw_seg(Surface *s, const Camera *c, int i0, int i1, int col)
{
    s32 a[3], b[3], t, x0, y0, x1, y1;
    int k;
    for (k = 0; k < 3; k++) { a[k] = vw[i0 * 3 + k]; b[k] = vw[i1 * 3 + k]; }
    if (a[2] < near_z && b[2] < near_z) return;
    if (a[2] < near_z) {
        t = b[2] - a[2];
        a[0] += muldiv(b[0] - a[0], near_z - a[2], t);
        a[1] += muldiv(b[1] - a[1], near_z - a[2], t);
        a[2] = near_z;
    } else if (b[2] < near_z) {
        t = a[2] - b[2];
        b[0] += muldiv(a[0] - b[0], near_z - b[2], t);
        b[1] += muldiv(a[1] - b[1], near_z - b[2], t);
        b[2] = near_z;
    }
    project(c, a[0], a[1], a[2], &x0, &y0);
    project(c, b[0], b[1], b[2], &x1, &y1);
    vstats.lines++;
    gfx_line(s, x0, y0, x1, y1, col);
}

/* transforma los vertices de un objeto. Devuelve 0 si no se dibuja. */
static int xform_obj(const Camera *c, const Obj *o, s32 dmax)
{
    s32 dx, dy, dz, d;
    int U = 0, k, n = o->nv;
    s16 rx, ry, rz;
    const s16 *v = o->v;
    if (n > MAXV) n = MAXV;
    dx = ((s32)o->ox << 8) - c->x;
    dy = ((s32)o->oy << 8) - c->y;
    dz = ((s32)o->oz << 8) - c->z;
    d = MAX(ABS(dx), ABS(dy)) >> 8;
    if (d - (s32)o->radius > dmax) return 0;
    d = d + o->radius + (ABS(dz) >> 8);
    while (((d << 3) >> U) > 28000L) { U++; if (U > 9) return 0; }
    rx = (s16)(dx >> (5 + U)); ry = (s16)(dy >> (5 + U)); rz = (s16)(dz >> (5 + U));
    light_step = 1 + (U > 2 ? (1 << (U - 2)) - 1 : 0);   /* luces lejanas: 1 de cada k */
    /* descarte por esfera envolvente contra la piramide de vision: un solo
     * vertice transformado decide si merece la pena transformar el resto */
    {
        s16 c3[3];
        s32 cv[3], r = ((s32)o->radius * 9 >> U) + 8;
        c3[0] = rx; c3[1] = ry; c3[2] = rz;
        xform3(c->m, c3, cv, 1);
        if (cv[2] + r < 0) return 0;
        if ((ABS(cv[0]) - r) * c->fx > (cv[2] + r) * (VIEW_W / 2)) return 0;
        if ((ABS(cv[1]) - r) * c->fy > (cv[2] + r) * (VIEW_H / 2)) return 0;
    }
    if (o->type == OBJ_LIGHTS) {
        /* solo se transforman las luces que se van a pintar */
        int j = 0;
        for (k = 0; k < n; k += light_step, j++) {
            rel[j * 3]     = (s16)(rx + (((s32)v[k * 3] << o->vshift) >> U));
            rel[j * 3 + 1] = (s16)(ry + (((s32)v[k * 3 + 1] << o->vshift) >> U));
            rel[j * 3 + 2] = (s16)(rz + (((s32)v[k * 3 + 2] << o->vshift) >> U));
        }
        xform3(c->m, rel, vw, j);
        near_z = MAX(4 >> U, 1);
        obj_u = U;
        vstats.verts += j;
        return j;
    }
    for (k = 0; k < n; k++) {
        rel[k * 3]     = (s16)(rx + (((s32)v[k * 3] << o->vshift) >> U));
        rel[k * 3 + 1] = (s16)(ry + (((s32)v[k * 3 + 1] << o->vshift) >> U));
        rel[k * 3 + 2] = (s16)(rz + (((s32)v[k * 3 + 2] << o->vshift) >> U));
    }
    xform3(c->m, rel, vw, n);
    near_z = 4 >> U;
    if (near_z < 1) near_z = 1;
    obj_u = U;
    vstats.verts += n;
    /* descarte rapido: todo detras de la camara */
    for (k = 0; k < n; k++) if (vw[k * 3 + 2] >= near_z) break;
    return k < n;
}

static void draw_sorted(Surface *s, const Camera *c, const Obj *o)
{
    const u8 *f = o->f, *fp[64];
    s32 key[64], t;
    int i, j, k, nf = MIN(o->nf, 64), ord[64], to;
    for (i = 0; i < nf; i++) {
        fp[i] = f; t = 0;
        for (k = 0; k < f[0]; k++) t += vw[f[2 + k] * 3 + 2];
        key[i] = t / (f[0] ? f[0] : 1);
        ord[i] = i;
        f += 2 + f[0];
    }
    for (i = 1; i < nf; i++) {
        to = ord[i]; j = i - 1;
        while (j >= 0 && key[ord[j]] < key[to]) { ord[j + 1] = ord[j]; j--; }
        ord[j + 1] = to;
    }
    for (i = 0; i < nf; i++) draw_poly(s, c, fp[ord[i]], 1);
}

static void draw_obj(Surface *s, const Camera *c, const Obj *o)
{
    const u8 *f = o->f;
    int i, k, n, col;
    s32 sx, sy;
    if (o->type == OBJ_SOLID && (o->flags & OF_SORT)) { draw_sorted(s, c, o); return; }
    for (i = 0; i < o->nf; i++) {
        n = f[0];
        switch (o->type) {
        case OBJ_GROUND: draw_poly(s, c, f, 0); break;
        case OBJ_SOLID:  draw_poly(s, c, f, 1); break;
        case OBJ_LINES:
            for (k = 0; k + 1 < n; k++) draw_seg(s, c, f[2 + k], f[3 + k], f[1] & 3);
            break;
        case OBJ_LIGHTS:
            /* las luces se guardan en orden: el vertice j-esimo transformado es
             * el (j*light_step)-esimo del objeto (ver xform_obj) */
            col = f[1] & 3;
            for (k = 0; k < n; k += light_step) {
                s32 *p = vw + (f[2 + k] / light_step) * 3;
                if (p[2] < near_z) continue;
                project(c, p[0], p[1], p[2], &sx, &sy);
                gfx_pixel(s, (int)sx, (int)sy, col);
                if (obj_u == 0) gfx_pixel(s, (int)sx + 1, (int)sy, col);
            }
            break;
        }
        f += 2 + n;
    }
}

typedef struct { const Obj *o; s32 key; } Item;
static Item items[MAXITEMS];

void view_world(Surface *s, const Camera *c, World *w)
{
    Tile *tl[TILE_CACHE];
    int nt, i, k, ni = 0, ng, j;
    s32 dmax = c->vis, cx = c->x >> 8, cy = c->y >> 8, d;
    Item t;
    vstats.objs = vstats.faces = vstats.lines = vstats.verts = 0;
    nt = world_tiles_near(w, cx, cy, dmax, tl, TILE_CACHE);
    vstats.tiles = nt;
    /* 1) objetos de suelo: primero los grandes (campos, lagos), luego pistas */
    for (i = 0; i < nt; i++)
        for (k = 0; k < tl[i]->nobj; k++) {
            const Obj *o = &tl[i]->obj[k];
            if (ni >= MAXITEMS) break;
            if (c->light == LIGHT_NIGHT) {
                if (o->type != OBJ_LIGHTS && !(o->flags & OF_NIGHT)) continue;
            } else if (o->type == OBJ_LIGHTS && !(o->flags & OF_DAYLIGHT) && c->light == LIGHT_DAY)
                continue;
            if (c->cloud_base && (c->z >> 8) > c->cloud_top && o->type != OBJ_SOLID) continue;
            d = MAX(ABS(o->ox - cx), ABS(o->oy - cy));
            if (d - (s32)o->radius > MIN(dmax, (s32)o->lod << 8)) continue;
            items[ni].o = o;
            items[ni].key = (o->type == OBJ_GROUND || o->type == OBJ_LINES)
                            ? 0x40000000L + ((s32)o->radius << 8) + o->type * 0 : d;
            if (o->type == OBJ_LINES) items[ni].key = 0x40000000L - 1 - d;
            ni++;
        }
    /* orden descendente de clave: suelo (por tamano) y luego solidos de lejos a cerca */
    for (i = 1; i < ni; i++) {
        t = items[i]; j = i - 1;
        while (j >= 0 && items[j].key < t.key) { items[j + 1] = items[j]; j--; }
        items[j + 1] = t;
    }
    ng = ni;
    for (i = 0; i < ng; i++) {
        const Obj *o = items[i].o;
        if (xform_obj(c, o, dmax)) { vstats.objs++; draw_obj(s, c, o); }
    }
}

/* ------------------------------------------------ avion en vista exterior */
void view_model(Surface *s, const Camera *c, const Model *mdl,
                s32 ax, s32 ay, s32 az, const s16 *am)
{
    s16 C[9], base16[3];
    s32 base[3], key[64], tk;
    int i, j, k, n, order[64], nf, to;
    const u8 *f, *fp[64];
    s16 *v = mdl->v;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            C[i * 3 + j] = (s16)((MULS16(c->m[i * 3], am[j * 3]) + MULS16(c->m[i * 3 + 1], am[j * 3 + 1]) +
                                  MULS16(c->m[i * 3 + 2], am[j * 3 + 2])) >> 14);
    base16[0] = (s16)CLAMP((ax - c->x) >> 5, -30000L, 30000L);
    base16[1] = (s16)CLAMP((ay - c->y) >> 5, -30000L, 30000L);
    base16[2] = (s16)CLAMP((az - c->z) >> 5, -30000L, 30000L);
    xform3(c->m, base16, base, 1);
    n = MIN(mdl->nv, MAXV);
    xform3(C, v, vw, n);
    for (i = 0; i < n; i++) { vw[i * 3] += base[0]; vw[i * 3 + 1] += base[1]; vw[i * 3 + 2] += base[2]; }
    near_z = 4;
    /* caras ordenadas por profundidad media (pintor), sin ocultacion */
    f = mdl->f; nf = MIN(mdl->nf, 64);
    for (i = 0; i < nf; i++) {
        fp[i] = f; tk = 0;
        for (k = 0; k < f[0]; k++) tk += vw[f[2 + k] * 3 + 2];
        key[i] = tk / (f[0] ? f[0] : 1);
        order[i] = i;
        f += 2 + f[0];
    }
    for (i = 1; i < nf; i++) {
        to = order[i]; j = i - 1;
        while (j >= 0 && key[order[j]] < key[to]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = to;
    }
    for (i = 0; i < nf; i++) {
        f = fp[order[i]];
        if (f[0] == 2) draw_seg(s, c, f[2], f[3], f[1] & 3);
        else draw_poly(s, c, f, 0);
    }
}

#ifdef HOST_DEBUG
#include <stdio.h>
void view_debug(const Camera *c, World *w)
{
    Tile *tl[TILE_CACHE];
    int nt = world_tiles_near(w, c->x >> 8, c->y >> 8, c->vis, tl, TILE_CACHE), i, k, j;
    for (i = 0; i < nt; i++)
        for (k = 0; k < tl[i]->nobj; k++) {
            const Obj *o = &tl[i]->obj[k];
            const u8 *f;
            if (!(o->flags & OF_MOUNTAIN)) continue;
            if (!xform_obj(c, o, c->vis)) continue;
            printf("montana %ld,%ld near %ld\n", (long)o->ox, (long)o->oy, (long)near_z);
            f = o->f;
            for (j = 0; j < o->nf; j++) {
                s32 p[MAXCLIP * 2];
                int m = clip_project(c, f + 2, f[0], p), q;
                printf("  cara %d:", j);
                for (q = 0; q < f[0]; q++) printf(" (%ld,%ld,%ld)", (long)vw[f[2+q]*3], (long)vw[f[2+q]*3+1], (long)vw[f[2+q]*3+2]);
                printf(" ->");
                for (q = 0; q < m; q++) printf(" [%ld,%ld]", (long)p[2*q], (long)p[2*q+1]);
                printf(" area %ld\n", (long)(m >= 3 ? area2(p, m) : 0));
                f += 2 + f[0];
            }
        }
}
#endif
