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

#ifndef PROF_SCENE
#define PROF_SCENE 1
#endif
#ifndef PROF_PAUSE
#define PROF_PAUSE 0
#endif
#ifndef PROF_HOLD
#define PROF_HOLD 0
#endif
#ifndef PROF_N
#define PROF_N 25
#endif
#ifndef PROF_DUMPS
#define PROF_DUMPS 4
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
    if (PROF_PAUSE) sim.paused = 1;              /* fisica parada: escena identica a la del PC (tools/ql_shot.sh) */
    {                                            /* direcciones (byte 2 y 1) de un buffer estatico y de un malloc, para ver en que RAM caen */
        void *m = malloc(20000);
        PROF_MARK(((unsigned long)m >> 16) & 255); PROF_MARK(((unsigned long)m >> 8) & 255);
        free(m);
    }
    PROF_BASE(main);
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
