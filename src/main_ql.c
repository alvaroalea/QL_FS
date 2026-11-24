/* main_ql.c - punto de entrada en el QL.
 *   EXEC_W flp1_qfs   (el dispositivo de datos va en la variable D$ de SuperBASIC; sin
 *   Toolkit II no se puede pasar texto tras EXEC_W: cuelga el sistema)
 */
#include "sim.h"
#include "platform.h"
#include "sound.h"
#include "gfx.h"
#include <string.h>
#include "qlvars.h"
#include "qfs_prof.h"

long _stack = 32768L;                  /* c68: pila de la tarea */
char _prog_name[] = "QFS";

extern char plat_dev[8];
#ifdef QFS_PROF
extern void prof_run(void);
#endif

int main(int argc, char **argv)
{
    char scn[48], acf[48];
    int hour, r;
    Surface s;
    char v[16];

    if (ql_basic_str("D", v, sizeof v) && v[0] && strlen(v) < 7) strcpy(plat_dev, v);
    plat_init();
    snd_init();
#ifdef QFS_PROF
    prof_run();                        /* escena fija y perfil (tools/ql_prof.sh) */
    plat_exit();
    return 0;
#endif
    for (;;) {
        if (!sim_menu(acf, scn, &hour)) break;
        r = sim_init(scn, acf, hour);
        if (r) {
            gfx_init_surface(&s, plat_screen(), SCR_H);
            gfx_text(&s, 150, 200, r == -1 ? "NO SE PUEDE ABRIR EL ESCENARIO" : "NO SE PUEDE LEER EL AVION", C_RED);
            { u32 t = plat_ticks(); while (plat_ticks() - t < 150) ; }
            continue;
        }
        while (!sim.quit) sim_frame();
        world_close(&sim.w);
        snd_off();
    }
    snd_off();
    plat_exit();
    return 0;
}
