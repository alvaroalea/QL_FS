/* view3d.c - motor 3D: horizonte, escenario y avion exterior
 *
 * Coordenadas de vista: X derecha, Y arriba, Z adelante.
 * Para no salirse de 16 bits, cada objeto elige su unidad: 1/8 m << U, con
 * U creciendo con la distancia. La perspectiva X/Z no depende de la unidad.
 */
#include "view3d.h"
#if defined(HOST_DEBUG) || defined(OBJ_DUMP)
#include <stdio.h>
#endif

ViewStats vstats;

#ifdef USE_ASM_XOBJ
void xform_obj3(const s16 *m, const s16 *v, s32 *out, int cnt, int step, int vshift, int U, int rx, int ry, int rz);
int xform_pre(const Camera *c, const Obj *o, s32 dmax, s32 *res);   /* prologo de xform_obj (ql68k.s) */
#endif

#ifdef USE_ASM_3D
int proj_all(const s32 *v, int n, s32 near_z, int fx, int fy, s32 *out, u8 *front);
void proj_pt(const s32 *v, int fx, int fy, s32 *out);
void draw_lights(Surface *s, const s32 *vw, const u8 *idx, int n, int step, int shift, s32 near_z, int fx, int fy, int col, int dbl);
int poly_fast(const u8 *idx, int n, const u8 *front, const s32 *pxy_, s32 *out, int cull);
#endif

#define MAXV   96
#define MAXCLIP 24
#define MAXITEMS 256
#ifndef BEHIND_CULL
#define BEHIND_CULL 1      /* descartar objetos detras de la camara antes de ordenarlos */
#endif
#ifndef LIGHT_U0
#define LIGHT_U0 0         /* a partir de que escala U (cada U dobla la distancia) se reducen las luces: 1 de 2, 1 de 4... */
#endif
#ifndef MIN_PX
#define MIN_PX 1           /* radio proyectado minimo (pixeles) para dibujar un solido; 0 = sin descarte */
#endif
#ifndef TINY_PX
#define TINY_PX 1          /* una cara cuya caja proyectada no pasa de TINY_PX pixeles no se pinta; 1 tambien en poly_fast (ql68k.s) */
#endif

static s16 rel[MAXV * 3];
static s32 vw[MAXV * 3];
static s32 near_z;
static int obj_u, light_step, light_shift;           /* escala del objeto actual: lejos -> sin contornos */

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

/* ------------------------------------------------ proyeccion */
/* sx = VIEW_CX + x*fx/z, sy = VIEW_CY - y*fy/z (z >= 1), division truncada hacia cero. Si x, y o z no caben en 16 bits con signo se
 * desplazan los tres a la vez hasta que quepan (la razon se conserva). En el QL es pj_xy (ql68k.s), que hace exactamente esto con
 * MULS y DIVS (o dos DIVU si el cociente no cabe en 16 bits). */
#ifndef USE_ASM_3D
static void proj_pt(const s32 *v, int fx, int fy, s32 *out)
{
    s32 x = v[0], y = v[1], z = v[2];
    while (z > 32767L || x > 32767L || x < -32768L || y > 32767L || y < -32768L) { x >>= 1; y >>= 1; z >>= 1; }
    if (z == 0) z = 1;
    out[0] = VIEW_CX + (x * fx) / z;
    out[1] = VIEW_CY - (y * fy) / z;
}
#endif

static void project(const Camera *c, s32 x, s32 y, s32 z, s32 *sx, s32 *sy)
{
    s32 v[3], o[2];
    v[0] = x; v[1] = y; v[2] = z;
    proj_pt(v, c->fx, c->fy, o);
    *sx = o[0]; *sy = o[1];
}

