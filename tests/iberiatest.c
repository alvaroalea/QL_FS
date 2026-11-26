/* iberiatest.c - pruebas del escenario Peninsula Iberica: cada aeropuerto
 * (sin agua bajo la pista, ILS en final, ATC), un despegue, y capturas. */
#include "../src/sim.h"
#include "../src/platform.h"
#include <stdio.h>
#include <string.h>

extern u32 host_ticks;
int host_dump(const char *fname);

static void shot(const char *name)
{
    char fn[96];
    int i;
    for (i = 0; i < PANEL_PARTS; i++) sim_render();
    sprintf(fn, "build/shots/%s.scr", name);
    host_dump(fn);
    printf("  captura %-26s obj %3d caras %3d lineas %3d vert %4d teselas %2d\n", name,
           vstats.objs, vstats.faces, vstats.lines, vstats.verts, vstats.tiles);
}

static int find_apt(const char *id)
{
    int i;
    for (i = 0; i < sim.w.napt; i++) if (!strncmp(sim.w.apt[i].ident, id, 4)) return i;
    return 0;
}

/* avion en final de la cabecera 0 a 'dist' m, senda de 3 grados */
static void final_at(int a, s32 dist, s32 kts)
{
    const Airport *ap = &sim.w.apt[a];
    const Runway *rw = &ap->rwy[0];
    s32 sx = isin(rw->hdg), cx = icos(rw->hdg);
    sim.start_apt = a;
    sim_reset();
    flight_place(&sim.f, rw->tx - (sx * dist >> 14), rw->ty - (cx * dist >> 14),
                 ap->elev + (dist + 300) * 524 / 10000, rw->hdg, kts * 10000 / 19438, 0);
    sim.f.flaps = 2; sim.f.flap_pos = FX_ONE * 2 / 3; sim.f.thr[0] = sim.f.thr[1] = FXF(45, 100);
    sim.f.gam = -3 * FX_DEG; sim.f.alf = 3 * FX_DEG; sim.f.the = 0; sim.f.elev = 0;
}

