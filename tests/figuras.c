/* figuras.c - capturas para las figuras del manual (tools/libro.sh): el menu inicial y la cabina del bimotor en Madrid.
 * Deja build/shots/fig_menu.scr y fig_cabina_bimotor.scr (make -f Makefile.pc capturas las pasa a PNG). */
#include "../src/sim.h"
#include "../src/platform.h"
#include <stdio.h>
#include <string.h>

int host_dump(const char *fname);
void host_key(int k, int down);

int main(void)
{
    char acf[64], scn[64];
    int hour = 12, i;
    host_key(KEY_ENTER, 1);                 /* el menu se dibuja una vez y sale al leer ENTER */
    sim_menu(acf, scn, &hour);
    host_key(KEY_ENTER, 0);
    host_dump("build/shots/fig_menu.scr");
    plat_path(acf, "bimotor.acf");
    plat_path(scn, "iberia.scn");
    if (sim_init(scn, acf, 12)) { printf("no carga\n"); return 1; }
    for (i = 0; i < PANEL_PARTS; i++) sim_render();
    host_dump("build/shots/fig_cabina_bimotor.scr");
    return 0;
}
