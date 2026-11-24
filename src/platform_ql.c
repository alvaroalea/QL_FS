/* platform_ql.c - Sinclair QL real (QDOS / SMSQ), compilado con qcc (XTC68) */
#include "platform.h"
#include "sound.h"
#include <string.h>

extern volatile long plat_tickcnt;     /* ql68k.s, incrementado a 50 Hz */
void plat_mode4(void);
void plat_timer_on(void);
void plat_timer_off(void);

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

void plat_init(void)
{
    plat_mode4();
    plat_timer_on();
}

void plat_exit(void)
{
    plat_timer_off();
}

u8 *plat_screen(void) { return (u8 *)0x20000L; }
u32 plat_ticks(void) { return (u32)plat_tickcnt; }
void plat_frame(void) { }

/* "mono.acf" -> "flp1_mono_acf" */
void plat_path(char *out, const char *name)
{
    int n;
    strcpy(out, plat_dev);
    n = (int)strlen(out);
    while (*name) { out[n++] = (char)(*name == '.' ? '_' : *name); name++; }
    out[n] = 0;
}