int main(void)
{
    char acf[64], scn[64];
    int i, a, k, bad = 0;
    plat_path(acf, "mono.acf");
    strcpy(scn, "build/iberia.scn");
    if (sim_init(scn, acf, 12)) { printf("no carga\n"); return 1; }
    printf("escenario %s: %d aeropuertos, %d radioayudas, rejilla %dx%d, visibilidad %d km\n",
           sim.w.name, sim.w.napt, sim.w.nnav, sim.w.ntx, sim.w.nty, sim.w.vis_km);

    printf("%-5s %-20s %-6s %-7s %-6s %s\n", "OACI", "nombre", "suelo", "LOC/GS", "DME", "frecuencias");
    for (a = 0; a < sim.w.napt; a++) {
        const Airport *ap = &sim.w.apt[a];
        int ok_ground = 1;
        char f1[8], f2[8];
        sim.start_apt = a;
        sim_reset();
        sim.f.thr[0] = sim.f.thr[1] = FXF(3, 10);
        for (k = 0; k < 25 * 4; k++) flight_step(&sim.f, &sim.w);   /* rodar 4 s por la pista */
        if (sim.f.crash) ok_ground = 0;
        final_at(a, 6000, 80);
        for (i = 0; i < 25 * 2; i++) flight_step(&sim.f, &sim.w);
        nav_update(&sim.r, &sim.w, &sim.f);
        sim.r.nav1 = 0;
        {   /* sintonizar el ILS de la cabecera 0: el mas cercano con tipo ILS */
            int j; s32 best = 0x7FFFFFFFL;
            for (j = 0; j < sim.w.nnav; j++) {
                const Nav *n = &sim.w.nav[j];
                /* el localizador de la cabecera 0 esta a len+300 m de su umbral (con pistas paralelas, el mas cercano al avion puede ser otro) */
                s32 lx = ap->rwy[0].tx + (isin(ap->rwy[0].hdg) * ((s32)ap->rwy[0].len + 300) >> 14);
                s32 ly = ap->rwy[0].ty + (icos(ap->rwy[0].hdg) * ((s32)ap->rwy[0].len + 300) >> 14);
                s32 dx = (n->x - lx) >> 8, dy = (n->y - ly) >> 8, d = dx * dx + dy * dy;
                if (n->type == NAV_ILS && d < best) { best = d; sim.r.nav1 = n->freq; }
            }
        }
        nav_update(&sim.r, &sim.w, &sim.f);
        freq_str(ap->atis, f1); freq_str(ap->twr, f2);
        printf("%-5s %-20s %-6s %d/%d %4d %5.1f  ATIS %s TWR %s\n", ap->ident, ap->name,
               ok_ground ? "tierra" : "AGUA!", sim.r.n1_loc, sim.r.gs_valid, sim.r.cdi1,
               sim.r.dme10 / 10.0, f1, f2);
        if (!ok_ground || !sim.r.gs_valid) bad++;
    }
    printf("aeropuertos con problemas: %d\n", bad);

    /* capturas */
    a = find_apt("LEMD"); sim.start_apt = a; sim_reset(); sim.clock = 12 * 3600UL;
    shot("ib01_madrid_cabecera");
    sim.f.thr[0] = FX_ONE;
    for (i = 0; i < 25 * 60 && sim.f.on_ground; i++) { if (sim.f.ias > FXI(28)) sim.f.elev = FXF(35, 100); flight_step(&sim.f, &sim.w); }
    sim.f.elev = FXF(8, 100);
    for (i = 0; i < 25 * 60; i++) flight_step(&sim.f, &sim.w);
    printf("despegue en Madrid: alt %.0f m, %s\n", sim.f.z / 256.0, sim.f.crash ? "CRASH" : "ok");
    sim.view_mode = VIEW_CHASE; shot("ib02_madrid_subida_ext");
    sim.view_mode = VIEW_COCKPIT;

    final_at(find_apt("LEBL"), 5000, 80); shot("ib03_barcelona_final");
    final_at(find_apt("LEMG"), 9000, 80); sim.f.z += 400 << 8; shot("ib04_malaga_final");
    final_at(find_apt("GCTS"), 7000, 80); sim.view_mode = VIEW_SPOT; shot("ib05_tenerife_sur");
    sim.view_mode = VIEW_COCKPIT;
    /* Teide desde el aire, mirando al noroeste */
    final_at(find_apt("GCTS"), 3000, 90);
    sim.f.psi = 300 * FX_DEG; sim.f.z += 900 << 8; shot("ib06_teide");
    final_at(find_apt("LEPA"), 6000, 80); shot("ib07_palma_final");
    final_at(find_apt("LPPT"), 6000, 80); sim.clock = 20 * 3600UL; shot("ib08_lisboa_atardecer");
    final_at(find_apt("LEMD"), 9000, 90); sim.clock = 23 * 3600UL; shot("ib09_madrid_noche");
    /* Pirineos desde Zaragoza hacia el norte */
    sim.clock = 12 * 3600UL;
    final_at(find_apt("LEBL"), 6000, 100);
    {
        const Nav *z = NULL; int j;
        for (j = 0; j < sim.w.nnav; j++) if (!strncmp(sim.w.nav[j].ident, "ZZA", 3)) z = &sim.w.nav[j];
        if (z) flight_place(&sim.f, z->x + 20000, z->y + 30000, 1800, 0, 60, 0);
    }
    sim.f.elev = 0; sim.f.flaps = 0; sim.f.flap_pos = 0;
    shot("ib10_pirineos");
    final_at(find_apt("LEMD"), 6000, 90); sim.f.psi = 200 * FX_DEG; sim.f.z += 300 << 8;
    sim.view_mode = VIEW_CHASE; shot("ib11_madrid_torres");
    return bad != 0;
}
