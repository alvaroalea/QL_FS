/* platform.h - capa de plataforma: QL real (platform_ql.c + ql68k.s) o PC (platform_host.c) */
#ifndef PLATFORM_H
#define PLATFORM_H
#include "qfs.h"

void  plat_init(void);
void  plat_exit(void);
u8   *plat_screen(void);          /* pantalla modo 4 (0x20000 en el QL) */
u32   plat_ticks(void);           /* contador de 50 Hz */
int   plat_keyrow(int row);       /* KEYROW del IPC (bit a 1 = pulsada) */
void  plat_isr_set(void (*fn)(void));   /* funcion en C que se llama en cada interrupcion de 50 Hz (NULL = ninguna); en PC no hace nada */
int   plat_run(int (*fn)(void));           /* llama a fn con la pila en RAM alta (QL); en PC, directamente */
int   plat_low_warn(void);                 /* QL: 1 si el programa esta en la RAM contenida habiendo expansion; en PC 0 */
void  plat_himem_init(void);            /* QL: pila y monton en la RAM sin contencion (doc/optimizacion.md, «Memoria alta»); en PC no hace nada */
void  plat_frame(void);           /* fin de cuadro (en PC: volcado de pantalla) */
void *plat_alloc_hi(long n);      /* memoria de la RAM de expansion (>= $40000, sin contencion con el ZX8301); en PC, malloc */
long  plat_free_kb(void);         /* KB que quedan en el monton (se prueba con bloques y se liberan); en PC un valor grande fijo */
void  plat_path(char *out, const char *name);   /* "mono.acf" -> "flp1_mono_acf" */

/* matriz del teclado QL: codigo = fila*8 + bit (KEYROW) - comprobar en QL real */
#define K(row, bit) ((row) * 8 + (bit))
#define KEY_ENTER K(1,0)
#define KEY_LEFT  K(1,1)
#define KEY_UP    K(1,2)
#define KEY_ESC   K(1,3)
#define KEY_RIGHT K(1,4)
#define KEY_BSL   K(1,5)
#define KEY_SPACE K(1,6)
#define KEY_DOWN  K(1,7)
#define KEY_F4    K(0,0)
#define KEY_F1    K(0,1)
#define KEY_5     K(0,2)
#define KEY_F2    K(0,3)
#define KEY_F3    K(0,4)
#define KEY_F5    K(0,5)
#define KEY_4     K(0,6)
#define KEY_7     K(0,7)
#define KEY_RBR   K(2,0)
#define KEY_Z     K(2,1)
#define KEY_DOT   K(2,2)
#define KEY_C     K(2,3)
#define KEY_B     K(2,4)
#define KEY_POUND K(2,5)
#define KEY_M     K(2,6)
#define KEY_QUOTE K(2,7)
#define KEY_LBR   K(3,0)
#define KEY_CAPS  K(3,1)
#define KEY_K     K(3,2)
#define KEY_S     K(3,3)
#define KEY_F     K(3,4)
#define KEY_EQ    K(3,5)
#define KEY_G     K(3,6)
#define KEY_SEMI  K(3,7)
#define KEY_L     K(4,0)
#define KEY_3     K(4,1)
#define KEY_H     K(4,2)
#define KEY_1     K(4,3)
#define KEY_A     K(4,4)
#define KEY_P     K(4,5)
#define KEY_D     K(4,6)
#define KEY_J     K(4,7)
#define KEY_9     K(5,0)
#define KEY_W     K(5,1)
#define KEY_I     K(5,2)
#define KEY_TAB   K(5,3)
#define KEY_R     K(5,4)
#define KEY_MINUS K(5,5)
#define KEY_Y     K(5,6)
#define KEY_O     K(5,7)
#define KEY_8     K(6,0)
#define KEY_2     K(6,1)
#define KEY_6     K(6,2)
#define KEY_Q     K(6,3)
#define KEY_E     K(6,4)
#define KEY_0     K(6,5)
#define KEY_T     K(6,6)
#define KEY_U     K(6,7)
#define KEY_SHIFT K(7,0)
#define KEY_CTRL  K(7,1)
#define KEY_ALT   K(7,2)
#define KEY_X     K(7,3)
#define KEY_V     K(7,4)
#define KEY_SLASH K(7,5)
#define KEY_N     K(7,6)
#define KEY_COMMA K(7,7)
#endif
