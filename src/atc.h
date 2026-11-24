/* atc.h - control de trafico basico: ATIS y torre por la frecuencia COM */
#ifndef ATC_H
#define ATC_H
#include "qfs.h"
#include "world.h"
#include "flight.h"
#include "nav.h"

#define ATC_TEXT 200

typedef struct {
    const Airport *apt;      /* aeropuerto sintonizado */
    int  mode;               /* 0 nada, 1 ATIS, 2 torre */
    int  phase;              /* fase del vuelo vista por la torre */
    int  rwy;                /* pista asignada */
    char text[ATC_TEXT];     /* mensaje actual (teletipo) */
    int  pos, len, hold;     /* desplazamiento del teletipo */
    char letter;             /* letra ATIS */
    int  new_msg;            /* para el pitido */
} Atc;

#define PH_PARKED   0
#define PH_TAKEOFF  1        /* autorizado a despegar */
#define PH_DEPART   2
#define PH_ENROUTE  3
#define PH_APPROACH 4
#define PH_LAND     5        /* autorizado a aterrizar */
#define PH_LANDED   6

void atc_init(Atc *a);
void atc_update(Atc *a, World *w, const Flight *f, const Radios *r, int contact);
void atc_scroll(Atc *a);                /* llamar una vez por cuadro */
/* ventana de 'n' caracteres del teletipo */
void atc_window(const Atc *a, char *out, int n);
void atc_say(Atc *a, const char *s);
#endif
