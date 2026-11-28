/* platform_ql.c - Sinclair QL real (QDOS / SMSQ), compilado con qcc (XTC68) */
#include "platform.h"
#include "sound.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>

extern volatile long plat_tickcnt;     /* ql68k.s, incrementado a 50 Hz */
void plat_mode4(void);
void plat_timer_on(void);
void plat_timer_off(void);

extern void (*plat_isr_fn)(void);      /* qdos68k.s: tm_poll la llama con todos los registros salvados y la pila plat_isr_sp */
extern void *plat_isr_sp;
static long isr_stack[2048];           /* 8 KB para la pila de la interrupcion */
void plat_isr_set(void (*fn)(void))
{
    plat_isr_sp = (void *)(isr_stack + 2048);
    plat_isr_fn = fn;
}

char plat_dev[8] = "flp1_";            /* dispositivo de datos (argumento 1) */

/* QSound: PIA 6821 en BASE+$2000. Protocolo de 2 fases confirmado en la
 * tarjeta QSound 2+ (registro -> $0F -> $0A ; dato -> $0E -> $0A).
 * Para otras tarjetas basta con cambiar esta funcion. */
volatile u8 *psg_pia = (volatile u8 *)0xC2000L;

void psg_write(int reg, int val)
{
    volatile u8 *p = psg_pia;
    if (!p) return;
    p[0] = (u8)reg;  p[2] = 0x0F; p[2] = 0x0A;
    p[0] = (u8)val;  p[2] = 0x0E; p[2] = 0x0A;
}

/* ---- memoria alta ($40000 en adelante, sin contencion) para todo lo que se usa a menudo (doc/optimizacion.md, «Memoria alta»)
 * En el QL el programa y sus datos globales los pone QDOS arriba, pero la pila de la tarea (c68: _stack) y todo lo que da malloc salen del
 * monton comun por el principio, que es la RAM interna del ZX8301: cada acceso espera al video. plat_himem_init llena ese hueco bajo con
 * bloques que se quedan reservados (tapon) y desde entonces malloc solo da memoria alta; la pila se cambia a un bloque alto. Los bloques
 * del tapon se devuelven al salir. Sin RAM de expansion (el primer malloc tras el tapon sale bajo o falla) no se hace nada. */
#define LOW_END 0x40000UL
#define PLUG_MAX 96
static void *plug[PLUG_MAX];
static int nplug;
static void *himem_stack;
extern int plat_run_on_stack(int (*fn)(void), void *top);

static void plug_release(void)
{
    while (nplug > 0) free(plug[--nplug]);
}

long plat_himem_used = 0;           /* bytes de RAM interna tapados (informe) */
int plat_himem_ok = 0;

void plat_himem_init(void)
{
    static const long sz[] = { 32768L, 16384L, 8192L, 4096L, 2048L, 1024L, 512L, 256L, 128L, 64L, 0 };
    int i;
    void *p;
    for (i = 0; sz[i]; i++)
        for (;;) {
            p = malloc((size_t)sz[i]);
            if (!p) break;
            if ((unsigned long)p >= LOW_END) { free(p); break; }     /* esta talla ya cae arriba: se prueba una menor por si cabe en los huecos de abajo */
            if (nplug >= PLUG_MAX) { free(p); break; }
            plug[nplug++] = p; plat_himem_used += sz[i];
        }
    p = malloc(256L);                                               /* ¿queda memoria alta? */
    if (!p || (unsigned long)p < LOW_END) { if (p) free(p); plug_release(); plat_himem_used = 0; return; }
    free(p);
    himem_stack = malloc(24576L);                                   /* pila alta de 24 KB */
    plat_himem_ok = himem_stack != NULL;
}

int plat_run(int (*fn)(void))                                       /* fn con la pila alta, si la hay */
{
    if (!himem_stack) return fn();
    return plat_run_on_stack(fn, (char *)himem_stack + 24576L);
}

