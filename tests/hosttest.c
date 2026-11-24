/* hosttest.c - pruebas en PC del motor: fisica (despegue, subida, perdida),
 * navegacion (ILS) y capturas de pantalla en formato QL para los mockups. */
#include "../src/sim.h"
#include "../src/platform.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern u32 host_ticks;
void host_key(int k, int down);
int host_dump(const char *fname);

static void frames(int n, int tpf)
{
    while (n--) { host_ticks += (u32)tpf; sim_frame(); }
}

static void shot(const char *name)
{
    char fn[96];
    int i;
    for (i = 0; i < PANEL_PARTS; i++) { host_ticks += 0; sim_render(); }
    sprintf(fn, "build/shots/%s.scr", name);
    host_dump(fn);
    printf("  captura %s  (obj %d caras %d lineas %d vert %d teselas %d)\n", name,
           vstats.objs, vstats.faces, vstats.lines, vstats.verts, vstats.tiles);
}

static double kt(s32 v) { return v / 65536.0 * 1.9438; }
static double m(s32 q8) { return q8 / 256.0; }

static void status(const char *t)
{
    Flight *f = &sim.f;
    printf("%-22s IAS %5.1f kt  ALT %6.1f m  AGL %6.1f  VS %6.0f fpm  alfa %5.1f  theta %5.1f  phi %5.1f  rumbo %5.1f  %s%s\n",
           t, kt(f->ias), m(f->z), f->agl / 65536.0, f->vs / 65536.0 * 196.85, f->alf / 1144.0,
           f->the / 1144.0, f->phi / 1144.0, f->psi / 1144.0,
           f->on_ground ? "SUELO " : "", f->stalled ? "PERDIDA" : "");
}

static void place_final(double dist, double kts, int flaps)
{
    const Airport *ap = &sim.w.apt[0];
    const Runway *rw = &ap->rwy[0];
    s32 sx = isin(rw->hdg), cx = icos(rw->hdg);
    s32 x = rw->tx - (sx * (s32)dist >> 14), y = rw->ty - (cx * (s32)dist >> 14);
    s32 h = (s32)((dist + 300) * 0.0524);          /* senda de 3 grados */
    flight_place(&sim.f, x, y, ap->elev + h, rw->hdg, (s32)(kts / 1.9438), 0);
    sim.f.flaps = flaps; sim.f.flap_pos = flaps * FX_ONE / (sim.ac.n_flaps - 1);
    sim.f.gear_down = 1; sim.f.gear_pos = FX_ONE;
    sim.f.gam = -3 * FX_DEG; sim.f.alf = 3 * FX_DEG; sim.f.the = 0;
    sim.f.elev = 0; sim.f.ail = 0; sim.f.p = 0; sim.f.q = 0;
}

