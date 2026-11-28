/* platform_host.c - plataforma de pruebas en PC: la pantalla es un buffer de
 * 32 KB con el mismo formato que la del QL; el programa de prueba controla
 * el reloj y las teclas, y vuelca capturas (tools/qlscr2png.py las pasa a PNG). */
#define _DEFAULT_SOURCE 1
#include "platform.h"
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
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
void plat_himem_init(void) {}
int plat_low_warn(void) { return 0; }
int plat_run(int (*fn)(void)) { return fn(); }
static void (*isr_fn)(void);
void plat_isr_set(void (*fn)(void)) { isr_fn = fn; }
void host_tick(void) { host_ticks++; if (isr_fn) isr_fn(); }     /* un turno de 50 Hz: contador e interrupcion (pruebas del planificador) */
void psg_write(int reg, int val) { psg_regs[reg & 15] = val; }

void plat_path(char *out, const char *name)
{
    int n = (int)strlen(name);
    strcpy(out, n > 4 && (!strcmp(name + n - 4, ".scn") || !strcmp(name + n - 4, ".sav")) ? "build/" : "data/");   /* escenarios compilados y partidas: build/ */
    strcat(out, name);
}

int plat_list(const char *ext, char names[][10], int max)      /* PC: la carpeta build/ */
{
    DIR *d = opendir("build");
    struct dirent *de;
    int n = 0, i, j, len, el = (int)strlen(ext);
    char t[10];
    if (!d) return 0;
    while (n < max && (de = readdir(d)) != NULL) {
        len = (int)strlen(de->d_name) - el - 1;
        if (len < 1 || len > 8 || de->d_name[len] != '.' || strcmp(de->d_name + len + 1, ext)) continue;
        for (i = 0; i < len; i++) names[n][i] = (char)toupper((unsigned char)de->d_name[i]);
        names[n][len] = 0;
        n++;
    }
    closedir(d);
    for (i = 1; i < n; i++)                                      /* orden alfabetico */
        for (j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            strcpy(t, names[j]); strcpy(names[j], names[j - 1]); strcpy(names[j - 1], t);
        }
    return n;
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
long plat_free_kb(void) { return 1024L; }
