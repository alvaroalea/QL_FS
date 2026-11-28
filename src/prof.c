/* prof.c - escenas fijas para medir el rendimiento en el QL (solo con -DQFS_PROF; ver tools/ql_prof.sh).
 * Escena (PROF_SCENE): 0 cabina en cabecera (parado), 1 cabina en final a 5 km, 2 persecucion en final,
 * 3 cabina en final de noche, 4 persecucion sobre la ciudad (reactor, 600 m).
 * Cada PROF_N cuadros se vuelca el perfil; al terminar vuelve a SuperBASIC. */
#ifdef QFS_PROF
#include "sim.h"
#include "platform.h"
#include "sound.h"
#include "qfs_prof.h"
#include <stdlib.h>
#include <string.h>

#ifndef PROF_SCENE
#define PROF_SCENE 1
#endif
#ifndef PROF_PAUSE
#define PROF_PAUSE 0
#endif
#ifndef PROF_HOLD
#define PROF_HOLD 0
#endif
#ifndef PROF_WIRE
#define PROF_WIRE 0
#endif
#ifndef PROF_N
#define PROF_N 25
#endif
#ifndef PROF_DUMPS
#define PROF_DUMPS 4
#endif

#ifdef PROF_ALT
long prof_alt_redo[2];
#endif
extern int main(int argc, char **argv);

#ifdef FXDIV_TEST          /* compara fxdiv (ensamblador) con la version C de referencia: marca 1 = discrepancias (bajo, alto), 2 = pares probados */
static s32 fxdiv_ref(s32 a, s32 b)
{
    int s = 16, neg = 0;
    if (a < 0) { a = -a; neg = 1; }
    if (b < 0) { b = -b; neg ^= 1; }
    while (s > 0 && a < 0x20000000L) { a <<= 1; s--; }
    b >>= s;
    if (b == 0) return neg ? -0x7FFFFFFFL : 0x7FFFFFFFL;
    a = a / b;
    return neg ? -a : a;
}
static void fxdiv_test(void)
{
    u32 r = 777; long i, bad = 0, n = 6000;
    for (i = 0; i < n; i++) {
        s32 a, b;
        int sa, sb;
        r = r * 1103515245UL + 12345UL; a = (s32)(r >> 1);
        r = r * 1103515245UL + 12345UL; b = (s32)(r >> 1);
        r = r * 1103515245UL + 12345UL; sa = (int)((r >> 16) % 31); sb = (int)((r >> 8) % 31);
        a >>= sa; b >>= sb;
        if (i & 1) a = -a;
        if (i & 2) b = -b;
        if (i < 64) { a = (i & 4) ? 0 : a; b = (i & 8) ? 0 : b; }
        if (fxdiv(a, b) != fxdiv_ref(a, b)) bad++;
    }
    PROF_MARK(1); PROF_MARK((int)(bad & 255)); PROF_MARK((int)(bad >> 8));
    PROF_MARK(2); PROF_MARK((int)((n >> 8) & 255));
}
#endif