/* 1 si hay RAM de expansion y aun asi el codigo o los datos globales estan en la contenida (el programa seria ~20 % mas lento) */
int plat_low_warn(void)
{
    return plat_himem_ok && ((unsigned long)&plat_low_warn < LOW_END || (unsigned long)&plat_himem_used < LOW_END);
}

void plat_himem_done(void)
{
    plug_release();
}

void plat_init(void)
{
    plat_mode4();
    plat_timer_on();
}

void plat_exit(void)
{
    plat_isr_fn = 0;
    plat_timer_off();
    plat_himem_done();
}

u8 *plat_screen(void) { return (u8 *)0x20000L; }
u32 plat_ticks(void) { return (u32)plat_tickcnt; }
void plat_frame(void) { }

/* La RAM interna del QL ($20000-$3FFFF) comparte bus con el ZX8301: cada acceso de la CPU espera a que el video lea su linea
 * (hasta 12 ciclos por byte en las lineas visibles). La RAM de expansion ($40000 en adelante) no tiene contencion, asi que el
 * buffer donde se dibuja la vista 3D y las copias que se leen a menudo deben estar alli. QDOS reparte la memoria del monton
 * de abajo arriba: si malloc devuelve una direccion baja, se queda ese bloque (para no volver a recibirlo) y se pide otro; al
 * terminar se liberan los bloques bajos. Si tras 64 intentos sigue siendo baja, se usa la que haya. */
void *plat_alloc_hi(long n)
{
    void *low[64], *p;
    int nl = 0, i;
    p = malloc((size_t)n);
    while (p && (unsigned long)p < 0x40000UL && nl < 64) {
        low[nl++] = p;
        p = malloc((size_t)n);
    }
    if (!p && nl) p = low[--nl];                 /* no hay mas: nos quedamos con un bloque bajo */
    for (i = 0; i < nl; i++) free(low[i]);
    return p;
}

/* RAM libre del monton: se piden bloques de mayor a menor (512 KB ... 4 KB) mientras malloc responda y se devuelven al final en orden inverso.
 * Con bloques de 4 KB sueltos el monton quedaba troceado y los bloques grandes de las teselas ya no cabian (con 640 KB se recargaban sin parar). */
long plat_free_kb(void)
{
    static void *blk[64];
    long sz, kb = 0;
    int n = 0;
    for (sz = 524288L; sz >= 4096L; sz >>= 1)
        while (n < 64 && (blk[n] = malloc((size_t)sz)) != NULL) { kb += sz >> 10; n++; }
    while (n > 0) free(blk[--n]);
    return kb;
}

/* "mono.acf" -> "flp1_mono_acf" */
void plat_path(char *out, const char *name)
{
    int n;
    strcpy(out, plat_dev);
    n = (int)strlen(out);
    while (*name) { out[n++] = (char)(*name == '.' ? '_' : *name); name++; }
    out[n] = 0;
}

/* ficheros "<nombre>_<ext>" del disco de datos (QDOS devuelve cada nombre con el prefijo de la unidad: "flp1_vuelo1_sav") */
int plat_list(const char *ext, char names[][10], int max)
{
    DIR *d = opendir(plat_dev);
    struct dirent *de;
    const char *nm;
    int n = 0, i, j, len, pl = (int)strlen(plat_dev), el = (int)strlen(ext);
    char t[10];
    if (!d) return 0;
    while (n < max && (de = readdir(d)) != NULL) {
        nm = de->d_name;
        for (i = 0; i < pl && nm[i] && tolower((unsigned char)nm[i]) == tolower((unsigned char)plat_dev[i]); i++) ;
        if (i == pl) nm += pl;
        len = (int)strlen(nm) - el - 1;
        if (len < 1 || len > 8 || nm[len] != '_') continue;
        for (i = 0; i < el && tolower((unsigned char)nm[len + 1 + i]) == tolower((unsigned char)ext[i]); i++) ;
        if (i != el) continue;
        for (i = 0; i < len; i++) names[n][i] = (char)toupper((unsigned char)nm[i]);
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
