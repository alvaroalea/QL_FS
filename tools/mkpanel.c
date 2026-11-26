/* mkpanel.c - genera el fondo del panel de cada avion: volcado crudo de los 32 KB de pantalla (modo 4 del QL).
 *   build/mkpanel data/mono.acf [data/bimotor.acf ...]   ->   data/mono.src ...
 * Se compila con Makefile.pc (make -f Makefile.pc data/mono.src) */
#include <stdio.h>
#include <string.h>
#include "panel.h"
#include "platform.h"
#include "aircraft.h"

int host_dump(const char *fname);

int main(int argc, char **argv)
{
    static AircraftDef ac;
    char out[256];
    int i, n, bad = 0;
    for (i = 1; i < argc; i++) {
        n = (int)strlen(argv[i]);
        if (n < 4 || strcmp(argv[i] + n - 4, ".acf") || !aircraft_load(&ac, argv[i])) {
            fprintf(stderr, "mkpanel: no se puede leer %s\n", argv[i]);
            bad = 1;
            continue;
        }
        memset(plat_screen(), 0, SCR_BYTES);
        panel_build(plat_screen(), &ac);
        strcpy(out, argv[i]);
        strcpy(out + n - 4, ".src");
        {
            FILE *fp = fopen(out, "wb");
            if (!fp || fwrite(plat_screen() + (long)PANEL_SRC_Y0 * SCR_STRIDE, 1, (size_t)PANEL_SRC_BYTES, fp) != (size_t)PANEL_SRC_BYTES) {
                fprintf(stderr, "mkpanel: no se puede escribir %s\n", out); bad = 1;
            } else printf("%s -> %s (%ld bytes)\n", argv[i], out, PANEL_SRC_BYTES);
            if (fp) fclose(fp);
        }
    }
    return bad;
}
