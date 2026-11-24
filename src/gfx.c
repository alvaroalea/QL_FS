/* gfx.c - primitivas graficas modo 4. Las funciones *_raw tienen version en
 * ensamblador (gfx68k.s); se usan las de C si no se define USE_ASM_GFX. */
#include "gfx.h"

const Fill fill_tab[16] = {
    0x00000000UL, 0x00FF00FFUL, 0xFF00FF00UL, 0xFFFFFFFFUL,
    0xAAAA5555UL,             /* gris */
    0xFFAAFF55UL,             /* agua */
    0xAA005500UL,             /* verde oscuro */
    0xAAFF55FFUL,             /* naranja */
    0xAA5555AAUL,             /* marron */
    0x00AA0055UL,             /* rojo oscuro */
    0x7777DDDDUL,             /* gris claro */
    0xFF77FFDDUL,             /* bruma */
    0x88882222UL,             /* gris oscuro */
    0x88FF22FFUL,             /* cielo de atardecer */
    0x7788DD22UL,             /* campo */
    0x80000800UL              /* suelo de noche */
};

void gfx_init_surface(Surface *s, u8 *base, int rows)
{
    s->base = base;
    s->rows = (s16)rows;
    s->cx0 = 0; s->cy0 = 0; s->cx1 = SCR_W - 1; s->cy1 = (s16)(rows - 1);
}

void gfx_clip(Surface *s, int x0, int y0, int x1, int y1)
{
    s->cx0 = (s16)x0; s->cy0 = (s16)y0; s->cx1 = (s16)x1; s->cy1 = (s16)y1;
}

#ifndef USE_ASM_GFX
void gfx_hspan_raw(u8 *row, s32 x0, s32 x1, s32 pat)
{
    u8 g = (u8)(pat >> 8), r = (u8)pat, m0, m1, m;
    int w0 = (int)(x0 >> 3), w1 = (int)(x1 >> 3), i;
    u8 *p = row + (w0 << 1);
    m0 = (u8)(0xFF >> (x0 & 7));
    m1 = (u8)(0xFF << (7 - (x1 & 7)));
    if (w0 == w1) {
        m = (u8)(m0 & m1);
        p[0] = (u8)((p[0] & ~m) | (g & m));
        p[1] = (u8)((p[1] & ~m) | (r & m));
        return;
    }
    p[0] = (u8)((p[0] & ~m0) | (g & m0));
    p[1] = (u8)((p[1] & ~m0) | (r & m0));
    p += 2;
    for (i = w0 + 1; i < w1; i++) { p[0] = g; p[1] = r; p += 2; }
    p[0] = (u8)((p[0] & ~m1) | (g & m1));
    p[1] = (u8)((p[1] & ~m1) | (r & m1));
}

void gfx_line_raw(u8 *base, s32 x0, s32 y0, s32 x1, s32 y1, s32 c)
{
    s32 dx = x1 - x0, dy = y1 - y0, sx = 1, sy = SCR_STRIDE, err, e2, n;
    u8 *p;
    u8 bit;
    if (dx < 0) { dx = -dx; sx = -1; }
    if (dy < 0) { dy = -dy; sy = -SCR_STRIDE; }
    p = base + y0 * SCR_STRIDE + ((x0 >> 3) << 1);
    bit = (u8)(0x80 >> (x0 & 7));
    err = dx - dy;
    n = (dx > dy ? dx : dy) + 1;
    while (n--) {
        if (c & 2) p[0] |= bit; else p[0] &= (u8)~bit;
        if (c & 1) p[1] |= bit; else p[1] &= (u8)~bit;
        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            if (sx > 0) { bit >>= 1; if (!bit) { bit = 0x80; p += 2; } }
            else { bit <<= 1; if (!bit) { bit = 0x01; p -= 2; } }
        }
        if (e2 < dx) { err += dx; p += sy; }
    }
}

void gfx_memcpy(u8 *dst, const u8 *src, s32 n)
{
    while (n--) *dst++ = *src++;
}

