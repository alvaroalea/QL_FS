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
    int start_apt;
    int crash_shown;
    u32 frames;
    int vpart;                       /* 0: el proximo cuadro empieza la vista 3D; 1: la termina */
    int show_fps, fps_n, fps_v, fps10, lps10;     /* indicador de FPS (tecla \): vistas y cuadros de simulacion por segundo x10 */
    u32 fps_t0;
} Sim;

extern Sim sim;

int  sim_init(const char *scn, const char *acf, int hour);
void sim_reset(void);
void sim_frame(void);
void sim_render(void);       /* solo dibujo (lo usa el PC para las capturas) */
int  sim_menu(char *acf, char *scn, int *hour);
#endif
