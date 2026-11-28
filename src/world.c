/* world.c - carga del escenario y cache de teselas */
#include "world.h"
#include <stdlib.h>
#include <string.h>

static u16 rd16(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static u32 rd32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static int little_endian(void)
{
    u16 t = 1;
    return *(u8 *)&t == 1;
}

static int rdblock(FILE *fp, u32 off, void *buf, u32 n)
{
    if (fseek(fp, (long)off, SEEK_SET) != 0) return 0;
    return fread(buf, 1, (size_t)n, fp) == (size_t)n;
}

int world_open(World *w, const char *fname)
{
    u8 h[64], *b;
    u32 off_tdir, off_nav, off_apt, n;
    int i, k;

    memset(w, 0, sizeof(*w));
    w->fp = fopen(fname, "rb");
    if (!w->fp) return 0;
    if (fread(h, 1, 64, w->fp) != 64 || memcmp(h, "QFS1", 4) != 0) {
        fclose(w->fp); w->fp = NULL; return 0;
    }
    w->tile_log2 = rd16(h + 6);
    w->org_x = (s32)rd32(h + 8);
    w->org_y = (s32)rd32(h + 12);
    w->ntx = rd16(h + 16);
    w->nty = rd16(h + 18);
    w->nnav = rd16(h + 20);
    w->napt = rd16(h + 22);
    off_tdir = rd32(h + 24);
    off_nav = rd32(h + 28);
    off_apt = rd32(h + 32);
    w->elev = (s16)rd16(h + 36);
    w->vis_km = rd16(h + 38);
    memcpy(w->name, h + 40, 24); w->name[24] = 0;
    for (i = 23; i >= 0 && (w->name[i] == ' ' || !w->name[i]); i--) w->name[i] = 0;

    n = (u32)w->ntx * w->nty;
    b = (u8 *)malloc((size_t)(n * 8));
    w->tdir_off = (u32 *)malloc((size_t)(n * 4));
    w->tdir_size = (u16 *)malloc((size_t)(n * 2));
    w->tdir_elev = (s16 *)malloc((size_t)(n * 2));
    if (!b || !w->tdir_off || !w->tdir_size || !w->tdir_elev) return 0;
    if (!rdblock(w->fp, off_tdir, b, n * 8)) return 0;
    for (i = 0; i < (int)n; i++) {
        w->tdir_off[i] = rd32(b + i * 8);
        w->tdir_size[i] = rd16(b + i * 8 + 4);
        w->tdir_elev[i] = (s16)rd16(b + i * 8 + 6);
    }
    free(b);

    w->nav = (Nav *)malloc(sizeof(Nav) * (w->nnav + 1));
    b = (u8 *)malloc((size_t)(w->nnav * 24 + 1));
    if (!w->nav || !b || !rdblock(w->fp, off_nav, b, (u32)w->nnav * 24)) return 0;
    for (i = 0; i < w->nnav; i++) {
        u8 *p = b + i * 24;
        Nav *v = &w->nav[i];
        v->type = p[0];
        v->freq = rd16(p + 2);
        v->x = (s32)rd32(p + 4);
        v->y = (s32)rd32(p + 8);
        v->z = (s16)rd16(p + 12);
        v->course = rd16(p + 14);
        v->range = rd16(p + 16);
        v->gs = rd16(p + 18);
        memcpy(v->ident, p + 20, 4); v->ident[4] = 0;
    }
    free(b);

    w->apt = (Airport *)malloc(sizeof(Airport) * (w->napt + 1));
    b = (u8 *)malloc((size_t)(w->napt * 120 + 1));
    if (!w->apt || !b || !rdblock(w->fp, off_apt, b, (u32)w->napt * 120)) return 0;
    for (i = 0; i < w->napt; i++) {
        u8 *p = b + i * 120;
        Airport *a = &w->apt[i];
        u32 to; u16 tl;
        memcpy(a->ident, p, 4); a->ident[4] = 0;
        memcpy(a->name, p + 4, 28); a->name[28] = 0;
        for (k = 27; k >= 0 && (a->name[k] == ' ' || !a->name[k]); k--) a->name[k] = 0;
        a->x = (s32)rd32(p + 32);
        a->y = (s32)rd32(p + 36);
        a->elev = (s16)rd16(p + 40);
        a->atis = rd16(p + 42);
        a->twr = rd16(p + 44);
        a->nrwy = p[46];
        a->plat_dx = (s16)((s8)p[47] * 8);
        a->plat_dy = (s16)((s8)p[118] * 8);
        a->plat_hdg = (u16)((u16)p[119] << 8);
        a->has_plat = (u8)(p[119] != 0);
        for (k = 0; k < 4; k++) {
            u8 *r = p + 48 + k * 16;
            a->rwy[k].hdg = rd16(r);
            a->rwy[k].len = rd16(r + 2);
            a->rwy[k].wid = rd16(r + 4);
            a->rwy[k].tx = (s32)rd32(r + 8);
            a->rwy[k].ty = (s32)rd32(r + 12);
        }
        to = rd32(p + 112); tl = rd16(p + 116);
        a->atis_text = (char *)malloc((size_t)tl + 1);
        if (a->atis_text) {
            if (!tl || !rdblock(w->fp, to, a->atis_text, tl)) tl = 0;
            a->atis_text[tl] = 0;
        }
    }
    free(b);
    for (i = 0; i < TILE_CACHE; i++) { w->cache[i].tx = -1; w->cache[i].mem = NULL; }
    w->slot = (s8 *)malloc((size_t)w->ntx * w->nty + 1);
    if (!w->slot) return 0;
    memset(w->slot, 0xFF, (size_t)w->ntx * w->nty);
    w->gen = 1; w->hl_gen = 0; w->hl = NULL; w->nhl = w->maxhl = 0; w->hit_near = 0;
    return 1;
}

void world_close(World *w)
{
    int i;
    for (i = 0; i < TILE_CACHE; i++) {
        if (w->cache[i].mem) free(w->cache[i].mem);
        if (w->cache[i].obj) free(w->cache[i].obj);
    }
    for (i = 0; i < w->napt; i++) if (w->apt[i].atis_text) free(w->apt[i].atis_text);
    free(w->hl); w->hl = NULL;
    free(w->slot); w->slot = NULL;
    free(w->apt); free(w->nav); free(w->tdir_off); free(w->tdir_size); free(w->tdir_elev);
    if (w->fp) fclose(w->fp);
    w->fp = NULL;
}

/* convierte un blob de tesela en objetos. El blob sigue en memoria: los
 * vertices y caras apuntan dentro de el (sin copiar). */
static int tile_parse(Tile *t, u8 *blob, u32 size)
{
    u8 *p = blob, *end = blob + size;
    int i, k, swap = little_endian();
    u16 n = rd16(p);
    p += 2;
    t->nobj = n;
    t->obj = (Obj *)malloc(sizeof(Obj) * (n + 1));
    if (!t->obj) return 0;
    for (i = 0; i < n && p + 22 <= end; i++) {
        Obj *o = &t->obj[i];
        o->type = p[0]; o->flags = p[1]; o->lod = p[2]; o->vshift = p[3];
        o->ox = (s32)rd32(p + 4);
        o->oy = (s32)rd32(p + 8);
        o->oz = (s16)rd16(p + 12);
        o->radius = rd16(p + 14);
        o->nv = p[16]; o->nf = p[17];
        o->nvf = p[20]; o->nff = p[21];
        o->fbytes = rd16(p + 18);
        p += 22;
        o->reach = ((s32)o->lod << 8) * (o->type == OBJ_SOLID && o->lod < 128 ? SOLID_LOD : 8) / 8;
        o->reach += o->radius;
        o->v = (s16 *)p;
        if (swap)
            for (k = 0; k < o->nv * 3; k++) o->v[k] = (s16)rd16(p + k * 2);
        o->flat = 1;                                     /* ¿todos los vertices a la misma z? */
        for (k = 1; k < o->nv; k++) if (o->v[k * 3 + 2] != o->v[2]) { o->flat = 0; break; }
        {   /* radio de vision: el de arriba solo cuenta la planta, y un edificio alto se descartaba entero al salir su base del campo */
            u32 mx = 0, q;
            s32 r;
            for (k = 0; k < o->nv; k++) {
                q = (u32)MULS16(o->v[k * 3], o->v[k * 3]) + (u32)MULS16(o->v[k * 3 + 1], o->v[k * 3 + 1]) + (u32)MULS16(o->v[k * 3 + 2], o->v[k * 3 + 2]);
                if (q > mx) mx = q;
            }
            r = (((s32)isqrt32(mx) << o->vshift) >> 3) + 1;
            o->vrad = (u16)MIN(65535L, MAX(r, (s32)o->radius));
        }
        p += o->nv * 6;
        o->f = p;
        p += (o->fbytes + 1) & ~1;
    }
    t->nobj = (u16)i;
    return 1;
}

static int tile_load(World *w, Tile *t, int tx, int ty)
{
    int idx = ty * w->ntx + tx;
    u32 sz = w->tdir_size[idx];
    u8 *blob;
    if (t->tx >= 0) w->slot[t->ty * w->ntx + t->tx] = -1;
    if (t->mem) { free(t->mem); t->mem = NULL; }
    if (t->obj) { free(t->obj); t->obj = NULL; }
    t->tx = -1; t->nobj = 0;
    blob = (u8 *)malloc((size_t)sz + 2);
    if (!blob) return 0;
    if (!rdblock(w->fp, w->tdir_off[idx], blob, sz)) { free(blob); return 0; }
    t->mem = blob;
    w->bytes_loaded += sz;
    if (!tile_parse(t, blob, sz)) return 0;
    t->tx = (s16)tx; t->ty = (s16)ty;
    w->gen++;
    return 1;
}

static Tile *tile_get(World *w, int tx, int ty)
{
    int i, best = -1;
    u32 old = 0xFFFFFFFFUL;
    i = w->slot[ty * w->ntx + tx];
    if (i >= 0) { w->cache[i].stamp = w->clock; return &w->cache[i]; }
    for (i = 0; i < TILE_CACHE; i++) {
        Tile *t = &w->cache[i];
        if (t->tx < 0) { best = i; break; }
        if (t->stamp != w->clock && t->stamp < old) { old = t->stamp; best = i; }
    }
    if (best < 0) return NULL;
    if (!tile_load(w, &w->cache[best], tx, ty)) return NULL;
    w->cache[best].stamp = w->clock;
    w->slot[ty * w->ntx + tx] = (s8)best;
    return &w->cache[best];
}

int world_tiles_near(World *w, s32 x, s32 y, s32 radius, Tile **out, int max)
{
    int tx0, tx1, ty0, ty1, tx, ty, n = 0;
    Tile *t;
    w->clock++;
    tx0 = (int)((x - radius - w->org_x) >> w->tile_log2);
    tx1 = (int)((x + radius - w->org_x) >> w->tile_log2);
    ty0 = (int)((y - radius - w->org_y) >> w->tile_log2);
    ty1 = (int)((y + radius - w->org_y) >> w->tile_log2);
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 >= w->ntx) tx1 = w->ntx - 1;
    if (ty1 >= w->nty) ty1 = w->nty - 1;
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++) {
            if (!w->tdir_size[ty * w->ntx + tx]) continue;
            if (n >= max) return n;
            t = tile_get(w, tx, ty);
            if (t) out[n++] = t;
        }
    return n;
}