#ifdef ASM_TEST     /* compara las rutinas en ensamblador (lerp_div, proj_pt, fxmul, gfx_hspan, gfx_line) con referencias en C: marcas 100+n, errores (bajo, alto) */
#include "gfx.h"
extern int proj_all(const s32 *v, int n, s32 near_z, int fx, int fy, s32 *out, u8 *front);
extern void proj_pt(const s32 *v, int fx, int fy, s32 *out);
static u32 rs = 4242;
static u32 rr(void) { rs = rs * 1103515245UL + 12345UL; return rs >> 8; }
static s32 rv(int bits)                       /* valor aleatorio de hasta `bits` bits, con signo */
{
    s32 v = (s32)(((rr() << 12) ^ rr()) & ((1UL << bits) - 1));
    return (rr() & 1) ? -v : v;
}
static u32 ref_mulq(u32 ad, u32 an, u32 aden)
{
    u32 al = ad & 0xFFFFUL, ah = ad >> 16, bl = an & 0xFFFFUL, bh = an >> 16;
    u32 ll = al * bl, hl = ah * bl, lh = al * bh, hh = ah * bh, mid = hl + lh, hi, lo, r, q;
    int i, c = mid < hl;
    hi = hh + (mid >> 16) + ((u32)c << 16);
    lo = ll + (mid << 16);
    if (lo < ll) hi++;
    if (hi >= aden) return 0x7FFFFFFFUL;
    r = hi; q = lo;
    for (i = 0; i < 32; i++) { c = (int)((q >> 31) & 1); q <<= 1; r = (r << 1) | (u32)c; if (r >= aden) { r -= aden; q |= 1; } }
    return q;
}
static s32 ref_lerp(s32 a, s32 b, s32 num, s32 den)
{
    s32 d = b - a; int neg = 0; u32 q;
    if (den == 0) return a;
    if (d < 0) { d = -d; neg ^= 1; } if (num < 0) { num = -num; neg ^= 1; } if (den < 0) { den = -den; neg ^= 1; }
    q = ref_mulq((u32)d, (u32)num, (u32)den);
    return a + (neg ? -(s32)q : (s32)q);
}
static s32 ref_fxmul(s32 a, s32 b)
{
    int neg = 0; u32 ua, ub, ah, al, bh, bl;
    if (a < 0) { a = -a; neg = 1; } if (b < 0) { b = -b; neg ^= 1; }
    ua = (u32)a; ub = (u32)b; ah = ua >> 16; al = ua & 0xFFFFUL; bh = ub >> 16; bl = ub & 0xFFFFUL;
    { u32 r = ((ah * bh) << 16) + ah * bl + al * bh + ((al * bl) >> 16); return neg ? -(s32)r : (s32)r; }
}
static void ref_proj(const s32 *v, int fx, int fy, s32 *out)
{
    s32 x = v[0], y = v[1], z = v[2];
    while (z > 32767L || x > 32767L || x < -32768L || y > 32767L || y < -32768L) { x >>= 1; y >>= 1; z >>= 1; }
    if (z == 0) z = 1;
    out[0] = 256 + (x * fx) / z; out[1] = 48 - (y * fy) / z;
}
static void rep(int id, long bad, long n)
{
    PROF_MARK(100 + id); PROF_MARK((int)(bad & 255)); PROF_MARK((int)((bad >> 8) & 255)); PROF_MARK((int)((n >> 8) & 255));
}
extern s32 sdiv32(s32 a, s32 b);
extern void draw_lrow(Surface *s, const s32 *p0, const s32 *p1, int cnt, int shift, s32 near_z, int fx, int fy, int col, int dbl);
static void asm_test(void)
{
    long i, bad;
    s32 a, b, c, d, o1[2], o2[2], v[3];
    static u8 b1[128 * 40], b2[128 * 40];
    Surface s1, s2;
    bad = 0;
    for (i = 0; i < 1000; i++) {                  /* lerp_div: operandos de distinto tamano, el cociente cabe (|num| <= |den|) */
        a = rv(1 + (int)(rr() % 28)); b = rv(1 + (int)(rr() % 28)); d = rv(1 + (int)(rr() % 28)); if (d == 0) d = 1;
        c = (d >> 10) * (s32)(rr() & 1023);                 /* |num| <= |den| */
        if (ref_lerp(a, b, c, d) != lerp_div(a, b, c, d)) bad++;
    }
    for (i = 0; i < 200; i++) {                   /* y casos sin restriccion (desbordes incluidos) */
        a = rv(30); b = rv(30); c = rv(30); d = rv(30);
        if (ref_lerp(a, b, c, d) != lerp_div(a, b, c, d)) bad++;
    }
    rep(0, bad, 1200);
    bad = 0;
    for (i = 0; i < 1000; i++) {                  /* proj_pt */
        int fx = 100 + (int)(rr() % 2600), fy = 100 + (int)(rr() % 2000);
        v[0] = rv(1 + (int)(rr() % 27)); v[1] = rv(1 + (int)(rr() % 27)); v[2] = 1 + (rv(1 + (int)(rr() % 27)) & 0x7FFFFFFL);
        proj_pt(v, fx, fy, o1); ref_proj(v, fx, fy, o2);
        if (o1[0] != o2[0] || o1[1] != o2[1]) bad++;
    }
    rep(1, bad, 1000);
    bad = 0;
    for (i = 0; i < 2000; i++) {                  /* fxmul */
        a = rv(1 + (int)(rr() % 31)); b = rv(1 + (int)(rr() % 31));
        if (i < 40) { a = (i & 1) ? 0 : a; b = (i & 2) ? 0 : b; }
        if (fxmul(a, b) != ref_fxmul(a, b)) bad++;
    }
    rep(2, bad, 2000);
    bad = 0;
    gfx_init_surface(&s1, b1, 40); gfx_init_surface(&s2, b2, 40);
    gfx_clip(&s1, 3, 2, 470, 37); gfx_clip(&s2, 3, 2, 470, 37);
    for (i = 0; i < 400; i++) {                   /* gfx_hspan (ensamblador) contra gfx_hspan_raw con el recorte hecho aqui */
        int y = (int)(rr() % 44) - 2, x0 = (int)(rr() % 520) - 20, x1 = (int)(rr() % 520) - 20, t;
        Fill f = (Fill)(rr() << 16) ^ (Fill)rr();
        long k;
        gfx_hspan(&s1, y, x0, x1, f);
        if (y >= 2 && y <= 37) {
            if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
            if (x0 < 3) x0 = 3;
            if (x1 > 470) x1 = 470;
            if (x0 <= x1) {                       /* referencia bit a bit (sin tablas ni palabras largas) */
                u8 g = (u8)(((y & 1) ? (f & 0xFFFFUL) : (f >> 16)) >> 8), r = (u8)((y & 1) ? (f & 0xFFFFUL) : (f >> 16));
                for (t = x0; t <= x1; t++) {
                    u8 *p = b2 + y * 128 + ((t >> 3) << 1), bit = (u8)(0x80 >> (t & 7));
                    p[0] = (u8)((p[0] & ~bit) | (g & bit)); p[1] = (u8)((p[1] & ~bit) | (r & bit));
                }
            }
        }
        for (k = 0; k < 128L * 40; k++) if (b1[k] != b2[k]) { bad++; b2[k] = b1[k]; }
    }
    rep(3, bad, 400);
    bad = 0;
    for (i = 0; i < 300; i++) {                   /* gfx_line (casos triviales en ensamblador) contra el recorte general */
        s32 x0 = rr() % 560 - 40, y0 = rr() % 60 - 10, x1 = rr() % 560 - 40, y1 = rr() % 60 - 10;
        int col = (int)(rr() & 3);
        long k;
        gfx_line(&s1, x0, y0, x1, y1, col);
        gfx_line_c(&s2, x0, y0, x1, y1, col);
        for (k = 0; k < 128L * 40; k++) if (b1[k] != b2[k]) { bad++; b2[k] = b1[k]; }
    }
    rep(4, bad, 300);
    bad = 0;
    for (i = 0; i < 1000; i++) {                  /* sdiv32 contra la division de la biblioteca */
        a = rv(1 + (int)(rr() % 31)); b = rv(1 + (int)(rr() % 31)); if (b == 0) b = 1;
        if (sdiv32(a, b) != a / b) bad++;
    }
    rep(5, bad, 1000);
    bad = 0;
    gfx_init_surface(&s1, b1, 40); gfx_clip(&s1, 3, 2, 470, 37);
    for (i = 0; i < 120; i++) {                   /* gfx_poly32 (recorte y relleno en ensamblador) contra la definicion: pixel claramente dentro = pintado */
        s32 p[16], cx0 = rv(12) % 700 + 240, cy0 = rv(12) % 150 + 20, rx = 40 + (rr() % 9000), ry = 40 + (rr() % 5000);
        int n = 3 + (int)(rr() % 5), j, x, y, ok = 1;
        long area = 0;
        static const s16 cs[8] = { 16384, 11585, 0, -11585, -16384, -11585, 0, 11585 };   /* seno y coseno en Q14 de 45 grados en 45 */
        n = 4 + (int)(rr() & 1) * 4;               /* cuadrado u octogono: siempre convexos */
        for (j = 0; j < n; j++) {
            int k = j * (8 / n);
            p[2 * j] = cx0 + (s32)(((long)rx * cs[(k + 2) & 7]) >> 14);
            p[2 * j + 1] = cy0 + (s32)(((long)ry * cs[k & 7]) >> 14);
        }
        area = 0;
        for (j = 0; j < n; j++) { int j2 = (j + 1) % n; area += (long)p[2 * j] * p[2 * j2 + 1] - (long)p[2 * j2] * p[2 * j + 1]; }
        memset(b1, 0, sizeof b1);
        gfx_poly32(&s1, n, p, fill_tab[F_WHITE]);
        for (y = 2; y <= 37 && ok; y += 5) for (x = 3; x <= 470 && ok; x += 19) {
            s32 mn = 0x7FFFFFFFL;
            int set = (b1[y * 128 + (x >> 3) * 2] >> (7 - (x & 7))) & 1;
            for (j = 0; j < n; j++) {
                int j2 = (j + 1) % n;
                s32 ex = p[2 * j2] - p[2 * j], ey = p[2 * j2 + 1] - p[2 * j + 1], len, d;
                len = isqrt32((u32)(ex * ex + ey * ey)); if (len == 0) continue;
                d = ((ex * (y - p[2 * j + 1]) - ey * (x - p[2 * j])) / len);
                if (area < 0) d = -d;
                if (d < mn) mn = d;
            }
            if (mn > 3 && !set) ok = 0;            
            if (mn < -3 && set) ok = 0;
        }
        if (!ok) bad++;
    }
    rep(6, bad, 120);
    bad = 0;
    gfx_init_surface(&s1, b1, 40); gfx_init_surface(&s2, b2, 40);
    gfx_clip(&s1, 0, 0, 479, 39); gfx_clip(&s2, 0, 0, 479, 39);
    for (i = 0; i < 300; i++) {                   /* draw_lrow (ensamblador) contra la referencia en C (la misma aritmetica que view3d.c) */
        s32 p0[3], p1[3], x, y, z, dx = 0, dy = 0, dz = 0, t[3], o[2], nz = 1 + (s32)(rr() % 20);
        int cnt = 1 + (int)(rr() % 60), shift = (int)(rr() % 4), dbl = (int)(rr() & 1), col = (int)(rr() & 3), fx = 100 + (int)(rr() % 700), fy = 100 + (int)(rr() % 500), k, n;
        long q;
        for (k = 0; k < 3; k++) { p0[k] = rv(14) + (k == 2 ? 6000 : 0); p1[k] = rv(14) + (k == 2 ? 6000 : 0); }
        if (i & 4) { p0[2] = rv(5); p1[2] = rv(5) + 40; }          /* cruzan el plano cercano */
        if (i & 8) { p0[0] = rv(16); p1[1] = rv(16); }            /* vertices grandes */
        memset(b1, 0, sizeof b1); memset(b2, 0, sizeof b2);
        draw_lrow(&s1, p0, p1, cnt, shift, nz, fx, fy, col, dbl);
        n = (cnt + (1 << shift) - 1) >> shift;
        if (!(p0[2] < nz && p1[2] < nz)) {
            x = (s32)((u32)p0[0] << 12); y = (s32)((u32)p0[1] << 12); z = (s32)((u32)p0[2] << 12);
            if (cnt > 1) {
                dx = sdiv32((s32)((u32)(p1[0] - p0[0]) << 12), cnt - 1); dy = sdiv32((s32)((u32)(p1[1] - p0[1]) << 12), cnt - 1);
                dz = sdiv32((s32)((u32)(p1[2] - p0[2]) << 12), cnt - 1);
                dx = (s32)((u32)dx << shift); dy = (s32)((u32)dy << shift); dz = (s32)((u32)dz << shift);
            }
            for (k = 0; k < n; k++) {
                t[2] = z >> 12;
                if (t[2] >= nz) {
                    t[0] = x >> 12; t[1] = y >> 12; ref_proj(t, fx, fy, o);
                    gfx_pixel(&s2, (int)o[0], (int)o[1], col);
                    if (dbl) gfx_pixel(&s2, (int)o[0] + 1, (int)o[1], col);
                }
                x = (s32)((u32)x + (u32)dx); y = (s32)((u32)y + (u32)dy); z = (s32)((u32)z + (u32)dz);
            }
        }
        for (q = 0; q < 128L * 40; q++) if (b1[q] != b2[q]) bad++;
    }
    rep(7, bad, 300);
    bad = 0;
    for (i = 0; i < 600; i++) {                   /* gfx_line_raw (bucle especializado por color y sentido) contra la definicion en C */
        s32 x0 = rr() % 470, y0 = rr() % 38, x1 = rr() % 470, y1 = rr() % 38, dx = x1 - x0, dy = y1 - y0, sx = 1, sy = 128, err, e2, n;
        int col = (int)(i & 3);
        long q;
        u8 *p, bit;
        if (i & 4) { x1 = x0; dx = 0; dy = y1 - y0; }          /* verticales */
        if (i & 8) { y1 = y0; dy = 0; dx = x1 - x0; }          /* horizontales */
        memset(b1, 0x5A, sizeof b1); memset(b2, 0x5A, sizeof b2);
        gfx_line_raw(b1, x0, y0, x1, y1, col);
        if (dx < 0) { dx = -dx; sx = -1; }
        if (dy < 0) { dy = -dy; sy = -128; }
        p = b2 + y0 * 128 + ((x0 >> 3) << 1); bit = (u8)(0x80 >> (x0 & 7)); err = dx - dy; n = (dx > dy ? dx : dy) + 1;
        while (n--) {
            if (col & 2) p[0] |= bit; else p[0] &= (u8)~bit;
            if (col & 1) p[1] |= bit; else p[1] &= (u8)~bit;
            e2 = err << 1;
            if (e2 > -dy) { err -= dy; if (sx > 0) { bit >>= 1; if (!bit) { bit = 0x80; p += 2; } } else { bit <<= 1; if (!bit) { bit = 0x01; p -= 2; } } }
            if (e2 < dx) { err += dx; p += sy; }
        }
        for (q = 0; q < 128L * 40; q++) if (b1[q] != b2[q]) bad++;
    }
    rep(8, bad, 600);
}
#endif

