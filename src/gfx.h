/* gfx.h - primitivas graficas para buffers en formato de pantalla QL modo 4
 *
 * Formato modo 4: cada palabra (2 bytes) son 8 pixeles; byte par = bits verdes,
 * byte impar = bits rojos, bit 7 = pixel izquierdo. G+R = blanco.
 */
#ifndef GFX_H
#define GFX_H
#include "qfs.h"

typedef struct {
    u8 *base;            /* fila 0 */
    s16 rows;            /* filas del buffer */
    s16 cx0, cy0, cx1, cy1;   /* rectangulo de recorte (inclusive) */
} Surface;

void gfx_init_surface(Surface *s, u8 *base, int rows);
void gfx_clip(Surface *s, int x0, int y0, int x1, int y1);

void gfx_clear(Surface *s, Fill f);
void gfx_hspan(Surface *s, int y, int x0, int x1, Fill f);      /* recorta */
void gfx_rect(Surface *s, int x0, int y0, int x1, int y1, Fill f);
void gfx_pixel(Surface *s, int x, int y, int c);
void gfx_line(Surface *s, s32 x0, s32 y0, s32 x1, s32 y1, int c); /* recorta */
void gfx_line_c(Surface *s, s32 x0, s32 y0, s32 x1, s32 y1, int c); /* recorte general (lo llama gfx_line en ensamblador) */
void gfx_line_xor_s(Surface *s, s32 x0, s32 y0, s32 x1, s32 y1, int c); /* recorta; OR exclusivo */
void gfx_poly(Surface *s, int n, const s16 *xy, Fill f);          /* scanline */
void gfx_ellipse(Surface *s, int cx, int cy, int rx, int ry, int c);
void gfx_char(Surface *s, int x, int y, int ch, int c);
void gfx_text(Surface *s, int x, int y, const char *t, int c);
int  gfx_textw(const char *t);
void gfx_copy_rows(u8 *dst, const u8 *src, int bytes);
void gfx_copy_box(u8 *dst, const u8 *src, int x0, int y0, int x1, int y1);

/* nucleo sin recorte: C o ensamblador (gfx68k.s) */
void gfx_hspan_raw(u8 *row, s32 x0, s32 x1, s32 pat);
void gfx_line_raw(u8 *base, s32 x0, s32 y0, s32 x1, s32 y1, s32 c);
void gfx_memcpy(u8 *dst, const u8 *src, s32 n);
void gfx_blit(u8 *dst, const u8 *src, s32 rows, s32 n, s32 dstride, s32 sstride);   /* filas de n bytes (par) con pasos distintos */
s32  lerp_div(s32 a, s32 b, s32 num, s32 den);               /* a + (b-a)*num/den exacto (ql68k.s en el QL) */
int  gfx_fill_convex(u8 *base, const s32 *p, int m, s32 f);   /* poligono convexo recortado; 0 si no es convexo */
void gfx_glyph(u8 *p, const u16 *spr, s32 cells);                 /* letra preparada (panel.c, glyph_make): 7 filas de 4 palabras, 1 o 2 grupos */
void gfx_text_raw(u8 *row0, s32 x, const char *t, s32 c);   /* texto 5x7 sin recorte desde la fila row0 */
void gfx_poly32(Surface *s, int n, const s32 *xy, Fill f);       /* recorta a la ventana y rellena */
void gfx_poly_gen(Surface *s, int m, const s32 *src, Fill f);     /* relleno general (no convexo) de un poligono ya recortado */
void gfx_line_xor(u8 *base, s32 x0, s32 y0, s32 x1, s32 y1, s32 c);

/* patrones de relleno con tramado */
extern const Fill fill_tab[16];
enum {
    F_BLACK, F_RED, F_GREEN, F_WHITE,
    F_GREY,          /* blanco/negro ajedrez */
    F_WATER,         /* verde/blanco */
    F_DKGREEN,       /* verde/negro */
    F_ORANGE,        /* rojo/blanco */
    F_BROWN,         /* rojo/verde */
    F_DKRED,         /* rojo/negro */
    F_LTGREY,        /* blanco con 1/4 negro */
    F_HAZE,          /* blanco con 1/4 verde */
    F_DKGREY,        /* negro con 1/4 blanco */
    F_SKYDUSK,       /* rojo con 1/4 blanco */
    F_FIELD,         /* verde con 1/4 rojo */
    F_NIGHTGND       /* negro con puntos verdes */
};
#endif
