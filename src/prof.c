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

void prof_run(void)
{
    char scn[48], acf[48];
    int i, k;
    const Airport *ap;
    const Runway *rw;
    s32 sx, cx;

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