s16 world_ground(World *w, s32 x, s32 y)
{
    int tx = (int)((x - w->org_x) >> w->tile_log2);
    int ty = (int)((y - w->org_y) >> w->tile_log2);
    if (tx < 0 || ty < 0 || tx >= w->ntx || ty >= w->nty) return w->elev;
    return w->tdir_elev[MULS16(ty, w->ntx) + tx];
}

const Airport *world_nearest_airport(World *w, s32 x, s32 y, s32 *dist)
{
    int i;
    s32 best = 0x7FFFFFFFL, d, dx, dy;
    const Airport *r = NULL;
    for (i = 0; i < w->napt; i++) {
        dx = ABS(w->apt[i].x - x) >> 4; dy = ABS(w->apt[i].y - y) >> 4;
        d = isqrt32((u32)(dx * dx + dy * dy)) << 4;
        if (d < best) { best = d; r = &w->apt[i]; }
    }
    if (dist) *dist = best;
    return r;
}

/* punto dentro de poligono 2D (vertices del objeto, primera cara) */
static int inside_face(const Obj *o, s32 px, s32 py)
{
    const u8 *f = o->f;
    int n = f[0], i, j, c = 0;
    s32 xi, yi, xj, yj;
    for (i = 0, j = n - 1; i < n; j = i++) {
        xi = ((s32)o->v[f[2 + i] * 3] << o->vshift) >> 3;
        yi = ((s32)o->v[f[2 + i] * 3 + 1] << o->vshift) >> 3;
        xj = ((s32)o->v[f[2 + j] * 3] << o->vshift) >> 3;
        yj = ((s32)o->v[f[2 + j] * 3 + 1] << o->vshift) >> 3;
        if (((yi > py) != (yj > py)) &&
            (px < xi + (xj - xi) * (py - yi) / (yj - yi + (yj == yi))))
            c = !c;
    }
    return c;
}