/* proyeccion de todos los vertices del objeto, una sola vez (las caras comparten vertices) */
static s32 pxy[MAXV * 2];
static u8 pfront[MAXV];                      /* 1 si el vertice esta delante del plano cercano */
#ifdef USE_ASM_XOBJ
#define FOFF(t, f) ((int)((char *)&((t *)0)->f - (char *)0))
/* xform_pre (ql68k.s) lee estos campos por desplazamiento fijo: si cambian las estructuras, que no compile */
typedef char chk_c_x[(FOFF(Camera, x) == 0 && FOFF(Camera, y) == 4 && FOFF(Camera, z) == 8) ? 1 : -1];
typedef char chk_c_m[(FOFF(Camera, m) == 18 && FOFF(Camera, fx) == 36 && FOFF(Camera, fy) == 38) ? 1 : -1];
typedef char chk_o_t[(FOFF(Obj, type) == 0 && FOFF(Obj, ox) == 4 && FOFF(Obj, oy) == 8) ? 1 : -1];
typedef char chk_o_z[(FOFF(Obj, oz) == 12 && FOFF(Obj, radius) == 14 && OBJ_SOLID == 2 && MIN_PX == 1) ? 1 : -1];
#endif
typedef char chk_view_cx[(VIEW_CX == 256) ? 1 : -1];     /* proj_all (ql68k.s) lleva 256 y 48 escritos */
typedef char chk_view_cy[(VIEW_CY == 48) ? 1 : -1];
typedef char chk_view_w2[(VIEW_W / 2 == 240) ? 1 : -1];  /* xform_obj multiplica por 240 y 48 con desplazamientos */
typedef char chk_view_h2[(VIEW_H / 2 == 48) ? 1 : -1];
#if MIN_PX == 1
#define PXMUL(v) (v)
#else
#define PXMUL(v) ((v) * MIN_PX)
#endif

#ifndef USE_ASM_3D
int proj_all(const s32 *v, int n, s32 near_z, int fx, int fy, s32 *out, u8 *front)
{
    int k, cnt = 0;
    for (k = 0; k < n; k++, v += 3, out += 2) {
        if (v[2] >= near_z) {
            front[k] = 1;
            proj_pt(v, fx, fy, out);
            cnt++;
        } else front[k] = 0;
    }
    return cnt;
}

/* luces: el vertice vw[idx[k] >> shift] de cada `step` indices, un punto (dos si dbl) de color col; en el QL es draw_lights (ql68k.s) */
void draw_lights(Surface *s, const s32 *vw, const u8 *idx, int n, int step, int shift, s32 nz, int fx, int fy, int col, int dbl)
{
    int k;
    s32 o[2];
    for (k = 0; k < n; k += step) {
        const s32 *p = vw + (idx[k] >> shift) * 3;
        if (p[2] < nz) continue;
        proj_pt(p, fx, fy, o);
        gfx_pixel(s, (int)o[0], (int)o[1], col);
        if (dbl) gfx_pixel(s, (int)o[0] + 1, (int)o[1], col);
    }
}
#endif

/* recorta un poligono 3D contra el plano z = near_z y lo proyecta (camino general, para caras que cruzan el plano) */
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
        j = i + 1; if (j == n) j = 0;
        a = in + i * 3; b = in + j * 3;
        ia = a[2] >= near_z; ib = b[2] >= near_z;
        if (ia) { cl[m * 3] = a[0]; cl[m * 3 + 1] = a[1]; cl[m * 3 + 2] = a[2]; m++; }
        if (ia != ib) {
            t = b[2] - a[2];
            cl[m * 3] = lerp_div(a[0], b[0], near_z - a[2], t);
            cl[m * 3 + 1] = lerp_div(a[1], b[1], near_z - a[2], t);
            cl[m * 3 + 2] = near_z;
            m++;
        }
    }
    for (i = 0; i < m; i++) project(c, cl[i * 3], cl[i * 3 + 1], cl[i * 3 + 2], &out[i * 2], &out[i * 2 + 1]);
    return m;
}

void gfx_poly32(Surface *s, int n, const s32 *xy, Fill f);

/* area con signo (para ocultacion de caras traseras): coordenadas reducidas a 11 bits para sumar con MULS de 16 bits */
static s32 area2(const s32 *p, int n)
{
    s32 mx = 0, a = 0, v;
    int i, sh = 0, x0, y0, x1, y1;
    for (i = 0; i < 2 * n; i++) { v = p[i]; if (v < 0) v = -v; if (v > mx) mx = v; }
    while ((mx >> sh) > 2047) sh++;
    x0 = (int)(p[2 * n - 2] >> sh); y0 = (int)(p[2 * n - 1] >> sh);
    for (i = 0; i < n; i++) {
        x1 = (int)(p[2 * i] >> sh); y1 = (int)(p[2 * i + 1] >> sh);
        a += MULS16(x0, y1) - MULS16(x1, y0);
        x0 = x1; y0 = y1;
    }
    return a;
}

