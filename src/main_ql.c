/* main_ql.c - punto de entrada en el QL.
 *   EX flp1_qfs ; "flp1_"     (el argumento opcional es el dispositivo de datos)
 */
#include "sim.h"
#include "platform.h"
#include "sound.h"
#include "gfx.h"
#include <string.h>

long _stack = 32768L;                  /* c68: pila de la tarea */
char _prog_name[] = "QFS";

extern char plat_dev[8];

int main(int argc, char **argv)
{
    char scn[48], acf[48];
    int hour, r;
    Surface s;

    if (argc > 1 && strlen(argv[1]) < 7) strcpy(plat_dev, argv[1]);
    plat_init();
    snd_init();
    for (;;) {
        if (!sim_menu(acf, &hour)) break;
        plat_path(scn, "demo.scn");
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
