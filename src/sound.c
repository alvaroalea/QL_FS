/* sound.c - efectos del simulador sobre el PSG (se llama a 25 Hz) */
#include "sound.h"

s32 psg_clock = PSG_CLOCK_HZ;
static u32 sclock;
static int crash_t;

static void tone(int ch, s32 hz)
{
    s32 p;
    if (hz < 20) hz = 20;
    p = psg_clock / (16L * hz);
    if (p > 4095) p = 4095;
    if (p < 1) p = 1;
    psg_write(ch * 2, (int)(p & 255));
    psg_write(ch * 2 + 1, (int)(p >> 8));
}

void snd_init(void)
{
    int i;
    for (i = 0; i < 14; i++) psg_write(i, 0);
    psg_write(7, 0x2A);        /* tono A y C, ruido en B; puertos E/S como entrada */
    sclock = 0; crash_t = 0;
}

void snd_off(void)
{
    psg_write(8, 0); psg_write(9, 0); psg_write(10, 0);
    psg_write(7, 0x3F);
}

void snd_update(const Flight *f, const Radios *r, int beep, int crash)
{
    const AircraftDef *a = f->ac;
    s32 rpm, v, vol;
    int volc = 0;
    sclock++;
    if (crash) {
        if (crash_t == 0) crash_t = 40;
        if (crash_t > 0) crash_t--;
        psg_write(6, 31);
        psg_write(9, crash_t * 15 / 40);
        psg_write(8, 0); psg_write(10, 0);
        return;
    }
    crash_t = 0;
    /* motor */
    if (a->eng_type == ENG_PISTON && f->e[0].running) {
        rpm = f->e[0].rpm >> 16;
        tone(0, rpm / 30);                       /* 4 cilindros: rpm/60*2 */
        vol = 6 + (fxmul(f->e[0].power, FXI(8)) / (a->power ? a->power : 1));
        psg_write(8, (int)CLAMP(vol, 0, 13));
    } else if (a->eng_type == ENG_JET && f->e[0].running) {
        tone(0, 200 + (fxmul(f->e[0].n1, FXI(1800)) >> 16));   /* silbido de turbina */
        psg_write(8, (int)(4 + (fxmul(f->e[0].n1, FXI(6)) >> 16)));
    } else psg_write(8, 0);
    /* ruido: viento y chorro */
    v = MS2KT(f->ias) >> 16;
    vol = v / 25;
    if (a->eng_type == ENG_JET) vol += fxmul(f->e[0].n1, FXI(8)) >> 16;
    psg_write(6, (int)CLAMP(31 - v / 12, 2, 31));
    psg_write(9, (int)CLAMP(vol, 0, 12));
    /* avisos (canal C), por prioridad */
    if (f->stall_warn) { tone(2, 900); volc = 13; }
    else if (a->retract && !f->gear_down && f->thr[0] < FXF(2, 10) && f->agl < FXI(150) && !f->on_ground) {
        tone(2, 600); volc = (sclock & 8) ? 12 : 0;
    } else if (r->marker == NAV_OM) { tone(2, 400); volc = (sclock % 16) < 10 ? 11 : 0; }
    else if (r->marker == NAV_MM) { tone(2, 1300); volc = (sclock % 12) < 6 || (sclock % 12) == 8 ? 11 : 0; }
    else if (r->marker == NAV_IM) { tone(2, 3000); volc = (sclock & 2) ? 11 : 0; }
    else if (beep) { tone(2, 1000); volc = 10; }
    psg_write(10, volc);
}
