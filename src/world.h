/* world.h - escenario: cabecera, radioayudas, aeropuertos y teselas (tiles)
 * que se cargan bajo demanda desde disco (cache LRU).
 *
 * Fichero .SCN (big-endian, generado por tools/mkscen.py):
 *   cabecera 64 bytes | directorio de teselas | radioayudas | aeropuertos |
 *   textos ATIS | blobs de teselas
 */
#ifndef WORLD_H
#define WORLD_H
#include "qfs.h"
#include <stdio.h>

/* tipos de objeto */
#define OBJ_GROUND  0      /* poligonos planos: pistas, lagos, campos */
#define OBJ_LINES   1      /* polilineas: carreteras, rios, costas */
#define OBJ_SOLID   2      /* caras 3D con ocultacion: edificios, montanas */
#define OBJ_LIGHTS  3      /* puntos de luz (noche/crepusculo) */

/* flags de objeto */
#define OF_HIT      0x01   /* colision (edificio / montana) */
#define OF_WATER    0x02   /* agua (SPLASH!) */
#define OF_NIGHT    0x04   /* visible de noche (contorno) */
#define OF_DAYLIGHT 0x08   /* luces visibles tambien de dia */
#define OF_MOUNTAIN 0x10
#define OF_SORT     0x20   /* ordenar caras por profundidad (manzanas, montanas) */
#define OF_PAPI     0x40   /* luces PAPI: 4 caras de 1 luz, blanca o roja segun el angulo de aproximacion (view3d.c) */
#define OF_ROWS     0x80   /* luces en filas: 2 vertices (extremos) y una cara [3, color, a, b, n] por fila de n luces equiespaciadas */

#ifndef SOLID_LOD
#define SOLID_LOD 5        /* alcance de dibujo de los solidos pequenos (edificios, lod < 128) en octavos de su lod: 5/8; 8 = sin recorte */
#endif

/* atributo de cara: bits 0-3 relleno, 4-5 color de contorno, 6 contorno, 7 sin relleno */
#define FA_FILL(a)   ((a) & 15)
#define FA_LCOL(a)   (((a) >> 4) & 3)
#define FA_OUTLINE   0x40
#define FA_NOFILL    0x80

typedef struct {
    u8  type, flags, lod, vshift;   /* lod: distancia maxima en unidades de 256 m */
    s32 ox, oy;                     /* origen en el mundo (m) */
    s16 oz;                         /* altitud del origen (m) */
    u16 radius;                     /* radio envolvente horizontal (m): colision, alcance, mapa */
    u16 vrad;                       /* radio de vision (m): distancia del origen al vertice mas lejano, con la altura (se calcula al cargar) */
    u8  nv, nf;
    u8  nvf, nff;                   /* version lejana (solo techos): los nvf primeros vertices y las nff primeras caras; 0 = no hay */
    u16 fbytes;
    s16 *v;                         /* nv*3, unidades (1/8 m) << vshift */
    u8  *f;                         /* caras: n, atributo, indices... */
    s32 reach;                      /* radio + alcance de dibujo (m, <<8 ya aplicado): d > reach = no se ve (calculado al cargar) */
    u8  flat;                       /* 1 si todos los vertices tienen la misma z (suelo, lineas, filas de luces): se transforman con 6 productos en vez de 9 (calculado al cargar) */
} Obj;

typedef struct {
    s16 tx, ty;          /* indice de tesela, -1 = libre */
    u16 nobj;
    u32 stamp;           /* para LRU */
    Obj *obj;
    u8  *mem;            /* bloque unico (objetos + datos) */
} Tile;

#define NAV_VOR  1
#define NAV_VORDME 2
#define NAV_NDB  3
#define NAV_ILS  4
#define NAV_OM   5
#define NAV_MM   6
#define NAV_IM   7
#define NAV_GS   8      /* antena de senda (misma frecuencia que el ILS) */

typedef struct {
    u8  type;
    u16 freq;            /* NAV/COM: 10 kHz (11030 = 110.30); NDB: kHz */
    s32 x, y;            /* m */
    s16 z;
    u16 course;          /* LOC: rumbo de aproximacion BAM16 */
    u16 range;           /* km */
    u16 gs;              /* senda de planeo BAM16 (3 grados = 546) */
    char ident[5];
} Nav;

typedef struct {
    u16 hdg;             /* BAM16 */
    u16 len, wid;        /* m */
    s32 tx, ty;          /* umbral (m) */
} Runway;

typedef struct {
    char ident[5];
    char name[29];
    s32 x, y;
    s16 elev;
    u16 atis, twr;       /* frecuencias */
    u8  nrwy;
    Runway rwy[4];
    u8  has_plat;        /* el escenario define una plataforma de estacionamiento */
    s16 plat_dx, plat_dy;/* posicion de la plataforma respecto a (x, y) en m */
    u16 plat_hdg;        /* rumbo del eje de la plataforma, BAM16 */
    char *atis_text;
} Airport;

#define TILE_CACHE 49      /* 7x7 teselas: visibilidad de hasta ~48 km; es el minimo de la cache y el maximo de teselas de una vista */
#define CACHE_RESERVE_KB 96L   /* RAM libre que se deja sin usar al dimensionar la cache (QDOS, pila, avisos): con 640 KB no queda nada que repartir */

typedef struct {
    FILE *fp;
    char  name[33];
    int   tile_log2;
    s32   org_x, org_y;
    int   ntx, nty;
    int   nnav, napt;
    s16   elev;
    int   vis_km;          /* visibilidad propuesta por el escenario (0 = por defecto) */
    u32  *tdir_off;
    u16  *tdir_size;
    s16  *tdir_elev;
    Nav  *nav;
    Airport *apt;
    Tile *cache;           /* ncache entradas (TILE_CACHE como minimo; mas si sobra RAM: world_cache_autosize) */
    int   ncache, nloaded; /* entradas y teselas cargadas */
    u32   mem_used, mem_budget;   /* bytes de teselas cargadas y tope para pasar de TILE_CACHE teselas (0 = no se pasa) */
    u32   clock;
    s16  *slot;            /* tesela (ty*ntx+tx) -> indice en cache, -1 = no esta */
    u32   gen, hl_gen;             /* sube cada vez que se carga una tesela (invalida la lista de colisiones) */
    Obj **hl; int nhl, maxhl;   /* objetos con OF_HIT/OF_WATER de las teselas en cache */
    s32   hit_near;        /* distancia horizontal minima (m) a un objeto de colision en la ultima comprobacion */
    u32   bytes_loaded;
} World;

int   world_open(World *w, const char *fname);
void  world_close(World *w);
/* asegura en cache las teselas a menos de 'radius' m de (x,y) y devuelve
 * cuantas hay en 'out' */
/* tras abrir el escenario y reservar lo demas: reparte la RAM libre (free_kb) entre mas teselas en cache; con poca RAM no cambia nada */
void  world_cache_autosize(World *w, long free_kb);
int   world_tiles_near(World *w, s32 x, s32 y, s32 radius, Tile **out, int max);
s16   world_ground(World *w, s32 x, s32 y);   /* elevacion del suelo (m) */
const Airport *world_nearest_airport(World *w, s32 x, s32 y, s32 *dist);
/* colision con objetos: devuelve 0, 1 edificio, 2 montana, 3 agua */
int   world_hit(World *w, s32 x, s32 y, s32 z);
#endif
