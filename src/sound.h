/* sound.h - sonido con el PSG (AY-3-8910 / parte SSG del YM2203 de la QSound)
 * Canal A: motor (tono) - Canal B: ruido (viento / reactor) - Canal C: avisos
 * (bocina de perdida, aviso de tren, balizas, pitido ATC). */
#ifndef SOUND_H
#define SOUND_H
#include "qfs.h"
#include "flight.h"
#include "nav.h"

/* Reloj efectivo del PSG en Hz. Debe ajustarse a la tarjeta (ver informe):
 * AY a 2 MHz -> 2000000. Se puede cambiar en tiempo de ejecucion. */
#ifndef PSG_CLOCK_HZ
#define PSG_CLOCK_HZ 2000000L
#endif

void snd_init(void);
void snd_update(const Flight *f, const Radios *r, int beep, int crash);
void snd_off(void);
extern s32 psg_clock;

/* acceso al chip (platform_ql.c / platform_host.c) */
void psg_write(int reg, int val);
#endif
