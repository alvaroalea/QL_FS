/* aircraft.h - definicion de avion cargada de disco (fichero de texto .ACF)
 * Todos los valores numericos se guardan en Q16 (salvo que se indique). */
#ifndef AIRCRAFT_H
#define AIRCRAFT_H
#include "qfs.h"
#include "view3d.h"

#define ENG_NONE    0     /* planeador */
#define ENG_PISTON  1
#define ENG_JET     2

#define PROP_FIXED  0     /* paso fijo */
#define PROP_CS     1     /* velocidad constante (palanca de paso/RPM) */

typedef struct {
    char name[24];
    int  eng_type, n_eng, prop_type;
    int  retract;                  /* tren retractil */
    s32  mass;                     /* kg (entero) */
    s32  fuel_max;                 /* litros Q16 */
    /* aerodinamica */
    s32  S;                        /* superficie alar m2 */
    s32  cl0, cla;                 /* CL a alfa 0, pendiente por radian */
    s32  alpha_stall;              /* rad */
    s32  cd0, k_ind;
    s32  dcl_flap, dcd_flap, dalpha_flap; /* a flaps completos */
    s32  dcd_gear;
    int  n_flaps;                  /* posiciones (incluida 0) */
    int  flap_deg[6];
    /* control */
    s32  roll_rate;                /* rad/s maximo a Vref */
    s32  pitch_auth;               /* alfa extra por elevador completo (rad) */
    s32  vref;                     /* m/s: referencia de eficacia de mandos */
    s32  steer;                    /* direccion en tierra */
    /* motor (por motor) */
    s32  power;                    /* W/1000 (kW) piston */
    s32  rpm_max, rpm_idle, rpm_static;
    s32  thrust;                   /* kN reactor */
    s32  spool;                    /* reactor: fraccion N1 por segundo */
    s32  ff_max;                   /* l/h a plena potencia */
    s32  eng_y;                    /* brazo lateral (m) del motor en bimotores */
    /* marcas de velocimetro, nudos (enteros) */
    int  vs0, vs1, vfe, vno, vne;
    int  max_alt;                  /* m */
    s32  gear_h;                   /* altura del ojo sobre ruedas (m Q16) */
    s32  pitch_gnd;                /* actitud en el suelo (rad) */
    Model model;
} AircraftDef;

int aircraft_load(AircraftDef *a, const char *fname);
#endif