void gfx_text_raw(u8 *row0, s32 x, const char *t, s32 c)
{
    const u8 *g;
    int ch, r, k;
    s32 px;
    u8 *p, bit;
    for (; *t; t++, x += 6) {
        ch = (unsigned char)*t;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        g = font5x7 + (ch - 32) * 7;
        for (r = 0; r < 7; r++)
            for (k = 0; k < 5; k++)
                if (g[r] & (0x10 >> k)) {
                    px = x + k;
                    p = row0 + (s32)r * SCR_STRIDE + ((px >> 3) << 1);
                    bit = (u8)(0x80 >> (px & 7));
                    if (c & 2) p[0] |= bit; else p[0] &= (u8)~bit;
                    if (c & 1) p[1] |= bit; else p[1] &= (u8)~bit;
                }
    }
}

void gfx_line_xor(u8 *base, s32 x0, s32 y0, s32 x1, s32 y1, s32 c)
{
    s32 dx = x1 - x0, dy = y1 - y0, sx = 1, sy = SCR_STRIDE, err, e2, n;
    u8 *p;
    u8 bit;
    if (dx < 0) { dx = -dx; sx = -1; }
    if (dy < 0) { dy = -dy; sy = -SCR_STRIDE; }
    p = base + y0 * SCR_STRIDE + ((x0 >> 3) << 1);
    bit = (u8)(0x80 >> (x0 & 7));
    err = dx - dy;
    n = (dx > dy ? dx : dy) + 1;
    while (n--) {
        if (c & 2) p[0] ^= bit;
        if (c & 1) p[1] ^= bit;
        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            if (sx > 0) { bit >>= 1; if (!bit) { bit = 0x80; p += 2; } }
            else { bit <<= 1; if (!bit) { bit = 0x01; p -= 2; } }
        }
        if (e2 < dx) { err += dx; p += sy; }
    }
}
#endif

void gfx_hspan(Surface *s, int y, int x0, int x1, Fill f)
{
    int t;
    if (y < s->cy0 || y > s->cy1) return;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (x0 < s->cx0) x0 = s->cx0;
    if (x1 > s->cx1) x1 = s->cx1;
    if (x0 > x1) return;
    gfx_hspan_raw(s->base + (s32)y * SCR_STRIDE, x0, x1,
                  (s32)((y & 1) ? (f & 0xFFFFUL) : (f >> 16)));
}

void gfx_rect(Surface *s, int x0, int y0, int x1, int y1, Fill f)
{
    int y;
    for (y = y0; y <= y1; y++) gfx_hspan(s, y, x0, x1, f);
}

void gfx_clear(Surface *s, Fill f)
{
    gfx_rect(s, s->cx0, s->cy0, s->cx1, s->cy1, f);
}

void gfx_pixel(Surface *s, int x, int y, int c)
{
    u8 *p, bit;
    if (x < s->cx0 || x > s->cx1 || y < s->cy0 || y > s->cy1) return;
    p = s->base + (s32)y * SCR_STRIDE + ((x >> 3) << 1);
    bit = (u8)(0x80 >> (x & 7));
    if (c & 2) p[0] |= bit; else p[0] &= (u8)~bit;
    if (c & 1) p[1] |= bit; else p[1] &= (u8)~bit;
}

/* --- recorte Cohen-Sutherland en s32 --- */
static int outcode(const Surface *s, s32 x, s32 y)
{
    int c = 0;
    if (x < s->cx0) c |= 1; else if (x > s->cx1) c |= 2;
    if (y < s->cy0) c |= 4; else if (y > s->cy1) c |= 8;
    return c;
}

static int nbits(s32 v)                      /* bits significativos (busqueda binaria) */
{
    int n = 0;
    if (v < 0) v = -v;
    if (v >> 16) { v >>= 16; n = 16; }
    if (v >> 8) { v >>= 8; n += 8; }
    if (v >> 4) { v >>= 4; n += 4; }
    if (v >> 2) { v >>= 2; n += 2; }
    if (v >> 1) { v >>= 1; n += 1; }
    return n + (int)v;
}

/* interpolacion segura: a + (b-a)*num/den sin desbordar 32 bits */
static s32 lerp_div(s32 a, s32 b, s32 num, s32 den)
{
    s32 d = b - a;
    int k = 0;
    s32 q;
    if (den == 0) return a;
    q = mdq(d, num, den);                    /* via rapida: MULS + DIVS */
    if (q != MDQ_BIG) return a + q;
    while (nbits(d) + nbits(num) > 30) {
        if (ABS(num) > ABS(d)) { num >>= 1; den >>= 1; if (den == 0) return a; }
        else { d >>= 1; k++; }
    }
    return a + (((d * num) / den) << k);
}