/* profundidad media de una cara (suma de z de sus vertices entre n) sin la division de biblioteca para n = 2, 3, 4 */
static s32 face_key(const u8 *f)
{
    s32 t = 0;
    int k, n = f[0];
    for (k = 0; k < n; k++) t += vw[f[2 + k] * 3 + 2];
    switch (n) {
    case 2: return t >> 1;
    case 3: return fxmul(t, 21846L);                       /* t/3 por reciproco */
    case 4: return t >> 2;
    default: return n ? t / n : 0;
    }
}

#ifndef USE_ASM_3D
/* cara con n indices: -1 si algun vertice esta detras del plano (hay que recortar); si no copia las proyecciones a out y, con
 * cull, devuelve 0 para una cara trasera (area >= 0) o de un solo pixel, y 1 para una visible; sin cull devuelve 1 */
int poly_fast(const u8 *idx, int n, const u8 *front, const s32 *pxy_, s32 *out, int cull)
{
    int i, k;
    s32 lx = MDQ_BIG, hx = -MDQ_BIG, ly = MDQ_BIG, hy = -MDQ_BIG, x, y;
    if (n < 3) return -1;
    for (i = 0; i < n; i++) if (!front[idx[i]]) return -1;
    for (i = 0; i < n; i++) {
        k = idx[i];
        x = out[2 * i] = pxy_[2 * k];
        y = out[2 * i + 1] = pxy_[2 * k + 1];
        if (x < lx) lx = x;
        if (x > hx) hx = x;
        if (y < ly) ly = y;
        if (y > hy) hy = y;
    }
    if (hx - lx <= TINY_PX && hy - ly <= TINY_PX) return 0;      /* cara de un pixel: no se pinta */
    if (!cull) return 1;
    return area2(out, n) < 0;
}
#endif

/* rellena y contornea una cara ya proyectada (m vertices en p) */
static void paint_face(Surface *s, int a, const s32 *p, int m)
{
    int i;
    vstats.faces++;
    if (!(a & FA_NOFILL)) gfx_poly32(s, m, p, fill_tab[FA_FILL(a)]);
    if ((a & FA_NOFILL) || ((a & FA_OUTLINE) && obj_u < 1))
        for (i = 0; i < m; i++)
            gfx_line(s, p[2 * i], p[2 * i + 1], p[2 * ((i + 1) % m)], p[2 * ((i + 1) % m) + 1], FA_LCOL(a));
}

static void draw_poly(Surface *s, const Camera *c, const u8 *f, int cull)
{
    s32 p[MAXCLIP * 2];
    int n = f[0], m, rc;
    rc = n <= MAXCLIP - 4 ? poly_fast(f + 2, n, pfront, pxy, p, cull) : -1;
    if (rc == 0) return;                          /* cara trasera */
    if (rc > 0) m = n;
    else {
        m = clip_project(c, f + 2, n, p);
        if (m < 3) return;
        if (cull && area2(p, m) >= 0) return;
    }
    paint_face(s, f[1], p, m);
}

static void draw_seg(Surface *s, const Camera *c, int i0, int i1, int col)
{
    s32 a[3], b[3], t, x0, y0, x1, y1;
    int k;
    for (k = 0; k < 3; k++) { a[k] = vw[i0 * 3 + k]; b[k] = vw[i1 * 3 + k]; }
    if (a[2] < near_z && b[2] < near_z) return;
    if (a[2] < near_z) {
        t = b[2] - a[2];
        a[0] = lerp_div(a[0], b[0], near_z - a[2], t);
        a[1] = lerp_div(a[1], b[1], near_z - a[2], t);
        a[2] = near_z;
    } else if (b[2] < near_z) {
        t = a[2] - b[2];
        b[0] = lerp_div(b[0], a[0], near_z - b[2], t);
        b[1] = lerp_div(b[1], a[1], near_z - b[2], t);
        b[2] = near_z;
    }
    project(c, a[0], a[1], a[2], &x0, &y0);
    project(c, b[0], b[1], b[2], &x1, &y1);
    vstats.lines++;
    gfx_line(s, x0, y0, x1, y1, col);
}

