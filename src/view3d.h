/* view3d.h - camara, transformacion, recorte y dibujo de la vista 3D */
#ifndef VIEW3D_H
#define VIEW3D_H
#include "qfs.h"
#include "gfx.h"
#include "world.h"

#define LIGHT_DAY   0
#define LIGHT_DUSK  1
#define LIGHT_NIGHT 2

typedef struct {
    s32 x, y, z;             /* posicion Q8 m */
    u16 hdg; s16 pit, bnk;   /* BAM16 */
    s16 m[9];                /* filas: derecha, arriba, adelante (Q14) */
    s16 fx, fy;              /* focales en pixeles (fy = fx*2/3: pixel del QL) */
    int light;
    s32 ground;              /* Q8 m: cota del suelo bajo la camara */
    s16 cloud_base, cloud_top;   /* m, 0 = sin nubes */
    s32 vis;                 /* visibilidad m */
    u16 lst;                 /* tiempo sidereo local, BAM16 (la hora da el giro del cielo) */
    s16 star_lim;            /* (magnitud limite + 1) * 10; 0 = sin estrellas */
} Camera;

typedef struct {
    int nv, nf;              /* nf incluye las ngear ultimas, que son el tren (segmentos) */
    int ngear;
    s16 *v;                  /* 1/8 m, ejes del avion: x dcha, y arriba, z adelante */
    u8  *f;
    int fbytes;
} Model;

typedef struct {             /* estadisticas del ultimo cuadro */
    int objs, faces, lines, verts, tiles;
} ViewStats;
extern ViewStats vstats;

void attitude_matrix(u16 hdg, s16 pit, s16 bnk, s16 *m);
void cam_set(Camera *c, s32 x, s32 y, s32 z, u16 hdg, s16 pit, s16 bnk);
void cam_zoom(Camera *c, int z256);  /* 256 = normal */

void view_sky(Surface *s, const Camera *c);
void view_stars(Surface *s, const Camera *c);   /* las pinta view_sky de noche y al anochecer */
void view_world(Surface *s, const Camera *c, World *w);
/* la misma vista partida: begin reune y ordena (devuelve el coste total), step dibuja objetos hasta gastar el presupuesto */
s32  view_world_begin(const Camera *c, World *w);
void view_begin_init(const Camera *c, World *w);     /* la misma reunion y orden en pasos pequenos (ver view3d.c) */
int  view_gather_step(const Camera *c, int n);
s32  view_sort_step(int n);
extern int view_wire;                                /* 1 = solo alambre */
int  view_world_step(Surface *s, const Camera *c, s32 budget);   /* 1 si no quedan objetos */
/* avion exterior: posicion Q8, matriz de actitud (filas derecha/arriba/adelante); gear = 0 oculta el tren */
void view_model(Surface *s, const Camera *c, const Model *mdl,
                s32 ax, s32 ay, s32 az, const s16 *am, int gear);

/* nucleo: 3x3 Q14 sobre n vertices s16 -> s32 (C o ensamblador) */
void xform3(const s16 *m, const s16 *in, s32 *out, int n);
#endif