/* recorta el segmento a la ventana de la superficie; 0 si queda fuera */
static int line_clip(const Surface *s, s32 *px0, s32 *py0, s32 *px1, s32 *py1)
{
    s32 x0 = *px0, y0 = *py0, x1 = *px1, y1 = *py1, x, y;
    int c0 = outcode(s, x0, y0), c1 = outcode(s, x1, y1), co, guard = 0;
    while (c0 | c1) {
        if (c0 & c1) return 0;
        if (++guard > 8) return 0;
        co = c0 ? c0 : c1;
        if (co & 4)      { y = s->cy0; x = lerp_div(x0, x1, y - y0, y1 - y0); }
        else if (co & 8) { y = s->cy1; x = lerp_div(x0, x1, y - y0, y1 - y0); }
        else if (co & 1) { x = s->cx0; y = lerp_div(y0, y1, x - x0, x1 - x0); }
        else             { x = s->cx1; y = lerp_div(y0, y1, x - x0, x1 - x0); }
        if (co == c0) { x0 = x; y0 = y; c0 = outcode(s, x0, y0); }
        else          { x1 = x; y1 = y; c1 = outcode(s, x1, y1); }
    }
    *px0 = x0; *py0 = y0; *px1 = x1; *py1 = y1;
    return 1;
}

void gfx_line(Surface *s, s32 x0, s32 y0, s32 x1, s32 y1, int c)
{
    if (line_clip(s, &x0, &y0, &x1, &y1)) gfx_line_raw(s->base, x0, y0, x1, y1, c);
}

/* igual con OR exclusivo (dos veces la misma linea la borra, sin tocar lo que hubiera debajo) */
void gfx_line_xor_s(Surface *s, s32 x0, s32 y0, s32 x1, s32 y1, int c)
{
    if (line_clip(s, &x0, &y0, &x1, &y1)) gfx_line_xor(s->base, x0, y0, x1, y1, c);
}

/* --- poligono: recorte Sutherland-Hodgman 2D + relleno por scanline --- */
#define PMAX 24
static int clip_edge(const s32 *in, int n, s32 *out, int edge, s32 lim)
{
    int i, m = 0, ia, ib;
    s32 ax, ay, bx, by, va, vb;
    const s32 *pb;
    pb = in;                                 /* b = vertice siguiente (el ultimo vuelve al primero) */
    for (i = 0; i < n; i++) {
        ax = in[2 * i]; ay = in[2 * i + 1];
        if (i + 1 == n) pb = in; else pb = in + 2 * (i + 1);
        bx = pb[0]; by = pb[1];
        va = (edge & 1) ? ay : ax;
        vb = (edge & 1) ? by : bx;
        if (edge >= 2) { ia = va <= lim; ib = vb <= lim; }
        else { ia = va >= lim; ib = vb >= lim; }
        if (ia && m < PMAX) { out[2 * m] = ax; out[2 * m + 1] = ay; m++; }
        if (ia != ib && m < PMAX) {
            if (edge & 1) {
                out[2 * m] = lerp_div(ax, bx, lim - ay, by - ay);
                out[2 * m + 1] = lim;
            } else {
                out[2 * m] = lim;
                out[2 * m + 1] = lerp_div(ay, by, lim - ax, bx - ax);
            }
            m++;
        }
    }
    return m;
}

void gfx_poly32(Surface *s, int n, const s32 *xy, Fill f);

void gfx_poly(Surface *s, int n, const s16 *xy, Fill f)
{
    s32 b[PMAX * 2];
    int i;
    if (n > PMAX) n = PMAX;
    for (i = 0; i < 2 * n; i++) b[i] = xy[i];
    gfx_poly32(s, n, b, f);
}

/* --- relleno de poligonos convexos (lo habitual): cada arista se recorre una vez y deja los limites de cada fila --- */
#define ROWS_MAX 256
s16 rmin_[ROWS_MAX], rmax_[ROWS_MAX];       /* exportados: los usa gfx_fill_convex en ensamblador */
u8 rcnt_[ROWS_MAX];

