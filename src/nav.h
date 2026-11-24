/* nav.h - radios y navegacion por instrumentos: VOR, ILS (LOC+GS), DME, NDB/ADF, balizas */
#ifndef NAV_H
#define NAV_H
#include "qfs.h"
#include "world.h"
#include "flight.h"

typedef struct {
    u16 com, nav1, nav2, adf;   /* frecuencias (10 kHz; ADF en kHz) */
    u16 obs1, obs2;             /* BAM16 */
    u16 squawk;
    /* resultados NAV1 */
    const Nav *n1;
    int  n1_loc;                /* 1 si es localizador */
    int  n1_flag;               /* 1 TO, -1 FROM, 0 sin senal */
    s16  cdi1;                  /* -127..127 (+ = volar a la derecha) */
    int  gs_valid;
    s16  gs;                    /* -127..127 (+ = volar hacia arriba) */
    s32  dme10;                 /* decimas de milla, -1 sin DME */
    /* NAV2 (solo VOR) */
    const Nav *n2;
    int  n2_flag;
    s16  cdi2;
    /* ADF */
    int  adf_valid;
    u16  adf_rel;               /* marcacion relativa BAM16 */
    int  marker;                /* 0, NAV_OM, NAV_MM, NAV_IM */
} Radios;

void nav_init(Radios *r);
void nav_update(Radios *r, World *w, const Flight *f);
const Nav *nav_find(World *w, u16 freq, int ndb, s32 x, s32 y);
void freq_str(u16 f, char *buf);       /* 11030 -> "110.30" */
#endif
