/* flight.h - modelo de vuelo y motores en punto fijo (paso fijo de 1/25 s) */
#ifndef FLIGHT_H
#define FLIGHT_H
#include "qfs.h"
#include "aircraft.h"
#include "world.h"

typedef struct {
    s32 rpm;            /* Q16 */
    s32 map;            /* inHg Q16 */
    s32 egt;            /* grados C Q16 */
    s32 n1;             /* reactor: 0..1.05 Q16 */
    s32 ff;             /* l/h Q16 */
    s32 power;          /* kW Q16 */
    s32 thrust;         /* N (entero) */
    int running;
} Engine;

#define CRASH_NONE     0
#define CRASH_GROUND   1
#define CRASH_GEAR     2
#define CRASH_BUILDING 3
#define CRASH_MOUNTAIN 4
#define CRASH_WATER    5
#define CRASH_OVERSTRESS 6

typedef struct {
    /* mandos (Q16) */
    s32 elev, ail, rud, trim;   /* -1..1 ; elev + = morro arriba */
    s32 thr[2], mix[2], prop[2];/* 0..1 */
    int flaps, gear_down, brakes, auto_coord, starter;
    /* estado */
    s32 x, y, z;                /* Q8 m (z = altitud del ojo, MSL) */
    s32 v;                      /* TAS m/s Q16 */
    s32 gam, alf, the, q;       /* rad Q16 */
    s32 psi, phi, p;            /* rad Q16 */
    s32 vs, ias, gs;            /* m/s Q16 */
    s32 turn, slip;             /* rad/s Q16, bola Q16 */
    s32 flap_pos, gear_pos;     /* Q16 0..1 */
    s32 sigma, delta;           /* atmosfera Q16 */
    s32 fuel;                   /* litros Q16 */
    s32 aL, aD, aT;             /* aceleraciones Q16 (depuracion) */
    s32 agl;                    /* m Q16 sobre el suelo */
    s32 wind_e, wind_n;         /* viento m/s Q16 (hacia donde sopla) */
    s32 accx, accy, accz;       /* restos de integracion (Q16 m) para no perder precision */
    int on_ground, stalled, stall_warn, crash;
    u32 rnd;
    Engine e[2];
    const AircraftDef *ac;
    s32 KL;                     /* 0.5*rho0*S/m  Q16 */
} Flight;

void flight_init(Flight *f, const AircraftDef *ac);
void flight_place(Flight *f, s32 x_m, s32 y_m, s32 z_m, u16 hdg_bam, s32 v_ms, int on_ground);
void flight_step(Flight *f, World *w);

/* conversiones para instrumentos */
#define MS2KT(v)   (fxmul((v), 127391L))      /* m/s Q16 -> nudos Q16 (1.9438) */
#define M2FT(v)    (fxmul((v), 215012L))      /* m -> pies (3.2808) */
#endif
