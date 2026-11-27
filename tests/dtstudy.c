/* dtstudy.c - la fisica a otro paso de tiempo: mismas maniobras con PHYS_DT = 1/25 s (por omision) o 1/12,5 s (-DPHYS_DT=5243L -DPHYS_HZ=12).
 * Imprime "clave valor" para compararlas (doc/optimizacion.md, «Planificador por turnos»). Uso: build/dtstudy25 y build/dtstudy125 */
#include "../src/sim.h"
#include "../src/platform.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define SPS (65536.0 / PHYS_DT)                 /* pasos por segundo */
static void steps(double sec) { long n = (long)(sec * SPS + 0.5), i; for (i = 0; i < n; i++) flight_step(&sim.f, &sim.w); }
static double deg(s32 a) { return a / 65536.0 * 57.29578; }
static double kt(s32 v) { return v / 65536.0 * 1.9438; }

static void trace(const char *name, double sec, int what, double every)   /* what: 0 theta, 1 phi, 2 IAS, 3 vs fpm, 4 q (deg/s) */
{
    double t; double mx = 0, mn = 0; int k = 0;
    printf("%s_traza", name);
    for (t = 0; t < sec; t += every) {
        double v;
        steps(every);
        v = what == 0 ? deg(sim.f.the) : what == 1 ? deg(sim.f.phi) : what == 2 ? kt(sim.f.ias) : what == 3 ? sim.f.vs / 65536.0 * 196.85 : deg(sim.f.q);
        if (v > mx) mx = v; if (v < mn) mn = v;
        printf(" %.1f", v); k++;
    }
    printf("\n%s_max %.2f\n%s_min %.2f\n", name, mx, name, mn);
}

static void setup(const char *acf)
{
    char a[64];
    plat_path(a, acf);
    if (sim_init("build/iberia.scn", a, 12)) { printf("no carga\n"); exit(1); }
}

int main(void)
{
    long i, n; double d; s32 x0, y0;
    printf("paso %.4f s\n", PHYS_DT / 65536.0);
    setup("mono.acf");
    /* 1. despegue */
    sim.f.thr[0] = FX_ONE; x0 = sim.f.x; y0 = sim.f.y;
    for (i = 0; i < 25 * 60 * 2 && sim.f.on_ground; i++) {
        if (kt(sim.f.ias) > 55) sim.f.elev = FXF(30, 100);
        flight_step(&sim.f, &sim.w);
    }
    d = sqrt(pow((sim.f.x - x0) / 256.0, 2) + pow((sim.f.y - y0) / 256.0, 2));
    printf("despegue_s %.2f\ndespegue_m %.0f\n", i / SPS, d);
    sim.f.elev = FXF(10, 100); steps(40);
    printf("subida_ias %.1f\nsubida_alt %.0f\nsubida_vs %.0f\n", kt(sim.f.ias), sim.f.z / 256.0, sim.f.vs / 65536.0 * 196.85);
    /* 2. cabeceo: pulso de profundidad de 1 s y respuesta durante 12 s */
    sim.f.elev = 0; steps(10);
    sim.f.elev = FXF(25, 100); steps(1.0); sim.f.elev = 0;
    trace("pulso_prof_the", 12, 0, 0.5);
    /* 3. alabeo: alerones 1,5 s y vuelta a nivel */
    sim.f.ail = FXF(4, 10); steps(1.5); sim.f.ail = 0;
    trace("alabeo_phi", 10, 1, 0.5);
    sim.f.ail = -FXF(4, 10); steps(1.5); sim.f.ail = 0; steps(4);
    /* 4. perdida: gases fuera y tirar poco a poco */
    sim.f.thr[0] = 0; sim.f.phi = 0; sim.f.p = 0;
    for (n = 0, i = 0; i < 25 * 60 * 2 && !sim.f.stalled; i++) {
        sim.f.elev = MIN(sim.f.elev + (s32)(200 * 5 / SPS), FX_ONE);   /* igual rampa por segundo en los dos pasos */
        flight_step(&sim.f, &sim.w);
    }
    printf("perdida_s %.2f\nperdida_ias %.1f\n", i / SPS, kt(sim.f.ias));
    /* 5. aterrizaje desde final a 3 km */
    setup("mono.acf");
    {
        const Airport *ap = &sim.w.apt[0]; const Runway *rw = &ap->rwy[0];
        s32 sx = isin(rw->hdg), cx = icos(rw->hdg), dist = 3000;
        s32 x = rw->tx - (sx * dist >> 14), y = rw->ty - (cx * dist >> 14), h = (s32)((dist + 300) * 0.0524);
        flight_place(&sim.f, x, y, ap->elev + h, rw->hdg, (s32)(70 / 1.9438), 0);
        sim.f.flaps = 2; sim.f.flap_pos = 2 * FX_ONE / (sim.ac.n_flaps - 1); sim.f.gear_down = 1; sim.f.gear_pos = FX_ONE;
        sim.f.gam = -3 * FX_DEG; sim.f.alf = 3 * FX_DEG; sim.f.the = 0; sim.f.thr[0] = FXF(3, 10);
        for (i = 0; i < 25 * 200 * 2 && !sim.f.on_ground && !sim.f.crash; i++) flight_step(&sim.f, &sim.w);
        printf("aterrizaje_s %.2f\naterrizaje_vs_fpm %.0f\naterrizaje_crash %d\n", i / SPS, sim.f.vs / 65536.0 * 196.85, sim.f.crash);
    }
    /* 6. el caso mas duro para un Euler: reactor rapido (qn = 3) con una orden de profundidad brusca */
    setup("reactor.acf");
    { const Airport *ap = &sim.w.apt[0]; const Runway *rw = &ap->rwy[0];
      flight_place(&sim.f, rw->tx, rw->ty, ap->elev + 3000, rw->hdg, 130, 0); sim.f.thr[0] = sim.f.thr[1] = FXF(6, 10); }
    steps(5);
    for (i = 0; i < 12; i++) { sim.f.elev = (i & 1) ? -FXF(60, 100) : FXF(60, 100); steps(0.4); if (i == 0) printf("reactor_q0 %.2f\n", deg(sim.f.q)); }
    sim.f.elev = 0;
    trace("reactor_q", 6, 4, 0.5);
    printf("reactor_ias %.0f\nreactor_the %.1f\n", kt(sim.f.ias), deg(sim.f.the));
    printf("reactor_crash %d\n", sim.f.crash);
    return 0;
}