#ifdef PROF_ADDR           /* donde ha puesto QDOS el codigo, los datos, la pila y el monton (marcas 250, id, 0, 4 bytes de la direccion) */
static int addr_glob;
static void addr_rep(int id, unsigned long a)
{
    int b;
    PROF_MARK(250); PROF_MARK(id); PROF_MARK(0);
    for (b = 24; b >= 0; b -= 8) PROF_MARK((int)((a >> b) & 255));
}
#endif

void prof_run(void)
{
    char scn[48], acf[48];
    int i, k;
    const Airport *ap;
    const Runway *rw;
    s32 sx, cx;

#ifdef FXDIV_TEST
    fxdiv_test(); PROF_DUMP(98); return;
#endif
#ifdef ASM_TEST
    asm_test(); PROF_DUMP(98); return;
#endif
#ifdef PROF_ADDR
    {
        int loc;
        void *m1 = malloc(1000), *m2 = malloc(20000), *h1 = plat_alloc_hi(1000), *h2;
        addr_rep(1, (unsigned long)&prof_run); addr_rep(2, (unsigned long)&addr_glob); addr_rep(3, (unsigned long)&loc);
        addr_rep(4, (unsigned long)m1); addr_rep(5, (unsigned long)m2); addr_rep(6, (unsigned long)h1);
        h2 = plat_alloc_hi(70000L); addr_rep(7, (unsigned long)h2);
        { extern long plat_himem_used; addr_rep(10, (unsigned long)plat_himem_used); }
        addr_rep(8, (unsigned long)&sim); addr_rep(9, (unsigned long)&fill_tab);
        PROF_DUMP(97);
        return;
    }
#endif
    plat_path(acf, PROF_SCENE == 4 ? "reactor.acf" : "mono.acf");
    plat_path(scn, "iberia.scn");
    if (sim_init(scn, acf, PROF_SCENE == 3 ? 22 : 12)) return;
    ap = &sim.w.apt[0];
    rw = &ap->rwy[0];
    sx = isin(rw->hdg); cx = icos(rw->hdg);
    if (PROF_SCENE >= 1) {                      /* en final de LEMD 32L, a 5 km y en senda de 3 grados */
        s32 d = PROF_SCENE == 4 ? 12000 : 5000;
        s32 h = PROF_SCENE == 4 ? 600 : d * 524 / 10000;
        flight_place(&sim.f, rw->tx - (sx * d >> 14), rw->ty - (cx * d >> 14), ap->elev + h, rw->hdg,
                     PROF_SCENE == 4 ? 90 : 35, 0);
        sim.f.gam = -3 * FX_DEG;
        if (PROF_SCENE != 4) sim.f.thr[0] = FXF(3, 10);
        else sim.f.thr[0] = sim.f.thr[1] = FXF(6, 10);
    }
    if (PROF_SCENE == 2 || PROF_SCENE == 4) sim.view_mode = VIEW_CHASE;
#ifdef PROF_HOUR
    sim.clock = PROF_HOUR * 3600UL;               /* hora de la prueba (estrellas: 22) */
#endif
#ifdef PROF_LOOKP
    sim.lookp = PROF_LOOKP;
#endif
    if (PROF_PAUSE) sim.paused = 1;              /* fisica parada: escena identica a la del PC (tools/ql_shot.sh) */
    {                                            /* direcciones (byte 2 y 1) de un buffer estatico y de un malloc, para ver en que RAM caen */
        void *m = malloc(20000);
        PROF_MARK(((unsigned long)m >> 16) & 255); PROF_MARK(((unsigned long)m >> 8) & 255);
        free(m);
    }
#ifdef PROF_MEM                                  /* memoria libre del heap tras calentar: bloques de 1 KB hasta que malloc falle (se liberan) */
    {
        void *blk[700]; int nb = 0, b, kb;
        for (b = 0; b < 400; b++) sim_frame();
        while (nb < 700 && (blk[nb] = malloc(1024L)) != NULL) nb++;
        kb = nb;
        for (b = 0; b < nb; b++) free(blk[b]);
        PROF_MARK(230); PROF_MARK(kb >> 8); PROF_MARK(kb & 255);
        {   void *big = malloc(65536L); PROF_MARK(231); PROF_MARK(big != NULL); if (big) free(big); }
        {   int i, nt = 0; long by = 0;
            for (i = 0; i < sim.w.ncache; i++) if (sim.w.cache[i].tx >= 0) {
                nt++; by += sim.w.tdir_size[sim.w.cache[i].ty * sim.w.ntx + sim.w.cache[i].tx] + 2 + (long)sizeof(Obj) * (sim.w.cache[i].nobj + 1); }
            PROF_MARK(236); PROF_MARK(sim.w.ncache & 255); PROF_MARK((sim.w.ncache >> 8) & 255); PROF_MARK(237); PROF_MARK((int)(sim.w.mem_budget >> 10) & 255); PROF_MARK((int)(sim.w.mem_budget >> 18) & 255);
            PROF_MARK(232); PROF_MARK(nt); PROF_MARK(233); PROF_MARK((int)(by >> 8) & 255); PROF_MARK((int)(by >> 16) & 255);
            { u32 mx = 0, sm = 0; int t2; for (t2 = 0; t2 < sim.w.ntx * sim.w.nty; t2++) { sm += sim.w.tdir_size[t2]; if (sim.w.tdir_size[t2] > mx) mx = sim.w.tdir_size[t2]; }
              PROF_MARK(234); PROF_MARK((int)(mx >> 8) & 255); PROF_MARK((int)(mx >> 16) & 255); PROF_MARK(235); PROF_MARK((int)(sm >> 8) & 255); PROF_MARK((int)(sm >> 16) & 255); } }
        PROF_DUMP(98);
        return;
    }
#endif
    PROF_BASE(main);
#ifdef PROF_SCHED_RUN                           /* el simulador entero con el planificador de interrupciones (-DUSE_SCHED): PROF_SCHED_RUN turnos de 20 ms */
    {
        u32 t0, k;
        extern void sim_stop(void);
        extern int sch_st_y, sch_st_e, sch_st_rec, sch_st_miss, sch_st_fast;
        int w;
        sim_vjob_wire = PROF_WIRE;
        for (w = 0; w < 400; w++) sim_frame();   /* calienta la cache de teselas */
        sim.fps_v = 0; sim.frames = 0; sim.fps_p = 0; sch_st_y = sch_st_e = sch_st_rec = sch_st_miss = sch_st_fast = 0;
        PROF_DUMP(99);
        k = sim.nsteps;
        t0 = plat_ticks();
        PROF_MARK(240);
        while (plat_ticks() - t0 < PROF_SCHED_RUN) sim_frame();
        PROF_MARK(241);
        sim_stop();
        { long v[10]; int j, b; v[0] = (long)(plat_ticks() - t0); v[1] = (long)(sim.nsteps - k); v[2] = sim.fps_v; v[3] = sim.frames;
          v[4] = sim.fps_p; v[5] = sch_st_y; v[6] = sch_st_e; v[7] = sch_st_rec; v[8] = sch_st_miss; v[9] = sch_st_fast;
          for (j = 0; j < 10; j++) { PROF_MARK(250); PROF_MARK(203 + j); PROF_MARK(0); for (b = 24; b >= 0; b -= 8) PROF_MARK((int)((v[j] >> b) & 255)); } }
        PROF_DUMP(97);
        return;
    }
#endif
#ifdef BENCH_MODE
    {                                           /* estudio de interrupciones (src/bench.c): primero se calienta la cache de teselas */
        extern void bench_run(void);
        int w;
        for (w = 0; w < 6; w++) sim_frame();
        bench_run();
        world_close(&sim.w);
        snd_off();
        return;
    }
#endif
#ifdef PROF_LROW                                /* solo draw_lrow: 200 filas de 1, 2 y 21 luces (shift 0) y 21 luces con shift 2, en cuatro volcados */
    {
        static s32 pa[3] = { -3000L, 500L, 8000L }, pb[3] = { 3000L, 500L, 9000L };
        extern void draw_lrow(Surface *s, const s32 *p0, const s32 *p1, int cnt, int shift, s32 near_z, int fx, int fy, int col, int dbl);
        Surface sf;
        gfx_init_surface(&sf, sim.scr, 96);
        PROF_DUMP(99);
        for (k = 0; k < 4; k++) {
            for (i = 0; i < 200; i++) draw_lrow(&sf, pa, pb, k == 0 ? 1 : k == 1 ? 2 : 21, k == 3 ? 2 : 0, 1, 320, 300, 3, 0);
            PROF_DUMP(k);
        }
        return;
    }
#endif
#ifdef PROF_ALT                                 /* solo el altimetro: PROF_ALT repintados con la altitud subiendo 10 pies cada vez (z en Q8 m) */
    {
        PanelInfo pi;
        memset(&pi, 0, sizeof pi); pi.status = "";
        panel_piece(sim.scr, &sim.f, &sim.r, &sim.atc, &pi, 1);
        PROF_DUMP(99);
        for (k = 0; k < 3; k++) {                /* rondas desde 500 pies (agujas fuera del texto), 3800 (la de 10000 pies cruza el texto), 25000 */
            sim.f.z = (k == 0 ? 500L : k == 1 ? 3800L : 25000L) * 78L;
            for (i = 0; i < PROF_ALT; i++) { sim.f.z += 780; panel_piece(sim.scr, &sim.f, &sim.r, &sim.atc, &pi, 1); }
            PROF_MARK(200 + k); PROF_MARK((int)prof_alt_redo[0] & 255); PROF_MARK((int)prof_alt_redo[1] & 255);
            PROF_DUMP(k);
        }
        return;
    }
#endif
    PROF_DUMP(99);                              /* descarta la carga */
    for (k = 0; k < PROF_DUMPS; k++) {
        for (i = 0; i < PROF_N; i++) sim_frame();
        PROF_DUMP(k);
    }
    if (PROF_HOLD) for (;;) ;                  /* se queda con la pantalla puesta (captura desde el emulador) */
    world_close(&sim.w);
    snd_off();
}
#endif
