/* panel.h - panel de instrumentos (filas 100..255 de la pantalla) */
#ifndef PANEL_H
#define PANEL_H
#include "qfs.h"
#include "gfx.h"
#include "flight.h"
#include "nav.h"
#include "atc.h"

#define PANEL_PARTS 4      /* el panel se refresca por grupos, uno por cuadro */

typedef struct {
    u32 clock;             /* segundos del dia */
    const char *status;    /* linea de estado */
    int warn;              /* bits: 1 perdida, 2 tren, 4 combustible */
    int fps10, lps10;      /* vistas 3D y cuadros de simulacion por segundo x10; -1 midiendo, -2 oculto */
} PanelInfo;

/* acf: ruta del fichero del avion; el fondo del panel se carga de su gemelo .src (volcado crudo de 32 KB de la pantalla) */
/* el .src guarda solo las filas PANEL_SRC_Y0..255 de la pantalla (las de arriba son de la vista 3D y estan a cero): 20 KB en vez de 32 */
#define PANEL_SRC_Y0 (PANEL_Y0 - 2)
#define PANEL_SRC_BYTES ((long)(SCR_H - PANEL_SRC_Y0) * SCR_STRIDE)
void panel_init(u8 *screen, const AircraftDef *ac, const char *acf, u8 *work);
#ifdef HOST
void panel_build(u8 *screen, const AircraftDef *ac);   /* dibuja el fondo estatico (lo usa tools/mkpanel.c para generar los .src) */
#endif
/* part: 0..PANEL_PARTS-1 (grupo de instrumentos) ; el horizonte artificial
 * y el teletipo ATC se pintan siempre */
void panel_update(u8 *screen, const Flight *f, const Radios *r, const Atc *a,
                  const PanelInfo *pi, int part);
void panel_piece(u8 *screen, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi, int k);   /* 0..5, ver panel.c */
int  panel_job_step(u8 *screen, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi, u32 tick0);   /* panel reanudable por turnos, ver panel.c */
void panel_all(u8 *screen, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi);
#endif