int main(void)
{
    int i;
    s32 x0, y0;
    char acf[64];
    double dist;

    printf("== MONOMOTOR ==\n");
    plat_path(acf, "mono.acf");
    if (sim_init("build/demo.scn", acf, 12)) { printf("no carga\n"); return 1; }
    printf("escenario '%s': %d navaids, %d aeropuertos\n", sim.w.name, sim.w.nnav, sim.w.napt);
    frames(8, 4);
    status("en cabecera");
    shot("01_cabina_pista_dia");

    /* despegue: gases a fondo, rotacion a 55 kt */
    sim.f.thr[0] = FX_ONE;
    x0 = sim.f.x; y0 = sim.f.y;
    for (i = 0; i < 25 * 60 && sim.f.on_ground; i++) {
        if (kt(sim.f.ias) > 55) sim.f.elev = FXF(30, 100);
        flight_step(&sim.f, &sim.w);
    }
    dist = isqrt32((u32)(((sim.f.x - x0) >> 8) * ((sim.f.x - x0) >> 8) + ((sim.f.y - y0) >> 8) * ((sim.f.y - y0) >> 8)));
    printf("despegue: %.1f s, %.0f m de carrera, %s\n", i / 25.0, dist, sim.f.crash ? "CRASH" : "ok");
    status("despegue");
    sim.f.elev = FXF(10, 100);
    for (i = 0; i < 25 * 20; i++) flight_step(&sim.f, &sim.w);
    status("subida 20 s");
    for (i = 0; i < 25 * 30; i++) flight_step(&sim.f, &sim.w);
    status("subida 50 s");
    shot("02_cabina_subida");
    /* viraje a la derecha 25 grados */
    sim.f.ail = FXF(4, 10);
    for (i = 0; i < 12; i++) flight_step(&sim.f, &sim.w);
    sim.f.ail = 0;
    for (i = 0; i < 25 * 4; i++) flight_step(&sim.f, &sim.w);
    status("viraje");
    shot("03_cabina_viraje");
    sim.view_mode = VIEW_CHASE; shot("04_exterior_persecucion");
#ifdef HOST_DEBUG
    { void view_debug(const Camera *c, World *w); view_debug(&sim.cam, &sim.w); }
#endif
    sim.view_mode = VIEW_SPOT; shot("05_exterior_lateral");
    sim.view_mode = VIEW_TOWER; shot("06_exterior_torre");
    sim.view_mode = VIEW_COCKPIT;

    /* crucero nivelado y perdida: gases al minimo y tirar poco a poco */
    sim.f.ail = 0; sim.f.phi = 0; sim.f.p = 0;
    sim.f.thr[0] = 0;
    for (i = 0; i < 25 * 40 && !sim.f.stalled; i++) {
        sim.f.elev = MIN(sim.f.elev + 200, FX_ONE);
        flight_step(&sim.f, &sim.w);
    }
    status("entrada en perdida");
    sim.f.elev = -FXF(3, 10); sim.f.thr[0] = FX_ONE;
    for (i = 0; i < 25 * 6; i++) flight_step(&sim.f, &sim.w);
    status("recuperacion");

    /* aproximacion ILS de noche */
    sim.clock = 22 * 3600UL;
    place_final(3200, 70, 2);
    sim.zoom = 512;
    sim.f.thr[0] = FXF(45, 100);
    sim.r.nav1 = 11030; sim.r.com = 11830;
    for (i = 0; i < 25 * 2; i++) flight_step(&sim.f, &sim.w);
    nav_update(&sim.r, &sim.w, &sim.f);
    status("final ILS noche");
    printf("  ILS: loc %d  cdi %d  gs_valid %d gs %d  dme %.1f NM  baliza %d\n", sim.r.n1_loc, sim.r.cdi1,
           sim.r.gs_valid, sim.r.gs, sim.r.dme10 / 10.0, sim.r.marker);
    atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 1);
    for (i = 0; i < 70; i++) atc_scroll(&sim.atc);
    printf("  ATC: %s\n", sim.atc.text);
    shot("07_ils_noche");
    sim.zoom = 256;
    flight_place(&sim.f, 16000, 9000, 800, 45500U, 55, 0);
    sim.f.flaps = 0; sim.f.flap_pos = 0; sim.f.elev = 0;
    flight_step(&sim.f, &sim.w);
    shot("07b_noche_ciudad");

    /* atardecer, cerca del VOR, mirando a la sierra */
    sim.clock = 20 * 3600UL;
    flight_place(&sim.f, 6000, -9000, 900, 15000, 55, 0);
    sim.f.flaps = 0; sim.f.flap_pos = 0; sim.f.thr[0] = FXF(70, 100); sim.f.elev = 0;
    sim.r.nav1 = 11320; sim.r.obs1 = 16384;
    flight_step(&sim.f, &sim.w);
    nav_update(&sim.r, &sim.w, &sim.f);
    shot("08_atardecer_sierra");

    /* ciudad de dia */
    sim.clock = 11 * 3600UL;
    flight_place(&sim.f, 5200, 3200, 420, 8192, 55, 0);
    sim.f.ail = 0; sim.f.p = 0; sim.f.phi = 10 * FX_DEG;
    flight_step(&sim.f, &sim.w);
    shot("09_ciudad_dia");

    /* reactor */
    printf("== REACTOR ==\n");
    world_close(&sim.w);
    plat_path(acf, "reactor.acf");
    if (sim_init("build/demo.scn", acf, 16)) { printf("no carga\n"); return 1; }
    sim.f.thr[0] = sim.f.thr[1] = FX_ONE; sim.f.flaps = 1; sim.f.flap_pos = FX_ONE / 2;
    x0 = sim.f.x; y0 = sim.f.y;
    for (i = 0; i < 25 * 90 && sim.f.on_ground; i++) {
        if (kt(sim.f.ias) > 110) sim.f.elev = FXF(35, 100);
        flight_step(&sim.f, &sim.w);
    }
    dist = isqrt32((u32)(((sim.f.x - x0) >> 8) * ((sim.f.x - x0) >> 8) + ((sim.f.y - y0) >> 8) * ((sim.f.y - y0) >> 8)));
    printf("despegue: %.1f s, %.0f m, N1 %.0f%%, empuje %ld N\n", i / 25.0, dist, sim.f.e[0].n1 / 655.36, (long)sim.f.e[0].thrust);
    sim.f.gear_down = 0; sim.f.flaps = 0; sim.f.elev = FXF(8, 100);
    for (i = 0; i < 25 * 30; i++) flight_step(&sim.f, &sim.w);
    status("subida 30 s");
    sim.view_mode = VIEW_CHASE;
    sim.f.ail = FXF(3, 10);
    for (i = 0; i < 10; i++) flight_step(&sim.f, &sim.w);
    sim.f.ail = 0;
    shot("10_reactor_persecucion");

    /* bimotor: fallo de un motor */
    printf("== BIMOTOR ==\n");
    world_close(&sim.w);
    plat_path(acf, "bimotor.acf");
    if (sim_init("build/demo.scn", acf, 12)) { printf("no carga\n"); return 1; }
    sim.f.thr[0] = sim.f.thr[1] = FX_ONE;
    for (i = 0; i < 25 * 60 && sim.f.on_ground; i++) {
        if (kt(sim.f.ias) > 85) sim.f.elev = FXF(30, 100);
        flight_step(&sim.f, &sim.w);
    }
    status("despegue bimotor");
    sim.f.gear_down = 0; sim.f.elev = FXF(5, 100);
    for (i = 0; i < 25 * 20; i++) flight_step(&sim.f, &sim.w);
    status("subida");
    sim.f.mix[1] = 0;       /* motor derecho cortado */
    for (i = 0; i < 25 * 10; i++) flight_step(&sim.f, &sim.w);
    status("motor dcho parado");
    printf("  empuje izq %ld N dcho %ld N\n", (long)sim.f.e[0].thrust, (long)sim.f.e[1].thrust);
    sim.view_mode = VIEW_SPOT;
    shot("11_bimotor_fallo_motor");

    /* planeador */
    printf("== PLANEADOR ==\n");
    world_close(&sim.w);
    plat_path(acf, "planeador.acf");
    if (sim_init("build/demo.scn", acf, 12)) { printf("no carga\n"); return 1; }
    flight_place(&sim.f, -2000, -2000, 1400, 8192, 26, 0);
    for (i = 0; i < 25 * 60; i++) flight_step(&sim.f, &sim.w);
    status("planeo 60 s");
    {
        s32 xa = sim.f.x, ya = sim.f.y, za = sim.f.z;
        double dh, dz;
        for (i = 0; i < 25 * 240; i++) flight_step(&sim.f, &sim.w);
        dh = sqrt(((sim.f.x - xa) / 256.0) * ((sim.f.x - xa) / 256.0) + ((sim.f.y - ya) / 256.0) * ((sim.f.y - ya) / 256.0));
        dz = (za - sim.f.z) / 256.0;
        status("planeo 300 s");
        printf("  fineza media (4 min): %.1f  (%.0f m recorridos, %.0f m perdidos)\n", dh / dz, dh, dz);
    }
    sim.view_mode = VIEW_SPOT;
    shot("12_planeador");
    return 0;
}