/* lista compacta de objetos de colision de la cache (se rehace solo cuando cambian las teselas) */
static void hit_list(World *w)
{
    int i, k, n = 0;
    for (i = 0; i < TILE_CACHE; i++) {
        Tile *t = &w->cache[i];
        if (t->tx < 0) continue;
        for (k = 0; k < t->nobj; k++) if (t->obj[k].flags & (OF_HIT | OF_WATER)) n++;
    }
    if (n > w->maxhl) {
        free(w->hl);
        w->hl = (Obj **)malloc(sizeof(Obj *) * (n + 16));
        w->maxhl = w->hl ? n + 16 : 0;
    }
    w->nhl = 0;
    if (!w->hl) return;
    for (i = 0; i < TILE_CACHE; i++) {
        Tile *t = &w->cache[i];
        if (t->tx < 0) continue;
        for (k = 0; k < t->nobj; k++) if (t->obj[k].flags & (OF_HIT | OF_WATER)) w->hl[w->nhl++] = &t->obj[k];
    }
}

int world_hit(World *w, s32 x, s32 y, s32 z)
{
    int i, res = 0;
    s32 dx, dy, top, near = 0x7FFFFFFFL, dd;
    Obj **pp;
    if (w->hl_gen != w->gen || !w->hl) { hit_list(w); w->hl_gen = w->gen; }
    for (pp = w->hl, i = w->nhl; i > 0; i--, pp++) {
        Obj *o = *pp;
        dx = x - o->ox; dy = y - o->oy;
        dd = MAX(ABS(dx), ABS(dy)) - (s32)o->radius;
        if (dd > 0) { if (dd < near) near = dd; continue; }
        near = 0;
        if (o->flags & OF_WATER) {
            if (z <= o->oz + 1 && inside_face(o, dx, dy)) { res = 3; break; }
            continue;
        }
        /* cilindro: 70% del radio, altura del vertice mas alto */
        if (MULS16(dx >> 2, dx >> 2) + MULS16(dy >> 2, dy >> 2) >
            MULS16(o->radius >> 2, o->radius >> 2) / 2) continue;
        {
            int j; s16 mz = 0;
            for (j = 0; j < o->nv; j++) if (o->v[j * 3 + 2] > mz) mz = o->v[j * 3 + 2];
            top = o->oz + (((s32)mz << o->vshift) >> 3);
        }
        if (o->flags & OF_MOUNTAIN) {
            /* cono: la altura baja con la distancia al centro */
            s32 r = isqrt32((u32)(dx * dx + dy * dy));
            top = o->oz + (top - o->oz) * (o->radius - r) / (o->radius + 1);
            if (z < top) { res = 2; break; }
        } else if (z < top) { res = 1; break; }
    }
    w->hit_near = near;
    return res;
}