#ifndef USE_ASM_XOBJ
/* prepara y transforma cnt vertices del objeto (uno de cada `step`): rel = r + ((v << vshift) >> U) y out = m * rel >> 14.
 * En el QL es una sola rutina en ensamblador (xform_obj3, ql68k.s); aqui hace exactamente lo mismo. */
static void xform_obj3(const s16 *m, const s16 *v, s32 *out, int cnt, int step, int vshift, int U, int rx, int ry, int rz)
{
    int j, k = 0;
    for (j = 0; j < cnt; j++, k += step) {
        rel[0] = (s16)(rx + (((s32)v[k * 3] << vshift) >> U));
        rel[1] = (s16)(ry + (((s32)v[k * 3 + 1] << vshift) >> U));
        rel[2] = (s16)(rz + (((s32)v[k * 3 + 2] << vshift) >> U));
        xform3(m, rel, out + j * 3, 1);
    }
}
#endif

/* transforma los vertices de un objeto. Devuelve 0 si no se dibuja. */
#ifndef USE_ASM_XOBJ
/* prologo de xform_obj: 0 si el objeto no se dibuja; si no res = { U, rx, ry, rz }. En el QL es xform_pre (ql68k.s); aqui hace lo mismo. */
static int xform_pre(const Camera *c, const Obj *o, s32 dmax, s32 *res)
{
    s32 dx, dy, dz, d;
    int U = 0;
    s16 rx, ry, rz;
    dx = ((s32)o->ox << 8) - c->x;
    dy = ((s32)o->oy << 8) - c->y;
    dz = ((s32)o->oz << 8) - c->z;
    d = MAX(ABS(dx), ABS(dy)) >> 8;
    if (d - (s32)o->radius > dmax) return 0;
    d = d + o->radius + (ABS(dz) >> 8);
    while (((d << 3) >> U) > 28000L) { U++; if (U > 9) return 0; }
    rx = (s16)(dx >> (5 + U)); ry = (s16)(dy >> (5 + U)); rz = (s16)(dz >> (5 + U));
    /* descarte por esfera envolvente contra la piramide de vision: un solo vertice transformado decide si merece la pena
     * transformar el resto */
    {
        s16 c3[3];
        s32 cv[3], r = ((((s32)o->radius << 3) + o->radius) >> U) + 8, z2;       /* radio*9 sin multiplicar */
        c3[0] = rx; c3[1] = ry; c3[2] = rz;
        xform3(c->m, c3, cv, 1);
        z2 = cv[2] + r;
        if (z2 < 0) return 0;
        /* (|x|-r)*fx > (z+r)*240 y (|y|-r)*fy > (z+r)*48 */
        if (((cv[0] < 0 ? -cv[0] : cv[0]) - r) * c->fx > (z2 << 8) - (z2 << 4)) return 0;     /* VIEW_W / 2 = 240 */
        if (((cv[1] < 0 ? -cv[1] : cv[1]) - r) * c->fy > (z2 << 5) + (z2 << 4)) return 0;     /* VIEW_H / 2 = 48 */
        /* solidos menores de un pixel: no se dibujan (el suelo y las lineas son grandes aunque sean finos) */
        if (MIN_PX && o->type == OBJ_SOLID && r * c->fx < PXMUL(cv[2])) return 0;
    }
    res[0] = U; res[1] = rx; res[2] = ry; res[3] = rz;
    return 1;
}
#endif

