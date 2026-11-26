/* sim.h - bucle principal del simulador */
#ifndef SIM_H
#define SIM_H
#include "qfs.h"
#include "world.h"
#include "aircraft.h"
#include "flight.h"
#include "nav.h"
#include "atc.h"
#include "view3d.h"
#include "panel.h"

#define START_RWY  0     /* salida desde la cabecera de pista */
#define START_RAMP 1     /* salida desde la plataforma */
#define START_AIR  2     /* salida en vuelo */
#define START_LAND 3     /* salida aterrizando: en la senda de descenso hacia una pista */
#define START_CONT 4     /* continuar el vuelo que se dejo al volver al menu (o el cargado de disco) */

#define LAND_PTS 4       /* puntos de la senda de START_LAND: 20 NM, IF, FAF y corta final */

#ifndef LAND_THR
#define LAND_THR 20     /* gases (%) al empezar aterrizando: helice (descenso de unos 3 grados) */
#endif
#ifndef LAND_THR_JET
#define LAND_THR_JET 45 /* y reactor */
#endif

#define VIEW_COCKPIT 0
#define VIEW_CHASE   1
#define VIEW_TOWER   2
#define VIEW_SPOT    3

typedef struct {
    World w;
    AircraftDef ac;
    Flight f;
    Radios r;
    Atc atc;
    Camera cam;
    u8 *scr;
    Surface view;
    int view_mode, look, zoom;
    u32 tick_phys, nsteps;
    u32 clock;            /* segundos desde medianoche */
    int clock_sub;
    int part, paused, quit;
    u8 keys[64], prev[64];
    char status[84];
    int start_apt, start_mode, start_rwy, start_pt;   /* aeropuerto de salida, tipo de salida (START_*) y cabecera (indice en Airport.rwy) */
    int crash_shown;
    u32 frames;
    int vpart;                       /* 0: el proximo cuadro empieza la vista 3D; 1: la termina */
    int show_fps, fps_n, fps_v, fps10, lps10;     /* indicador de FPS (tecla \): vistas y cuadros de simulacion por segundo x10 */
    u32 fps_t0;
} Sim;

extern Sim sim;
extern int sim_start_apt, sim_start_mode, sim_start_rwy, sim_start_pt, sim_restore;   /* eleccion del menu, la lee sim_init (por defecto 0: primer aeropuerto, en pista); sim_start_pt: punto de la senda (START_LAND);
                                    sim_restore: 1 = sim_init restaura el vuelo guardado (START_CONT) */

int  sim_init(const char *scn, const char *acf, int hour);
void sim_reset(void);
void sim_frame(void);
void sim_render(void);       /* solo dibujo (lo usa el PC para las capturas) */
extern const s32 land_dist[LAND_PTS];   /* distancia al umbral (m) de cada punto */
int  sim_menu(char *acf, char *scn, int *hour);
#endif
