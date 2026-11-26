/* platform_host.c - plataforma de pruebas en PC: la pantalla es un buffer de
 * 32 KB con el mismo formato que la del QL; el programa de prueba controla
 * el reloj y las teclas, y vuelca capturas (tools/qlscr2png.py las pasa a PNG). */
#include "platform.h"
#include <stdlib.h>
#include "sound.h"
#include <stdio.h>
#include <string.h>

static u8 screen[SCR_BYTES];
u32 host_ticks;
u8  host_rows[8];
int psg_regs[16];

void plat_init(void) {}
void plat_exit(void) {}
u8 *plat_screen(void) { return screen; }
u32 plat_ticks(void) { return host_ticks; }
int plat_keyrow(int row) { return host_rows[row & 7]; }
void plat_frame(void) {}
void psg_write(int reg, int val) { psg_regs[reg & 15] = val; }

void plat_path(char *out, const char *name)
{
    int n = (int)strlen(name);
    strcpy(out, n > 4 && !strcmp(name + n - 4, ".scn") ? "build/" : "data/");      /* los escenarios compilados estan en build/ */
    strcat(out, name);
}

void host_key(int k, int down)
{
    if (down) host_rows[k >> 3] |= (u8)(1 << (k & 7));
    else host_rows[k >> 3] &= (u8)~(1 << (k & 7));
}

int host_dump(const char *fname)
{
    FILE *f = fopen(fname, "wb");
    if (!f) return 0;
    fwrite(screen, 1, SCR_BYTES, f);
    fclose(f);
    return 1;
}

void *plat_alloc_hi(long n) { return malloc((size_t)n); }