#ifndef USE_ASM_POLY
static int is_convex(const s32 *p, int m)
{
    int i, j, k, sgn = 0;
    s32 c;
    for (i = 0; i < m; i++) {
        j = i + 1; if (j == m) j = 0;
        k = j + 1; if (k == m) k = 0;
        c = MULS16(p[2 * j] - p[2 * i], p[2 * k + 1] - p[2 * j + 1]) - MULS16(p[2 * j + 1] - p[2 * i + 1], p[2 * k] - p[2 * j]);
        if (c > 0) { if (sgn < 0) return 0; sgn = 1; }
        else if (c < 0) { if (sgn > 0) return 0; sgn = -1; }
    }
    return 1;
}
#endif

#ifndef USE_ASM_POLY
/* poligono convexo ya recortado a la ventana; devuelve 0 (sin dibujar) si no es convexo. Mismas reglas de fila y de redondeo
 * que el camino general (x en el centro de la fila, la ultima fila incluye el extremo) */
int gfx_fill_convex(u8 *base, const s32 *p, int m, s32 fill)
{
    int i, j, y, ymin = 32767, ymax = -32768, y0, y1, ye;
    s32 x0, x1, ex, edx, t;
    u8 *row;
    Fill f = (Fill)fill;
    if (!is_convex(p, m)) return 0;
    for (i = 0; i < m; i++) {
        y = (int)p[2 * i + 1];
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
    }
    if (ymin < 0 || ymax >= ROWS_MAX) return 1;
    for (y = ymin; y <= ymax; y++) rcnt_[y] = 0;
    for (i = 0; i < m; i++) {
        j = i + 1; if (j == m) j = 0;
        x0 = p[2 * i]; y0 = (int)p[2 * i + 1];
        x1 = p[2 * j]; y1 = (int)p[2 * j + 1];
        if (y0 == y1) continue;
        if (y0 > y1) { t = x0; x0 = x1; x1 = t; y = y0; y0 = y1; y1 = y; }
        edx = ((x1 - x0) << 16) / (y1 - y0);
        ex = (x0 << 16) + 32768L;
        ye = y1 == ymax ? y1 : y1 - 1;                 /* la ultima fila del poligono incluye el extremo inferior */
        for (y = y0; y <= ye; y++) {
            t = ex >> 16;
            if (rcnt_[y] == 0) { rmin_[y] = (s16)t; rmax_[y] = (s16)t; }
            else if (t < rmin_[y]) rmin_[y] = (s16)t;
            else if (t > rmax_[y]) rmax_[y] = (s16)t;
            rcnt_[y]++;
            ex += edx;
        }
    }
    row = base + (s32)ymin * SCR_STRIDE;
    for (y = ymin; y <= ymax; y++, row += SCR_STRIDE)
        if (rcnt_[y] >= 2) gfx_hspan_raw(row, rmin_[y], rmax_[y], (s32)((y & 1) ? (f & 0xFFFFUL) : (f >> 16)));
    return 1;
}
#endif

