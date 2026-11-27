/* map.h - vista de mapa (tecla Q): el escenario visto desde arriba, orientado al norte, a pantalla completa */
#ifndef MAP_H
#define MAP_H
#include "qfs.h"
#include "world.h"
#include "flight.h"

#define MAP_LEVELS 4           /* escalas: 32, 64, 128 y 256 m por pixel (16, 33, 65 y 131 km de ancho) */

/* carga las teselas del mapa centrado en el avion (cambia la cache de teselas: quien llama debe parar la fisica mientras tanto) */
void map_prepare(World *w, const Flight *f, int lvl);
/* dibuja el mapa en la pantalla scr (32 KB, modo 4) con las teselas de map_prepare */
void map_draw(u8 *scr, World *w, const Flight *f, int lvl);
#endif
