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
} PanelInfo;

void panel_init(u8 *screen, const AircraftDef *ac);
/* part: 0..PANEL_PARTS-1 (grupo de instrumentos) ; el horizonte artificial
 * y el teletipo ATC se pintan siempre */
void panel_update(u8 *screen, const Flight *f, const Radios *r, const Atc *a,
                  const PanelInfo *pi, int part);
void panel_all(u8 *screen, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi);
#endif