void gfx_poly32(Surface *s, int n, const s32 *xy, Fill f)
{
    s32 a[PMAX * 2], b[PMAX * 2];
    s32 ex[PMAX], edx[PMAX], xs[PMAX];
    s16 ey0[PMAX], ey1[PMAX];
    int i, j, m, ne, ymin, ymax, y, k, inside = 1;
    s32 x0, y0, x1, y1, t, lx = 32767, hx = -32768, ly = 32767, hy = -32768;
    const s32 *src;

    if (n < 3) return;
    if (n > PMAX) n = PMAX;
    /* caja del poligono: fuera del todo -> nada; dentro del todo -> sin recorte */
    for (i = 0; i < n; i++) {
        x0 = xy[2 * i]; y0 = xy[2 * i + 1];
        if (x0 < lx) lx = x0;
        if (x0 > hx) hx = x0;
        if (y0 < ly) ly = y0;
        if (y0 > hy) hy = y0;
    }
    if (hx < s->cx0 || lx > s->cx1 || hy < s->cy0 || ly > s->cy1) return;
    if (lx < s->cx0 || hx > s->cx1 || ly < s->cy0 || hy > s->cy1) inside = 0;
    if (inside) { src = xy; m = n; }
    else {
        m = clip_edge(xy, n, a, 0, s->cx0);    /* x >= cx0 */
        m = clip_edge(a, m, b, 1, s->cy0);     /* y >= cy0 */
        m = clip_edge(b, m, a, 2, s->cx1);     /* x <= cx1 */
        m = clip_edge(a, m, b, 3, s->cy1);     /* y <= cy1 */
        src = b;
    }
    if (m < 3) return;
    if (m <= PMAX && gfx_fill_convex(s->base, src, m, (s32)f)) return;

    /* tabla de aristas: x en Q16 en el centro de la fila */
    ne = 0; ymin = 32767; ymax = -32768;
    for (i = 0; i < m; i++) {
        x0 = src[2 * i]; y0 = src[2 * i + 1];
        j = i + 1; if (j == m) j = 0;
        x1 = src[2 * j]; y1 = src[2 * j + 1];
        if (y0 == y1) continue;
        if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
        edx[ne] = ((x1 - x0) << 16) / (y1 - y0);
        ex[ne] = (x0 << 16) + 32768L;
        ey0[ne] = (s16)y0; ey1[ne] = (s16)y1;
        if (y0 < ymin) ymin = (int)y0;
        if (y1 > ymax) ymax = (int)y1;
        ne++;
    }
    /* ex[i] pasa a ser la x de la fila actual: se avanza edx una vez por fila (sin multiplicar) */
    for (y = ymin; y <= ymax; y++) {
        k = 0;
        for (i = 0; i < ne; i++) {
            if (y >= ey0[i] && (y < ey1[i] || (y == ymax && y == ey1[i]))) {
                if (y > ey0[i]) ex[i] += edx[i];
                xs[k++] = ex[i] >> 16;
            }
        }
        /* orden por insercion (pocos elementos) */
        for (i = 1; i < k; i++) {
            t = xs[i]; j = i - 1;
            while (j >= 0 && xs[j] > t) { xs[j + 1] = xs[j]; j--; }
            xs[j + 1] = t;
        }
        for (i = 0; i + 1 < k; i += 2)
            gfx_hspan(s, y, (int)xs[i], (int)xs[i + 1], f);
    }
}

void gfx_ellipse(Surface *s, int cx, int cy, int rx, int ry, int c)
{
    int i;
    s32 px, py, x, y;
    px = cx + rx; py = cy;
    for (i = 1; i <= 32; i++) {
        x = cx + ((MULS16(icos((u16)(i << 11)), rx) + 8192) >> 14);
        y = cy + ((MULS16(isin((u16)(i << 11)), ry) + 8192) >> 14);
        gfx_line(s, px, py, x, y, c);
        px = x; py = y;
    }
}

void gfx_char(Surface *s, int x, int y, int ch, int c)
{
    const u8 *g;
    int r, k;
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 95) ch = '?';
    g = font5x7 + (ch - 32) * 7;
    for (r = 0; r < 7; r++)
        for (k = 0; k < 5; k++)
            if (g[r] & (0x10 >> k)) gfx_pixel(s, x + k, y + r, c);
}

void gfx_text(Surface *s, int x, int y, const char *t, int c)
{
    int n = 0;
    while (t[n]) n++;
    /* camino rapido (rutina por palabras) si todo el texto cabe en la ventana; si no, punto a punto con recorte */
    if (n && x >= s->cx0 && x + n * 6 - 6 + 4 <= s->cx1 && y >= s->cy0 && y + 6 <= s->cy1) {
        gfx_text_raw(s->base + (s32)y * SCR_STRIDE, x, t, c);
        return;
    }
    while (*t) { gfx_char(s, x, y, *t++, c); x += 6; }
}

int gfx_textw(const char *t)
{
    int n = 0;
    while (*t++) n++;
    return n * 6;
}

void gfx_copy_rows(u8 *dst, const u8 *src, int bytes)
{
    gfx_memcpy(dst, src, bytes);
}

void gfx_copy_box(u8 *dst, const u8 *src, int x0, int y0, int x1, int y1)
{
    int y, b0 = (x0 >> 3) << 1, n = (((x1 >> 3) << 1) + 2) - b0;
    s32 off;
    for (y = y0; y <= y1; y++) {
        off = (s32)y * SCR_STRIDE + b0;
        gfx_memcpy(dst + off, src + off, n);
    }
}