static int xform_obj(const Camera *c, const Obj *o, s32 dmax)
{
    s32 res[4];
    int U, n = o->nv;
    s16 rx, ry, rz;
    const s16 *v = o->v;
    if (n > MAXV) n = MAXV;
    if (!xform_pre(c, o, dmax, res)) return 0;
    U = (int)res[0]; rx = (s16)res[1]; ry = (s16)res[2]; rz = (s16)res[3];
    light_shift = U > LIGHT_U0 ? U - LIGHT_U0 : 0;       /* luces lejanas: 1 de cada 2^light_shift */
    light_step = 1 << light_shift;
    if (o->type == OBJ_LIGHTS) {
        /* solo se transforman las luces que se van a pintar */
        int j = (n + light_step - 1) >> light_shift;
        xform_obj3(c->m, v, vw, j, light_step, o->vshift, U, rx, ry, rz);
        near_z = MAX(4 >> U, 1);
        obj_u = U;
        vstats.verts += j;
        return j;
    }
    xform_obj3(c->m, v, vw, n, 1, o->vshift, U, rx, ry, rz);
    near_z = 4 >> U;
    if (near_z < 1) near_z = 1;
    obj_u = U;
    vstats.verts += n;
    /* proj_all devuelve cuantos vertices estan delante: ninguno = todo detras de la camara */
    return proj_all(vw, n, near_z, c->fx, c->fy, pxy, pfront) != 0;
}

#define SORTF 64
static s32 sbuf[SORTF][16];                  /* proyeccion de cada cara visible (hasta 8 vertices) */

/* caras de un solido concavo (montanas, manzanas): se descartan primero las traseras (la mitad, sin clave ni ordenacion) y
 * solo las visibles se ordenan por profundidad (pintor, de lejos a cerca) */
static void draw_sorted(Surface *s, const Camera *c, const Obj *o)
{
    const u8 *f = o->f, *fp[SORTF];
    s32 key[SORTF];
    int i, j, nf = MIN(o->nf, SORTF), nv = 0, n, rc, ord[SORTF], fm[SORTF], to;
    for (i = 0; i < nf; i++) {
        n = f[0];
        rc = n <= 8 ? poly_fast(f + 2, n, pfront, pxy, sbuf[nv], 1) : -1;
        if (rc != 0) {                            /* 1 = visible, -1 = hay que recortarla */
            fp[nv] = f; fm[nv] = rc > 0 ? n : -1;
            key[nv] = face_key(f);
            ord[nv] = nv;
            nv++;
        }
        f += 2 + n;
    }
    for (i = 1; i < nv; i++) {
        to = ord[i]; j = i - 1;
        while (j >= 0 && key[ord[j]] < key[to]) { ord[j + 1] = ord[j]; j--; }
        ord[j + 1] = to;
    }
    for (i = 0; i < nv; i++) {
        to = ord[i];
        if (fm[to] > 0) paint_face(s, fp[to][1], sbuf[to], fm[to]);
        else draw_poly(s, c, fp[to], 1);
    }
}

static void draw_obj(Surface *s, const Camera *c, const Obj *o)
{
    const u8 *f = o->f;
    int i, k, n;
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
            draw_lights(s, vw, f + 2, n, light_step, light_shift, near_z, c->fx, c->fy, f[1] & 3, obj_u == 0);
            break;
        }
        f += 2 + n;
    }
}

typedef struct { const Obj *o; s32 key; } Item;
static Item items[MAXITEMS];
static int n_items, next_item;            /* lista ordenada del cuadro y primer objeto sin dibujar */

/* Primera mitad de view_world: reune los objetos visibles y los ordena (de lejos a cerca); devuelve el coste estimado de
 * dibujarlos (vertices + caras). Se pueden dibujar en uno o varios cuadros con view_world_step. Los tiles quedan protegidos
 * del desalojo mientras no se vuelva a llamar a world_tiles_near (solo la vista lo hace). */
