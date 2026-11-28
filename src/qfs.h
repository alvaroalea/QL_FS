/*
 * AFS - Simulador de vuelo para Sinclair QL (68008, modo 4 512x256)
 * qfs.h : tipos, configuracion global y aritmetica de punto fijo.
 *
 * Reglas del proyecto:
 *  - C89 estricto (c68/XTC68): comentarios con barra-asterisco, declaraciones
 *    al principio de bloque, nada de coma flotante en ejecucion.
 *  - Q16 = s32 con 16 bits de fraccion. Q14 = s16 (+-2.0) para matrices/senos.
 *  - Angulos de render: BAM16 (65536 = 360 grados). Angulos de fisica: rad Q16.
 *  - Mundo: x = este, y = norte, z = arriba. Posiciones en metros Q8 (s32),
 *    lo que da +-8388 km de rango, mas que suficiente para varios escenarios.
 */
#ifndef QFS_H
#define QFS_H

#ifdef HOST
#include <stdint.h>
typedef int32_t  s32;
typedef uint32_t u32;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int8_t   s8;
typedef uint8_t  u8;
#else
/* c68: int y long son de 32 bits, short de 16 */
typedef long           s32;
typedef unsigned long  u32;
typedef short          s16;
typedef unsigned short u16;
typedef signed char    s8;
typedef unsigned char  u8;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ---------------- pantalla ---------------- */
#define SCR_W       512
#define SCR_H       256
#define SCR_STRIDE  128          /* bytes por linea en modo 4 */
#define SCR_BYTES   32768

/* ventana 3D: 480 x 96, el resto de columnas es borde */
#define VIEW_X0     16
#define VIEW_W      480
#define VIEW_X1     (VIEW_X0 + VIEW_W - 1)
#define VIEW_Y0     2            /* fila de pantalla donde empieza */
#define VIEW_H      96
#define VIEW_CX     (VIEW_X0 + VIEW_W / 2)
#define VIEW_CY     (VIEW_H / 2)

#define PANEL_Y0    100          /* primera fila del panel */

/* colores modo 4 (indice de 2 bits = G<<1 | R) */
#define C_BLACK 0
#define C_RED   1
#define C_GREEN 2
#define C_WHITE 3

/* relleno: palabra alta = patron filas pares, baja = filas impares.
 * cada patron de 16 bits = byte G (alto) + byte R (bajo), 8 pixeles. */
typedef u32 Fill;
#define PAT_SOLID(c) ((u16)((((c) & 2) ? 0xFF00 : 0) | (((c) & 1) ? 0x00FF : 0)))
#define FILL_SOLID(c) (((u32)PAT_SOLID(c) << 16) | PAT_SOLID(c))

/* ---------------- punto fijo ---------------- */
#define FX_ONE      65536L
#define FXI(n)      ((s32)(n) * 65536L)
#define FXF(n, d)   ((s32)(((s32)(n) * 65536L) / (s32)(d)))   /* n/d sin floats */
#define FX_PI       205887L
#define FX_2PI      411775L
#define FX_HALFPI   102944L
#define FX_DEG      1144L        /* 1 grado en rad Q16 */
#define RAD2BAM     10430L       /* rad Q16 -> BAM16 via fxmul */

s32 fxmul(s32 a, s32 b);         /* (a*b)>>16 con signo, 32x32 -> 32 */
s32 fxdiv(s32 a, s32 b);         /* (a<<16)/b aproximado, sin 64 bits */
/* a*b/d truncado hacia cero con 16x16 (muls) y 32/16 (divs); MDQ_BIG si no cabe en esa via rapida (version
 * en ensamblador en ql68k.s con USE_ASM_MDQ; la de C hace lo mismo) */
#define MDQ_BIG 0x7FFFFFFFL
s32 mdq(s32 a, s32 b, s32 d);
s32 isqrt32(u32 v);              /* raiz entera */
s16 isin(u16 bam);               /* Q14 */
s16 icos(u16 bam);
u16 iatan2(s32 y, s32 x);        /* BAM16, 0 = +y (norte), sentido horario */
s32 fxsin(s32 rad);              /* rad Q16 -> Q16 */
s32 fxcos(s32 rad);
s32 fxlerp_tab(const s32 *tab, int n, s32 x_q16_step); /* tabla equiespaciada */

/* multiplicacion 16x16 -> 32 (MULS en el 68000) */
#define MULS16(a, b) ((s32)(s16)(a) * (s32)(s16)(b))

extern const s16 sin_tab[1024];
extern const u16 atan_tab[257];
extern const s32 atm_sigma[17];
extern const s32 atm_delta[17];
extern const u8  font5x7[64 * 7];

#define ABS(a) ((a) < 0 ? -(a) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/* ---------------- tiempo ---------------- */
#define TICKS_HZ    50           /* interrupcion de cuadro del QL (PAL) */
#if defined(USE_SCHED) && !defined(PHYS_DT)   /* planificador por interrupciones (doc/optimizacion.md, «Planificador por turnos»): fisica a 12,5 Hz y motores aparte */
#define PHYS_DT     5243L        /* 1/12,5 s en Q16 */
#define PHYS_HZ     12           /* (solo aproximado: el reloj del simulador se cuenta con los 50 Hz) */
#endif
#if defined(USE_SCHED) && !defined(ENG_EXTERNAL)
#define ENG_EXTERNAL             /* flight_engines() la llama el planificador (tarea E) */
#endif
#if defined(USE_SCHED) && !defined(AI_INCR)
#define AI_INCR                  /* horizonte con una division por cuadro (panel.c): 160 -> 110 ms */
#endif
#if defined(AI_INCR) && !defined(AI_DIFF) && !defined(AI_NODIFF)
#define AI_DIFF                  /* horizonte: solo se repintan los pixeles que cambian (panel.c) */
#endif
#ifndef PHYS_DT
#define PHYS_HZ     25           /* paso fijo de fisica */
#define PHYS_DT     2621L        /* 1/25 s en Q16 */
#endif                           /* (se puede cambiar con -DPHYS_DT=5243L -DPHYS_HZ=12: estudio de la fisica a 12,5 Hz) */

#endif
