/* hittest.c - la comprobacion espaciada de colisiones (HIT_SKIP_MAX) debe dar el mismo choque, en el mismo paso, que la de
 * cada paso (compilar con -DHIT_SKIP_MAX=0 para la referencia). Vuela contra edificios/montanas a varias velocidades. */
#include "../src/sim.h"
#include "../src/platform.h"
#include <stdio.h>

int main(void)
{
    int a, n = 0, t, i, k, v, nobj = 0;
    Tile *tl[TILE_CACHE];
    static Obj *cand[40];
    char acf[64];
    plat_path(acf, "mono.acf");
    if (sim_init("build/iberia.scn", acf, 12)) { printf("no carga\n"); return 1; }
    a = 0;
    for (t = 0; t < sim.w.napt && nobj < 40; t++) {
        const Airport *ap = &sim.w.apt[t];
        int nt = world_tiles_near(&sim.w, ap->x, ap->y, 6000, tl, TILE_CACHE);
        for (i = 0; i < nt && nobj < 40; i++)
            for (k = 0; k < tl[i]->nobj && nobj < 40; k += 7)
                if ((tl[i]->obj[k].flags & (OF_HIT | OF_WATER)) && tl[i]->obj[k].radius > 15) {
                    cand[nobj++] = &tl[i]->obj[k];
                }
        if (nobj >= 40) break;
    }
    for (i = 0; i < nobj; i++) {
        Obj *o = cand[i];
        for (v = 0; v < 3; v++) {
            s32 spd = v == 0 ? 30 : v == 1 ? 90 : 250;
            s32 sx = 3000, sy = 0;
            world_tiles_near(&sim.w, o->ox - sx, o->oy - sy, 500, tl, TILE_CACHE);
            world_tiles_near(&sim.w, o->ox, o->oy, 500, tl, TILE_CACHE);
            flight_place(&sim.f, o->ox - sx, o->oy - sy, o->oz + 3, 16384, spd, 0);   /* rumbo este (BAM 90 grados) */
            sim.f.gam = 0; sim.f.on_ground = 0;
            for (k = 0; k < 25 * 120 && !sim.f.crash; k++) {
                if ((k & 15) == 0) world_tiles_near(&sim.w, sim.f.x >> 8, sim.f.y >> 8, 4000, tl, TILE_CACHE);
                sim.f.z = (o->oz + 3) << 8; sim.f.gam = 0;
                flight_step(&sim.f, &sim.w);
            }
            printf("%d v%d: crash %d paso %d\n", i, (int)spd, sim.f.crash, k);
            n++;
        }
    }
    return 0;
}