/* reune en items[] los objetos de una tesela que pueden verse (alcance, luz, nubes, detras de la camara) */
static int gather_tile(const Tile *t, const Camera *c, int ni, s32 cx, s32 cy, s32 dmax)
{
    const Obj *o = t->obj;
    int k, night = c->light == LIGHT_NIGHT, above = c->cloud_base && (c->z >> 8) > c->cloud_top;
    s32 d, dx, dy, cz = c->z >> 8, dm = dmax;
    s16 m6 = c->m[6], m7 = c->m[7], m8 = c->m[8];
    Item *it = &items[ni];
    for (k = t->nobj; k > 0; k--, o++) {
        dx = o->ox - cx; if (dx < 0) dx = -dx;
        dy = o->oy - cy; if (dy < 0) dy = -dy;
        d = dx > dy ? dx : dy;
        if (d > o->reach) continue;                              /* fuera del alcance propio (lod) */
        if (d - (s32)o->radius > dm) continue;                   /* y de la visibilidad */
        if (night) {
            if (o->type != OBJ_LIGHTS && !(o->flags & OF_NIGHT)) continue;
        } else if (o->type == OBJ_LIGHTS && !(o->flags & OF_DAYLIGHT) && c->light == LIGHT_DAY)
            continue;
        if (above && o->type != OBJ_SOLID) continue;
        /* detras de la camara (con el radio de margen): ni se transforma ni entra en la lista que se ordena */
        if (BEHIND_CULL && (MULS16((o->ox - cx) >> 3, m6) + MULS16((o->oy - cy) >> 3, m7) +
             MULS16((o->oz - cz) >> 3, m8)) >> 14 < -(((s32)o->radius >> 3) + 16)) continue;
        if (ni >= MAXITEMS) break;
        it->o = o;
        if (o->type == OBJ_GROUND) it->key = 0x40000000L + ((s32)o->radius << 8);
        else if (o->type == OBJ_LINES) it->key = 0x40000000L - 1 - d;
        else it->key = d;
        it++; ni++;
    }
    return ni;
}

s32 view_world_begin(const Camera *c, World *w)
{
    Tile *tl[TILE_CACHE];
    int nt, i, ni = 0, j;
    s32 total = 0;
    s32 dmax = c->vis, cx = c->x >> 8, cy = c->y >> 8;
    Item t;
    vstats.objs = vstats.faces = vstats.lines = vstats.verts = 0;
    nt = world_tiles_near(w, cx, cy, dmax, tl, TILE_CACHE);
    vstats.tiles = nt;
    /* 1) objetos de suelo: primero los grandes (campos, lagos), luego pistas */
    for (i = 0; i < nt; i++) ni = gather_tile(tl[i], c, ni, cx, cy, dmax);
    /* orden descendente de clave: suelo (por tamano) y luego solidos de lejos a cerca */
    for (i = 1; i < ni; i++) {
        t = items[i]; j = i - 1;
        while (j >= 0 && items[j].key < t.key) { items[j + 1] = items[j]; j--; }
        items[j + 1] = t;
    }
    n_items = ni; next_item = 0;
    for (i = 0; i < ni; i++) total += (s32)items[i].o->nv + items[i].o->nf;
    return total;
}

/* Segunda mitad: dibuja objetos de la lista, en orden, hasta gastar `budget` de coste (siempre al menos uno). Devuelve 1 si ya
 * estan todos. */
int view_world_step(Surface *s, const Camera *c, s32 budget)
{
    s32 dmax = c->vis, spent = 0;
#ifdef OBJ_DUMP
    s32 cx = c->x >> 8, cy = c->y >> 8;
#endif
    while (next_item < n_items) {
        const Obj *o = items[next_item++].o;
        spent += (s32)o->nv + o->nf;
        if (xform_obj(c, o, dmax)) { vstats.objs++;
#ifdef OBJ_DUMP
            printf("OBJ type %d nv %d nf %d radius %d dist %ld lod %d flags %x\n", o->type, o->nv, o->nf, o->radius, (long)(MAX(ABS(o->ox - cx), ABS(o->oy - cy))), o->lod, o->flags);
#endif
            draw_obj(s, c, o); }
        if (spent >= budget) break;
    }
    return next_item >= n_items;
}

void view_world(Surface *s, const Camera *c, World *w)
{
    view_world_begin(c, w);
    view_world_step(s, c, 0x7FFFFFFFL);
}


/* ------------------------------------------------ avion en vista exterior */
void view_model(Surface *s, const Camera *c, const Model *mdl,
                s32 ax, s32 ay, s32 az, const s16 *am, int gear)
{
    s16 C[9], base16[3];
    s32 base[3], key[64];
    int i, j, n, order[64], nf, to;
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
    proj_all(vw, n, near_z, c->fx, c->fy, pxy, pfront);
    /* caras ordenadas por profundidad media (pintor), sin ocultacion */
    f = mdl->f; nf = MIN(mdl->nf - (gear ? 0 : mdl->ngear), 64);
    for (i = 0; i < nf; i++) {
        fp[i] = f;
        key[i] = face_key(f);
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
