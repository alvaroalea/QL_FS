/* panel.c - instrumentos. Fondo estatico guardado en RAM: cada instrumento se
 * restaura desde esa copia limpia (solo su caja) y luego se pintan agujas. */
#include "panel.h"
#include "util.h"
#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define RX 40
#define RY 27
#define ROW1 130
#define ROW2 191
#define COL1 52
#define COL2 144
#define COL3 236
#define COL4 328
#define RB_X 376               /* bloque derecho */
#define GEAR_LV_X 468          /* palanca del tren: guia vertical de 7 px y cuadrado de 3x3 */
#define GEAR_LV_Y0 166
#define GEAR_LV_Y1 172
#define MSG_Y 229
#define STAT_Y 241

static u8 *clean;              /* copia del panel limpio (filas PANEL_Y0..255) */
static Surface S;
static const AircraftDef *AC;
static int asi_max;
static s16 hw_tab[RY + 1];     /* semiancho del horizonte artificial por fila */
#ifdef AI_DIFF
/* Lo que hay pintado en cada fila: color de la parte izquierda (0 cielo, 1 tierra, 2 = sin pintar) y la columna donde empieza la derecha.
 * Al cambiar la actitud solo se repintan los pixeles entre la frontera vieja y la nueva (y las filas enteras si cambia el color). */
static u8 ai_pl[2 * RY + 1];
static s16 ai_lx0[2 * RY + 1], ai_lx1[2 * RY + 1];   /* hueco libre de cada fila dentro del aro (de la copia limpia del panel) */
static s16 ai_pb[2 * RY + 1];
static int ai_touch;                 /* se ha pintado algo en las filas del avion de referencia */
#endif

/* Rosa del direccional: el patron de rayitas se repite cada 10 grados, asi que bastan GYN copias del fondo (anillo + rayitas
 * cortas + centro negro) giradas de 1 en 1 grado. Al cambiar el rumbo se vuelca la copia (rumbo % 10) y encima solo se pintan
 * las 12 rayitas largas y los numeros. */
#ifndef GYN
#define GYN 30                 /* 30 copias: las rayitas largas (cada 30 grados) tambien van en el fondo; con 10 se pintan encima */
#endif
#define GY_X0 (COL2 - RX - 2)
#define GY_Y0 (ROW2 - RY - 1)
#define GY_Y1 (ROW2 + RY + 1)
#define GY_B0 (((GY_X0) >> 3) << 1)
#define GY_N  (((((COL2 + RX + 2) >> 3) << 1) + 2) - GY_B0)
#define GY_ROWS (GY_Y1 - GY_Y0 + 1)
#define GY_SZ ((s32)GY_ROWS * GY_N)
static u8 *gyro;               /* GYN copias de GY_ROWS filas x GY_N bytes */

static void dpt(int cx, int cy, u16 a, int k, s32 *x, s32 *y)
{
    *x = cx + ((s32)isin(a) * (RX * k >> 8) >> 14);
    *y = cy - ((s32)icos(a) * (RY * k >> 8) >> 14);
}

static void tick(int cx, int cy, u16 a, int k0, int k1, int c)
{
    s32 x0, y0, x1, y1;
    dpt(cx, cy, a, k0, &x0, &y0);
    dpt(cx, cy, a, k1, &x1, &y1);
    gfx_line(&S, x0, y0, x1, y1, c);
}

static void label_at(int cx, int cy, u16 a, int k, const char *t, int c)
{
    s32 x, y;
    dpt(cx, cy, a, k, &x, &y);
    gfx_text(&S, (int)x - gfx_textw(t) / 2 + 1, (int)y - 3, t, c);
}

static void ctext(int cx, int y, const char *t, int c)
{
    gfx_text(&S, cx - gfx_textw(t) / 2 + 1, y, t, c);
}

static void ring(int cx, int cy)
{
    gfx_ellipse(&S, cx, cy, RX, RY, C_WHITE);
    gfx_ellipse(&S, cx, cy, RX + 1, RY, C_WHITE);
}

static void restore(int cx, int cy)
{
    gfx_copy_box(S.base, clean - (s32)PANEL_Y0 * SCR_STRIDE,
                 cx - RX - 2, cy - RY - 1, cx + RX + 2, cy + RY + 1);
}

static void restore_box(int x0, int y0, int x1, int y1)
{
    gfx_copy_box(S.base, clean - (s32)PANEL_Y0 * SCR_STRIDE, x0, y0, x1, y1);
}

/* ---------------------------------------------------------- angulos de escala */
static u16 asi_ang(s32 kt) { return (u16)(-29127L + kt * 58254L / asi_max); }
static u16 vsi_ang(s32 fpm) { fpm = CLAMP(fpm, -2100, 2100); return (u16)(49152L + fpm * 15474L / 1000); }
static u16 rpm_ang(s32 r, s32 max) { return (u16)(-24576L + r * 49152L / max); }

static void panel_reset_state(void);

/* ---------------------------------------------------------- fondo estatico
 * El fondo (anillos, escalas, etiquetas) se dibuja una sola vez, en el PC, con tools/mkpanel.c: sale un volcado crudo de la
 * pantalla (32 KB) por avion (data/xxx.src) que el simulador carga al arrancar. */
/* Palancas con barra (T gases, P paso, M mezcla, F flaps): solo las que tiene el avion, en columnas consecutivas */
static int slot_x[4];                  /* x de la barra, -1 = el avion no la tiene */
static const char slot_ch[4] = { 'T', 'P', 'M', 'F' };

static void panel_setup(const AircraftDef *ac)
{
    int i, n = 0, has[4];
    AC = ac;
    has[0] = ac->eng_type != ENG_NONE;
    has[1] = ac->eng_type != ENG_NONE && ac->prop_type == PROP_CS;
    has[2] = ac->eng_type == ENG_PISTON;
    has[3] = ac->n_flaps > 1;
    for (i = 0; i < 4; i++) slot_x[i] = has[i] ? RB_X + 1 + 12 * n++ : -1;
    for (i = 0; i <= RY; i++) hw_tab[i] = (s16)(isqrt32((u32)(RY * RY - i * i) * 256 * 9 / 4) * 1 / 16 - 1);
    asi_max = ((ac->vne ? ac->vne : 160) * 6 / 5 + 19) / 20 * 20;
}

#ifdef HOST
void panel_build(u8 *scr, const AircraftDef *ac)
{
    int i, k;
    char b[8];
    s32 v;
    panel_setup(ac);
    gfx_init_surface(&S, scr, SCR_H);
    gfx_rect(&S, 0, PANEL_Y0 - 2, SCR_W - 1, SCR_H - 1, fill_tab[F_BLACK]);
    gfx_rect(&S, 0, PANEL_Y0 - 2, SCR_W - 1, PANEL_Y0 - 2, fill_tab[F_RED]);
    gfx_rect(&S, 0, MSG_Y - 3, SCR_W - 1, MSG_Y - 3, fill_tab[F_RED]);

    /* ASI */
    ring(COL1, ROW1);
    for (v = 0; v <= asi_max; v += (asi_max > 250 ? 50 : 20))
        tick(COL1, ROW1, asi_ang(v), 200, 256, C_WHITE);
    for (v = ac->vs0; v <= ac->vfe; v += 2) tick(COL1, ROW1, asi_ang(v), 232, 246, C_WHITE);
    for (v = ac->vs1; v <= ac->vno; v += 2) tick(COL1, ROW1, asi_ang(v), 210, 228, C_GREEN);
    for (v = ac->vno; v <= ac->vne; v += 2) tick(COL1, ROW1, asi_ang(v), 210, 228, C_RED);
    tick(COL1, ROW1, asi_ang(ac->vne), 180, 256, C_RED);
    for (v = (asi_max > 250 ? 100 : 40); v < asi_max; v += (asi_max > 250 ? 100 : 40)) {
        u_num(b, v, 1, ' ');
        label_at(COL1, ROW1, asi_ang(v), 150, b, C_WHITE);
    }
    ctext(COL1, ROW1 + 8, "KT", C_GREEN);

    /* horizonte artificial: solo el anillo */
    ring(COL2, ROW1);

    /* altimetro */
    ring(COL3, ROW1);
    for (i = 0; i < 50; i++) tick(COL3, ROW1, (u16)(i * 1311L), i % 5 ? 236 : 200, 256, C_WHITE);
    for (i = 0; i < 10; i++) { b[0] = (char)('0' + i); b[1] = 0; label_at(COL3, ROW1, (u16)(i * 6554L), 160, b, C_WHITE); }
    ctext(COL3, ROW1 - 14, "ALT", C_GREEN);

    /* RPM / N1 */
    ring(COL4, ROW1);
    if (ac->eng_type == ENG_JET) {
        for (i = 0; i <= 110; i += 10) tick(COL4, ROW1, rpm_ang(i, 110), i % 20 ? 226 : 196, 256, C_WHITE);
        for (i = 20; i <= 100; i += 40) { u_num(b, i, 1, ' '); label_at(COL4, ROW1, rpm_ang(i, 110), 150, b, C_WHITE); }
        tick(COL4, ROW1, rpm_ang(104, 110), 170, 256, C_RED);
        ctext(COL4, ROW1 + 8, "N1 %", C_GREEN);
    } else if (ac->eng_type == ENG_PISTON) {
        for (i = 0; i <= 35; i++) tick(COL4, ROW1, rpm_ang(i, 35), i % 5 ? 232 : 196, 256, C_WHITE);
        for (i = 0; i <= 30; i += 10) { u_num(b, i, 1, ' '); label_at(COL4, ROW1, rpm_ang(i, 35), 150, b, C_WHITE); }
        k = (int)(ac->rpm_max >> 16);
        tick(COL4, ROW1, rpm_ang(k / 100, 35), 170, 256, C_RED);
        ctext(COL4, ROW1 + 8, "RPM", C_GREEN);
    } else {
        ctext(COL4, ROW1 - 3, "PLANEADOR", C_GREEN);
    }

    /* coordinador de giro */
    ring(COL1, ROW2);
    tick(COL1, ROW2, (u16)(16384 + 2731), 200, 256, C_WHITE);
    tick(COL1, ROW2, (u16)(49152U - 2731), 200, 256, C_WHITE);
    tick(COL1, ROW2, 16384, 200, 256, C_WHITE);
    tick(COL1, ROW2, 49152U, 200, 256, C_WHITE);
    gfx_text(&S, COL1 - 36, ROW2 + 4, "L", C_WHITE);
    gfx_text(&S, COL1 + 32, ROW2 + 4, "R", C_WHITE);
    gfx_line(&S, COL1 - 14, ROW2 + 13, COL1 + 14, ROW2 + 13, C_WHITE);
    gfx_line(&S, COL1 - 14, ROW2 + 19, COL1 + 14, ROW2 + 19, C_WHITE);
    gfx_line(&S, COL1 - 4, ROW2 + 13, COL1 - 4, ROW2 + 19, C_WHITE);
    gfx_line(&S, COL1 + 4, ROW2 + 13, COL1 + 4, ROW2 + 19, C_WHITE);

    /* direccional: solo anillo y linea de fe */
    ring(COL2, ROW2);
    gfx_line(&S, COL2, ROW2 - RY - 2, COL2, ROW2 - RY + 5, C_RED);
    gfx_line(&S, COL2 + 1, ROW2 - RY - 2, COL2 + 1, ROW2 - RY + 5, C_RED);

    /* variometro */
    ring(COL3, ROW2);
    for (i = -2000; i <= 2000; i += 500) tick(COL3, ROW2, vsi_ang(i), (i % 1000) ? 226 : 196, 256, C_WHITE);
    label_at(COL3, ROW2, vsi_ang(0), 160, "0", C_WHITE);
    label_at(COL3, ROW2, vsi_ang(1000), 160, "1", C_WHITE);
    label_at(COL3, ROW2, vsi_ang(-1000), 160, "1", C_WHITE);
    label_at(COL3, ROW2, vsi_ang(2000), 160, "2", C_WHITE);
    ctext(COL3 + 8, ROW2 + 8, "VS", C_GREEN);

    /* CDI / ILS */
    ring(COL4, ROW2);
    for (i = -5; i <= 5; i++)
        if (i) gfx_pixel(&S, COL4 + i * 6, ROW2, C_WHITE), gfx_pixel(&S, COL4 + i * 6 + 1, ROW2, C_WHITE);
    for (i = -4; i <= 4; i++)
        if (i) gfx_pixel(&S, COL4, ROW2 + i * 4, C_WHITE);
    gfx_ellipse(&S, COL4, ROW2, 4, 3, C_WHITE);

    /* bloque derecho: etiquetas de palancas */
    for (i = 0; i < 4; i++)
        if (slot_x[i] >= 0) { char l[2]; l[0] = slot_ch[i]; l[1] = 0; gfx_text(&S, slot_x[i], 215, l, C_GREEN); }
    if (ac->retract) {
        gfx_text(&S, 434, 176, "TREN", C_GREEN);
        gfx_line(&S, GEAR_LV_X, GEAR_LV_Y0, GEAR_LV_X, GEAR_LV_Y1, C_WHITE);     /* guia de la palanca del tren */
    }
}
#endif

/* "flp1_mono_acf" / "data/mono.acf" -> "flp1_mono_src" / "data/mono.src" */
static void src_path(char *out, const char *acf)
{
    int n = (int)strlen(acf);
    strcpy(out, acf);
    if (n >= 3 && !strcmp(out + n - 3, "acf")) strcpy(out + n - 3, "src");
    else strcat(out, ".src");
}

static int panel_load(u8 *scr, const char *acf)
{
    char path[80];
    FILE *fp;
    int ok;
    src_path(path, acf);
    fp = fopen(path, "rb");
    if (!fp) return 0;
    ok = fread(scr + (s32)PANEL_SRC_Y0 * SCR_STRIDE, 1, (size_t)PANEL_SRC_BYTES, fp) == (size_t)PANEL_SRC_BYTES;
    fclose(fp);
    return ok;
}

static void gyro_tick_set(int k)                 /* rayitas cortas (y largas, si GYN = 30) con la rosa girada k grados */
{
    int i;
    for (i = 0; i < 36; i++)
        tick(COL2, ROW2, (u16)(((i * 10L - k + 360) % 360) * 65536L / 360), 226, 250, C_WHITE);
#if GYN == 30
    for (i = 0; i < 12; i++)
        tick(COL2, ROW2, (u16)(((30L * i - k + 360) % 360) * 65536L / 360), 196, 226, C_WHITE);
#endif
    gfx_rect(&S, COL2 - 10, ROW2 - 4, COL2 + 10, ROW2 + 4, fill_tab[F_BLACK]);
}

/* Las GYN copias se dibujan fuera de la pantalla, en el buffer de la vista 3D (work: VIEW_H filas en RAM alta, sin contencion y
 * sin que se vea el dibujo): una superficie con la base desplazada para que las filas GY_Y0.. caigan al principio del buffer. */
static void gyro_build(u8 *work)
{
    int k, y;
    Surface keep = S, W;
    if (!work || !clean || GY_ROWS > VIEW_H) return;
    if (!gyro) gyro = (u8 *)plat_alloc_hi(GY_SZ * GYN);
    if (!gyro) return;
    gfx_init_surface(&W, work - (s32)GY_Y0 * SCR_STRIDE, GY_Y1 + 1);
    gfx_clip(&W, 0, GY_Y0, SCR_W - 1, GY_Y1);
    S = W;                                       /* tick() y gyro_tick_set() pintan en S */
    for (k = 0; k < GYN; k++) {
        gfx_copy_box(S.base, clean - (s32)PANEL_Y0 * SCR_STRIDE, GY_X0, GY_Y0, COL2 + RX + 2, GY_Y1);
        gyro_tick_set(k);
        for (y = GY_Y0; y <= GY_Y1; y++)
            gfx_memcpy(gyro + k * GY_SZ + (s32)(y - GY_Y0) * GY_N, S.base + (s32)y * SCR_STRIDE + GY_B0, GY_N);
    }
    S = keep;
    memset(work, 0, (size_t)VIEW_H * SCR_STRIDE);
}

#ifdef AI_DIFF
/* De la copia limpia del panel: por cada fila del horizonte, el tramo libre desde el centro hasta el primer punto del aro (o de lo que haya) */
static int clean_px(int x, int y)
{
    const u8 *p = clean + (s32)(y - PANEL_Y0) * SCR_STRIDE + ((x >> 3) << 1);
    return ((p[0] | p[1]) & (0x80 >> (x & 7))) != 0;
}

static void ai_limits(void)
{
    int r, y, x;
    if (!clean) return;
    for (r = 0; r <= 2 * RY; r++) {
        y = ROW1 - RY + r;
        x = COL2;
        if (clean_px(x, y)) { ai_lx0[r] = 1; ai_lx1[r] = 0; continue; }
        for (x = COL2 - 1; x > COL2 - RX - 3 && !clean_px(x, y); x--) ;
        ai_lx0[r] = (s16)(x + 1);
        for (x = COL2 + 1; x < COL2 + RX + 3 && !clean_px(x, y); x++) ;
        ai_lx1[r] = (s16)(x - 1);
    }
}
#endif

void panel_init(u8 *scr, const AircraftDef *ac, const char *acf, u8 *work)
{
    panel_setup(ac);
    gfx_init_surface(&S, scr, SCR_H);
    if (!panel_load(scr, acf)) {
#ifdef HOST
        panel_build(scr, ac);                    /* sin .src: se dibuja (solo PC) */
#else
        gfx_rect(&S, 0, PANEL_Y0 - 2, SCR_W - 1, SCR_H - 1, fill_tab[F_BLACK]);
        gfx_text(&S, 4, PANEL_Y0 + 4, "FALTA EL FICHERO DEL PANEL (.SRC)", C_RED);
#endif
    }
    if (!clean) clean = (u8 *)plat_alloc_hi((long)(SCR_H - PANEL_Y0) * SCR_STRIDE);   /* RAM alta: se lee a menudo */
    if (clean) gfx_memcpy(clean, scr + (s32)PANEL_Y0 * SCR_STRIDE, (s32)(SCR_H - PANEL_Y0) * SCR_STRIDE);
    gyro_build(work);
#ifdef AI_DIFF
    ai_limits();
#endif
    panel_reset_state();                 /* nada de lo dinamico esta pintado todavia */
}

/* ---------------------------------------------------------- estado mostrado
 * Cada instrumento recuerda lo ultimo que pinto y solo se toca la pantalla si el valor (cuantizado) cambia. Las agujas
 * se dibujan con OR exclusivo: dibujar otra vez la misma aguja la borra sin tocar lo que hay debajo. Orden obligado en
 * cada instrumento: 1) borrar la aguja vieja, 2) cambiar el texto (restore_box) y 3) dibujar la nueva; si no, el texto
 * se pintaria encima de una aguja a medias y al deshacerla quedarian puntos sueltos. */
#define NONE (-30000)
static int s_kt, s_alt, s_rpm[2], s_rpmt, s_vsi, s_bank, s_ball, s_hdg, s_ai_b, s_ai_h, s_cdi_x, s_cdi_y, s_cdi_col;
static int s_cdi_loc, s_cdi_flag, s_cdi_obs, s_bar[7], s_gear, s_lever, s_marker, s_stopped;
static int s_up_on;                    /* tren recogido y bloqueado: luces rojas hasta pasar unos segundos */
static u32 s_up_t;
static char s_rad[6][40], s_eng[4][24], s_msg[84], s_stat[84];
static int s_warn, s_fps = -3;
static u16 s_asi_a, s_alt_a1, s_alt_a2, s_rpm_a[2], s_vsi_a;

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void xline(int x0, int y0, int x1, int y1, int c)
{
    gfx_line_xor_s(&S, x0, y0, x1, y1, c);
}

/* aguja gruesa (dos lineas contiguas, sin solaparse: si se solaparan el OR exclusivo las anularia) */
static void xneedle(int cx, int cy, u16 a, int k, int c)
{
    s32 x, y, dx, dy;
    dpt(cx, cy, a, k, &x, &y);
    dx = x - cx; dy = y - cy;
    if (ABS(dx) >= ABS(dy)) { xline(cx, cy, (int)x, (int)y, c); xline(cx, cy + 1, (int)x, (int)y + 1, c); }
    else { xline(cx, cy, (int)x, (int)y, c); xline(cx + 1, cy, (int)x + 1, (int)y, c); }
}

static void xrect(int x0, int y0, int x1, int y1, int c)
{
    int y;
    for (y = y0; y <= y1; y++) xline(x0, y, x1, y, c);
}

static void panel_reset_state(void)
{
    int i;
    s_kt = s_alt = s_rpmt = s_vsi = s_bank = s_ball = s_hdg = s_ai_b = s_ai_h = s_cdi_x = s_cdi_y = NONE;
    s_rpm[0] = s_rpm[1] = NONE;
    s_cdi_loc = s_cdi_flag = s_cdi_obs = s_cdi_col = s_gear = s_lever = s_marker = s_stopped = NONE;
    for (i = 0; i < 7; i++) s_bar[i] = NONE;
    s_up_on = 0;
    for (i = 0; i < 6; i++) s_rad[i][0] = 1;       /* distinto de cualquier cadena real: se pinta la primera vez */
    for (i = 0; i < 4; i++) s_eng[i][0] = 1;
    s_msg[0] = 1; s_stat[0] = 1; s_warn = NONE; s_fps = -3;
#ifdef AI_DIFF
    memset(ai_pl, 2, sizeof ai_pl);
    ai_touch = 1;
#endif
}

/* ---------------------------------------------------------- instrumentos */
static void draw_asi(const Flight *f)
{
    s32 kt = MS2KT(f->ias) >> 16;
    char b[8];
    if (kt == s_kt) return;
    if (s_kt != NONE) xneedle(COL1, ROW1, s_asi_a, 230, C_WHITE);
    u_num(b, kt, 3, ' ');
    restore_box(COL1 - 10, ROW1 + 15, COL1 + 10, ROW1 + 24);
    gfx_text(&S, COL1 - 8, ROW1 + 16, b, C_WHITE);
    s_asi_a = asi_ang(MIN(kt, asi_max));
    xneedle(COL1, ROW1, s_asi_a, 230, C_WHITE);
    s_kt = (int)kt;
}

/* Horizonte artificial en tres tiempos, para poder repartirlo entre varios turnos de 20 ms (panel_job_step):
 * ai_begin fija actitud y colores, ai_rows pinta un rango de filas y ai_end el avion de referencia y el aro. */
static s32 ai_sa, ai_ca, ai_h;
#ifdef AI_INCR
static s32 ai_K, ai_D;
#endif
static Fill ai_sky, ai_gnd;

static void ai_begin(const Flight *f)
{
    u16 bb = (u16)fxmul(f->phi, RAD2BAM);
    ai_sa = isin(bb); ai_ca = icos(bb);
    ai_sky = fill_tab[F_WHITE]; ai_gnd = fill_tab[F_BROWN];
    ai_h = fxmul(f->the, 57L * 65536L) * RY / 25 >> 16;         /* grados -> pixeles */
    ai_h = CLAMP(ai_h, -RY * 2, RY * 2);
#ifdef AI_INCR
    ai_D = 3L * ai_ca;                                          /* el horizonte es una recta: xb(y) crece K/256 pixeles por fila (K = 3 ca / (2 sa) en Q8) */
    ai_K = ABS(ai_sa) >= 64 ? (3L * ai_ca * 128L) / ai_sa : 0;  /* la unica division del horizonte */
#endif
}

#ifdef AI_DIFF
static void ai_rows(int ya, int yb)
{
    s32 sa = ai_sa, B, xb, K = ai_K, D = ai_D, xq;
    int y, x0, x1, cx = COL2, cy = ROW1, lc, b, r, pl, pb;
    xq = (s32)cx * 256 - (s32)(ya - cy - ai_h) * K;
    B = (s32)(ya - cy - ai_h) * D;
    for (y = ya; y <= yb; y++) {
        r = y - (cy - RY);
        x0 = ai_lx0[r]; x1 = ai_lx1[r];
        xb = xq >> 8; xq -= K;
        if (x1 < x0) { B += D; continue; }               /* fila cuyo centro es del aro: nada que pintar */
        if (ABS(sa) < 64) { lc = B < 0 ? 0 : 1; b = x1 + 1; B += D; }
        else {
            lc = sa > 0 ? 0 : 1;                        /* cielo a la izquierda si sa > 0 */
            b = (int)CLAMP(xb, (s32)x0, (s32)x1 + 1);
            if (b <= x0) { lc ^= 1; b = x1 + 1; }       /* toda la fila del color de la derecha: se guarda como un solo color */
        }
        pl = ai_pl[r]; pb = ai_pb[r];
        ai_pl[r] = (u8)lc; ai_pb[r] = (s16)b;
        if (pl == lc && pb == b) continue;
        if (y >= cy - 1 && y <= cy + 3) ai_touch = 1;
        if (pl == lc) {                                 /* mismo color a la izquierda: solo cambia el tramo entre las dos fronteras */
            if (b > pb) gfx_hspan(&S, y, pb, b - 1, lc ? ai_gnd : ai_sky);
            else gfx_hspan(&S, y, b, pb - 1, lc ? ai_sky : ai_gnd);
        } else {                                        /* fila nueva o con otro color a la izquierda: entera */
            gfx_hspan(&S, y, x0, b - 1, lc ? ai_gnd : ai_sky);
            if (b <= x1) gfx_hspan(&S, y, b, x1, lc ? ai_sky : ai_gnd);
        }
    }
}
#else
static void ai_rows(int ya, int yb)
{
    s32 sa = ai_sa, B, xb;
    int y, x0, x1, dy, w, cx = COL2, cy = ROW1;
#ifdef AI_INCR
    s32 K = ai_K, D = ai_D, xq = (s32)cx * 256 - (s32)(ya - cy - ai_h) * K;
    B = (s32)(ya - cy - ai_h) * D;                              /* 2 B (solo importa su signo) */
#endif
    for (y = ya; y <= yb; y++) {
        dy = y - cy;
        w = hw_tab[ABS(dy)];
        x0 = cx - w; x1 = cx + w;
#ifdef AI_INCR
        xb = xq >> 8; xq -= K;
        if (ABS(sa) < 64) { gfx_hspan(&S, y, x0, x1, B < 0 ? ai_sky : ai_gnd); B += D; continue; }
#else
        B = (dy - ai_h) * 3 * ai_ca / 2;      /* cielo si (x-cx)*sa + B < 0 */
        if (ABS(sa) < 64) {
            gfx_hspan(&S, y, x0, x1, B < 0 ? ai_sky : ai_gnd);
            continue;
        }
        xb = cx - B / sa;
#endif
        if (sa > 0) {                          /* cielo a la izquierda de xb */
            if (xb <= x0) gfx_hspan(&S, y, x0, x1, ai_gnd);
            else if (xb > x1) gfx_hspan(&S, y, x0, x1, ai_sky);
            else { gfx_hspan(&S, y, x0, (int)xb - 1, ai_sky); gfx_hspan(&S, y, (int)xb, x1, ai_gnd); }
        } else {
            if (xb <= x0) gfx_hspan(&S, y, x0, x1, ai_sky);
            else if (xb > x1) gfx_hspan(&S, y, x0, x1, ai_gnd);
            else { gfx_hspan(&S, y, x0, (int)xb - 1, ai_gnd); gfx_hspan(&S, y, (int)xb, x1, ai_sky); }
        }
    }
}

#endif

static void ai_plane_lines(void)
{
    int cx = COL2, cy = ROW1;
    /* avion de referencia */
    gfx_line(&S, cx - 24, cy, cx - 8, cy, C_RED);
    gfx_line(&S, cx - 8, cy, cx - 4, cy + 3, C_RED);
    gfx_line(&S, cx + 8, cy, cx + 24, cy, C_RED);
    gfx_line(&S, cx + 8, cy, cx + 4, cy + 3, C_RED);
    gfx_line(&S, cx - 24, cy + 1, cx - 8, cy + 1, C_RED);
    gfx_line(&S, cx + 8, cy + 1, cx + 24, cy + 1, C_RED);
    gfx_rect(&S, cx - 1, cy - 1, cx + 1, cy, fill_tab[F_RED]);
}

#ifdef AI_DIFF
/* El avion de referencia como mascara (5 filas x 7 grupos): se dibuja una vez con las lineas sobre un fondo de unos y desde entonces
 * se estampa (verde fuera, rojo dentro) en vez de trazar seis lineas y un rectangulo (7 ms) */
#define PL_Y0 (ROW1 - 1)
#define PL_C0 ((COL2 - 24) >> 3)
static u8 pl_m[5][7];
static int pl_ok;

static void ai_plane(void)
{
    int r, c;
    u8 *p, m;
    if (!pl_ok) {
        static u8 tmp[5 * SCR_STRIDE];
        Surface keep = S, W;
        memset(tmp, 0xFF, sizeof tmp);
        gfx_init_surface(&W, tmp - (s32)PL_Y0 * SCR_STRIDE, PL_Y0 + 5);
        gfx_clip(&W, 0, PL_Y0, SCR_W - 1, PL_Y0 + 4);
        S = W;
        ai_plane_lines();
        S = keep;
        for (r = 0; r < 5; r++)
            for (c = 0; c < 7; c++) pl_m[r][c] = (u8)~tmp[r * SCR_STRIDE + (PL_C0 + c) * 2];
        pl_ok = 1;
    }
    for (r = 0; r < 5; r++) {
        p = S.base + (s32)(PL_Y0 + r) * SCR_STRIDE + PL_C0 * 2;
        for (c = 0; c < 7; c++, p += 2) {
            m = pl_m[r][c];
            if (m) { p[0] &= (u8)~m; p[1] |= m; }
        }
    }
}
#else
static void ai_plane(void) { ai_plane_lines(); }
#endif

static void ai_end(void)
{
#ifdef AI_DIFF
    if (ai_touch) { ai_plane(); ai_touch = 0; }         /* el aro no se toca: el horizonte no llega a el */
#else
    ai_plane();
    ring(COL2, ROW1);
#endif
}

static void ai_paint(const Flight *f)
{
    ai_begin(f);
    ai_rows(ROW1 - RY + 1, ROW1 + RY - 1);
    ai_end();
}


/* horizonte: solo se repinta si cambia el alabeo (en grados) o el cabeceo (en pixeles) */
static void draw_ai(const Flight *f)
{
    int qb = (int)(((u16)fxmul(f->phi, RAD2BAM) + 91U) / 182U), qh;
    s32 hh = fxmul(f->the, 57L * 65536L) * RY / 25 >> 16;
    qh = (int)CLAMP(hh, -RY * 2, RY * 2);
    if (qb == s_ai_b && qh == s_ai_h) return;
    s_ai_b = qb; s_ai_h = qh;
    ai_paint(f);
}

static void draw_alt(const Flight *f)
{
    s32 ft = (M2FT(f->z) >> 8) / 10 * 10;     /* z esta en Q8 m; se muestra de 10 en 10 pies */
    char b[8];
    if (ft == s_alt) return;
    if (s_alt != NONE) { xneedle(COL3, ROW1, s_alt_a1, 140, C_GREEN); xneedle(COL3, ROW1, s_alt_a2, 230, C_WHITE); }
    gfx_rect(&S, COL3 - 16, ROW1 + 9, COL3 + 16, ROW1 + 17, fill_tab[F_BLACK]);
    u_num(b, ft, 5, ' ');
    gfx_text(&S, COL3 - 14, ROW1 + 10, b, C_WHITE);
    s_alt_a1 = (u16)((ft % 10000) * 65536L / 10000);
    s_alt_a2 = (u16)((ft % 1000) * 65536L / 1000);
    xneedle(COL3, ROW1, s_alt_a1, 140, C_GREEN);
    xneedle(COL3, ROW1, s_alt_a2, 230, C_WHITE);
    s_alt = (int)ft;
}

static void draw_rpm(const Flight *f)
{
    char b[8];
    int i, v[2], t, chg;
    if (AC->eng_type == ENG_NONE) return;
    v[0] = v[1] = 0;
    for (i = 0; i < AC->n_eng && i < 2; i++)
        v[i] = AC->eng_type == ENG_JET ? (int)MIN(fxmul(f->e[i].n1, FXI(100)) >> 16, 110)
                                       : (int)((f->e[i].rpm >> 16) / 10 * 10);
    t = AC->eng_type == ENG_JET ? (int)(fxmul(f->e[0].n1, FXI(100)) >> 16) : (int)((f->e[0].rpm >> 16) / 10 * 10);
    chg = t != s_rpmt;
    for (i = 0; i < AC->n_eng && i < 2; i++) if (v[i] != s_rpm[i]) chg = 1;
    if (!chg) return;
    for (i = AC->n_eng - 1; i >= 0; i--)                                /* 1) borra las agujas viejas */
        if (i < 2 && s_rpm[i] != NONE) xneedle(COL4, ROW1, s_rpm_a[i], i ? 190 : 230, i ? C_RED : C_WHITE);
    if (t != s_rpmt) {                                                  /* 2) texto */
        if (AC->eng_type == ENG_JET) u_num(b, t, 3, ' ');
        else u_num(b, t, 4, ' ');
        restore_box(COL4 - 13, ROW1 + 15, COL4 + 14, ROW1 + 24);
        gfx_text(&S, COL4 - 11, ROW1 + 16, b, C_WHITE);
        s_rpmt = t;
    }
    for (i = AC->n_eng - 1; i >= 0; i--) {                              /* 3) agujas nuevas */
        if (i >= 2) continue;
        s_rpm[i] = v[i];
        s_rpm_a[i] = AC->eng_type == ENG_JET ? rpm_ang(v[i], 110) : rpm_ang(MIN((s32)v[i], 3500), 3500);
        xneedle(COL4, ROW1, s_rpm_a[i], i ? 190 : 230, i ? C_RED : C_WHITE);
    }
}

/* barra del coordinador de giro: dos lineas contiguas y una marca central (OR exclusivo: la misma llamada borra) */
static void tc_draw(u16 a)
{
    s32 ca = icos(a), sa = isin(a), x = (ca * 26) >> 14, y = (sa * 17) >> 14;
    int cx = COL1, cy = ROW2 - 2;
    xline(cx - (int)x, cy + (int)y, cx + (int)x, cy - (int)y, C_WHITE);
    xline(cx - (int)x, cy + (int)y + 1, cx + (int)x, cy - (int)y + 1, C_WHITE);
    xline(cx, cy, cx - (int)((sa * 6) >> 14), cy - (int)((ca * 5) >> 14), C_WHITE);
}

static void draw_tc(const Flight *f)
{
    s32 bank = CLAMP(fxmul(f->turn, FXI(5)), -FXF(7, 10), FXF(7, 10));   /* 3 g/s -> 15 g */
    u16 a = (u16)(fxmul(bank, RAD2BAM) / 90 * 90);                       /* paso de medio grado */
    int bx = (int)CLAMP(COL1 + fxmul(f->slip, FXI(10)), COL1 - 12, COL1 + 12);
    if ((int)a != s_bank) {
        if (s_bank != NONE) tc_draw((u16)s_bank);
        tc_draw(a);
        s_bank = (int)a;
    }
    if (bx != s_ball) {                                                  /* bola: se mueve por diferencia */
        if (s_ball != NONE) xrect(s_ball - 2, ROW2 + 14, s_ball + 2, ROW2 + 18, C_WHITE);
        xrect(bx - 2, ROW2 + 14, bx + 2, ROW2 + 18, C_WHITE);
        s_ball = bx;
    }
}

static const char *const hdg_lab[12] = { "N", "3", "6", "E", "12", "15", "S", "21", "24", "W", "30", "33" };

/* posiciones de los numeros: el dibujo se repite cada 30 grados (solo cambia que numero hay en cada sitio), asi que se calculan una vez
 * para cada rumbo % 30 y cada uno de los 12 sitios: centro del texto (x, y) */
static s16 lab_xy[30][12][2];
static u8 lab_ix[12][2];                                 /* indice de letra de cada caracter de cada numero de la rosa */
static int lab_ok;

/* Letras como mapas de bits ya desplazados. Escribir texto con gfx_text_raw cuesta ~0,9 ms por letra en el 68008 (siete filas con
 * desplazamientos, mascaras y dos lecturas-escrituras de pantalla); aqui cada letra, para cada uno de los 8 desplazamientos dentro del
 * grupo de pixeles y para su tinta, se deja preparada como 7 filas de (conservar, poner) de uno o dos grupos, y pintarla es
 * palabra = (palabra y conservar) o poner. Se usan las letras de la rosa: N E S W en verde y las cifras en blanco, y las cifras
 * rojas del rumbo (solo en los tres sitios que ocupan). */
#define GL_W 0                                           /* blancas: cifras 0..6 (las que llevan los numeros de la rosa) */
#define GL_NW 7
static u16 *gl_lab;                                      /* [letra][desplazamiento][7][4]: 7 blancas (0..6), 4 verdes (N E S W) */
static u16 *gl_num;                                      /* [cifra 0..9][3 sitios][7][4], rojas */
#define GL_SH 5                                          /* cada letra ocupa 32 palabras (7 filas x 4 usadas): el indice es un desplazamiento, no un producto de 32 bits */
#define GL_WORDS (1 << GL_SH)
static const u8 num_x[3] = { COL2 - 8, COL2 - 8 + 6, COL2 - 8 + 12 };

static void glyph_make(u16 *dst, int ch, int al, int ink)
{
    const u8 *g = font5x7 + (ch - 32) * 7;
    int r, k;
    u16 field, ink_w = (u16)(((ink & 2) ? 0xFF00 : 0) | ((ink & 1) ? 0x00FF : 0)), m;
    for (r = 0; r < 7; r++) {
        field = (u16)(((u16)(g[r] << 3) << 8) >> al);
        for (k = 0; k < 2; k++) {
            m = (u16)(k ? (field & 0xFF) : (field >> 8));
            m = (u16)(m | (m << 8));
            dst[r * 4 + 2 * k] = (u16)~m;
            dst[r * 4 + 2 * k + 1] = (u16)(ink_w & m);
        }
    }
}

static void glyphs_init(void)
{
    int i, a;
    if (gl_lab) return;
    gl_lab = (u16 *)malloc((size_t)(7 + 4) * 8 * GL_WORDS * 2);
    gl_num = (u16 *)malloc((size_t)10 * 4 * GL_WORDS * 2);
    if (!gl_lab || !gl_num) { free(gl_lab); free(gl_num); gl_lab = gl_num = NULL; return; }
    for (i = 0; i < 7; i++) for (a = 0; a < 8; a++) glyph_make(gl_lab + ((((s32)i << 3) + a) << GL_SH), '0' + i, a, C_WHITE);
    for (i = 0; i < 4; i++) for (a = 0; a < 8; a++) glyph_make(gl_lab + ((((s32)(7 + i) << 3) + a) << GL_SH), "NESW"[i], a, C_GREEN);
    for (i = 0; i < 10; i++) for (a = 0; a < 3; a++) glyph_make(gl_num + (((s32)i << 2) + a) * GL_WORDS, '0' + i, num_x[a] & 7, C_RED);
}

static void glyph_put(int x, int y, const u16 *spr)
{
    gfx_glyph(S.base + ((s32)y << 7) + ((x >> 3) << 1), spr, (x & 7) > 3 ? 2 : 1);
}

static void hdg_tables(void)
{
    int k, q;
    s32 x, y;
    if (lab_ok) return;
    for (k = 0; k < 30; k++)
        for (q = 0; q < 12; q++) {
            dpt(COL2, ROW2, (u16)(((30L * q - k + 360) % 360) * 65536L / 360), 150, &x, &y);
            lab_xy[k][q][0] = (s16)x; lab_xy[k][q][1] = (s16)y;
        }
    for (q = 0; q < 12; q++)
        for (k = 0; hdg_lab[q][k]; k++) {
            char ch = hdg_lab[q][k];
            lab_ix[q][k] = (u8)(ch >= '0' && ch <= '6' ? ch - '0' : 7 + (ch == 'N' ? 0 : ch == 'E' ? 1 : ch == 'S' ? 2 : 3));
        }
    glyphs_init();
    lab_ok = 1;
}

static void hdg_base(int hd)                             /* fondo + rayitas de una copia ya hecha */
{
    const u8 *p;
    if (gyro) {
        p = gyro + (hd % GYN) * GY_SZ;
        gfx_blit(S.base + (s32)GY_Y0 * SCR_STRIDE + GY_B0, p, GY_ROWS, GY_N, SCR_STRIDE, GY_N);
    } else {
        restore(COL2, ROW2);
        gyro_tick_set(hd % GYN);
    }
}

static void hdg_ticks(int hd)                            /* rayitas largas, si no vienen en la copia */
{
#if GYN != 30
    int j;
    for (j = 0; j < 12; j++)
        tick(COL2, ROW2, (u16)(((30L * j - hd + 360) % 360) * 65536L / 360), 196, 226, C_WHITE);
#endif
    (void)hd;
}

static void hdg_labels(int hd)                           /* numeros de la rosa */
{
    int q, j, s = hd / 30, k = hd - 30 * s, x, y, c;
    const s16 *xy;
    const char *t;
    hdg_tables();
    xy = &lab_xy[k][0][0];
    j = s;
    for (q = 0; q < 12; q++, xy += 2, j = j == 11 ? 0 : j + 1) {   /* el numero j esta en el sitio q cuando la rosa ha girado s * 30 grados */
        t = hdg_lab[j];
        x = xy[0] - (t[1] ? 6 : 3) + 1; y = xy[1] - 3;
        for (c = 0; t[c]; c++, x += 6) {
            if (!gl_lab) { char b[2]; b[0] = t[c]; b[1] = 0; gfx_text(&S, x, y, b, j % 3 ? C_WHITE : C_GREEN); continue; }
            glyph_put(x, y, gl_lab + ((((s32)lab_ix[j][c] << 3) + (x & 7)) << GL_SH));
        }
    }
}

static void hdg_num(int hd)                              /* rumbo en el centro (sin divisiones de la biblioteca) */
{
    char b[4];
    int c = 0, i;
    hdg_tables();
    while (hd >= 100) { hd -= 100; c++; }
    b[0] = (char)('0' + c);
    c = 0;
    while (hd >= 10) { hd -= 10; c++; }
    b[1] = (char)('0' + c); b[2] = (char)('0' + hd); b[3] = 0;
    if (!gl_num) { gfx_text(&S, COL2 - 8, ROW2 - 3, b, C_RED); return; }
    for (i = 0; i < 3; i++) glyph_put(num_x[i], ROW2 - 3, gl_num + ((((s32)(b[i] - '0')) << 2) + i) * GL_WORDS);
}

/* (Probado: componer la rosa en RAM alta y volcarla de una vez no ayuda, 20,7 ms contra 17 ms: la copia de entrada y de salida cuesta
 * lo que se ahorra en las letras; ver doc/horizonte_rosa.md) */
static void hdg_paint(int hd)
{
    hdg_base(hd); hdg_ticks(hd); hdg_labels(hd); hdg_num(hd);
}

static void draw_hdg(const Flight *f)
{
    u16 h = (u16)fxmul(f->psi, RAD2BAM);
    int hd = (int)((((s32)h * 360L + 32768L) >> 16) % 360);
    if (hd == s_hdg) return;
    s_hdg = hd;                                          /* la rosa solo gira de grado en grado */
    hdg_paint(hd);
}

static void draw_vsi(const Flight *f)
{
    s32 fpm = M2FT(f->vs) * 60 >> 16;
    int q = (int)(fpm / 50 * 50);
    if (q == s_vsi) return;
    if (s_vsi != NONE) xneedle(COL3, ROW2, s_vsi_a, 230, C_WHITE);
    s_vsi_a = vsi_ang(q);
    xneedle(COL3, ROW2, s_vsi_a, 230, C_WHITE);
    s_vsi = q;
}

static void draw_cdi(const Radios *r)
{
    int x = COL4 + r->cdi1 * 30 / 127, y = r->gs_valid ? ROW2 - r->gs * 16 / 127 : NONE;
    int col = r->n1_flag ? C_WHITE : C_RED, flag = r->n1_flag, obs = (int)((((s32)r->obs1 * 360L + 32768L) >> 16) % 360);
    char b[10];
    int texts = r->n1_loc != s_cdi_loc || flag != s_cdi_flag || (!r->n1_loc && obs != s_cdi_obs);
    if (x == s_cdi_x && y == s_cdi_y && col == s_cdi_col && !texts) return;
    if (s_cdi_x != NONE) {                                              /* 1) borra las barras viejas */
        xline(s_cdi_x, ROW2 - 20, s_cdi_x, ROW2 + 20, s_cdi_col);
        if (s_cdi_y != NONE) xline(COL4 - 22, s_cdi_y, COL4 + 22, s_cdi_y, C_GREEN);
    }
    if (texts) {                                                        /* 2) textos */
        restore_box(COL4 - 22, ROW2 - 23, COL4 + 22, ROW2 - 15);
        if (r->n1_loc) u_cpy(b, "LOC", 10);
        else { u_cpy(b, "OBS ", 10); u_num(b + 4, obs, 3, '0'); }
        ctext(COL4, ROW2 - 22, b, C_GREEN);
        restore_box(COL4 + 8, ROW2 + 5, COL4 + 38, ROW2 + 13);
        ctext(COL4 + 22, ROW2 + 6, flag > 0 ? "TO" : (flag < 0 ? "FR" : "OFF"), flag ? C_WHITE : C_RED);
        s_cdi_loc = r->n1_loc; s_cdi_flag = flag; s_cdi_obs = obs;
    }
    xline(x, ROW2 - 20, x, ROW2 + 20, col);                             /* 3) barras nuevas */
    if (y != NONE) xline(COL4 - 22, y, COL4 + 22, y, C_GREEN);
    s_cdi_x = x; s_cdi_y = y; s_cdi_col = col;
}

/* linea de texto que solo se repinta si cambia */
static void line_txt(char *last, int x, int y, const char *t, int c)
{
    if (same(last, t)) return;
    u_cpy(last, t, 40);
    restore_box(x, y, x + 6 * 22, y + 7);
    gfx_text(&S, x, y, t, c);
}

static void draw_radios(const Radios *r, const PanelInfo *pi)
{
    char b[40], fq[8];
    u_cpy(b, "COM1 ", 40); freq_str(r->com, fq); u_cat(b, fq, 40);
    line_txt(s_rad[0], RB_X, 104, b, C_WHITE);
    u_cpy(b, "NAV1 ", 40); freq_str(r->nav1, fq); u_cat(b, fq, 40);
    if (r->n1) { u_cat(b, " ", 40); u_cat(b, r->n1->ident, 40); }
    line_txt(s_rad[1], RB_X, 113, b, C_WHITE);
    u_cpy(b, "NAV2 ", 40); freq_str(r->nav2, fq); u_cat(b, fq, 40);
    if (r->n2) { u_cat(b, r->n2_flag > 0 ? " TO " : " FR ", 40); u_catn(b, r->cdi2, 1, ' ', 40); }
    line_txt(s_rad[2], RB_X, 122, b, C_WHITE);
    u_cpy(b, "ADF  ", 40); u_catn(b, r->adf, 4, ' ', 40);
    if (r->adf_valid) { u_cat(b, " ", 40); u_catn(b, (((s32)r->adf_rel * 360L + 32768L) >> 16) % 360, 3, '0', 40); }
    line_txt(s_rad[3], RB_X, 131, b, C_WHITE);
    u_cpy(b, "DME  ", 40);
    if (r->dme10 >= 0) { char t[12]; u_dec1(t, r->dme10, 5); u_cat(b, t, 40); u_cat(b, " NM", 40); }
    else u_cat(b, "---", 40);
    line_txt(s_rad[4], RB_X, 140, b, C_WHITE);
    u_cpy(b, "XPDR ", 40); u_catn(b, r->squawk, 4, '0', 40);
    u_cat(b, "  ", 40); u_catn(b, (pi->clock / 3600) % 24, 2, '0', 40); u_cat(b, ":", 40);
    u_catn(b, (pi->clock / 60) % 60, 2, '0', 40);
    line_txt(s_rad[5], RB_X, 149, b, C_GREEN);
}

/* barra de palanca: solo se pintan las filas que cambian respecto a la ultima vez. top = 0: el color sube desde abajo (gases,
 * paso, mezcla); top = 1: el color baja desde arriba (flaps: con 0 todo gris y arriba, la barra baja al dar grados) */
static void bar(int idx, int x, int w, s32 v, int c, int top)
{
    int h = (int)(fxmul(CLAMP(v, 0, FX_ONE), FXI(44)) >> 16), o = s_bar[idx], x1 = x + w - 1;
    if (h == o) return;
    if (top) {
        if (o == NONE) {
            gfx_rect(&S, x, 168, x1, 212, fill_tab[F_DKGREY]);
            if (h) gfx_rect(&S, x, 168, x1, 167 + h, fill_tab[c]);
        } else if (h > o) gfx_rect(&S, x, 168 + o, x1, 167 + h, fill_tab[c]);
        else gfx_rect(&S, x, 168 + h, x1, 167 + o, fill_tab[F_DKGREY]);
    } else if (o == NONE) {
        gfx_rect(&S, x, 168, x1, 212, fill_tab[F_DKGREY]);
        gfx_rect(&S, x, 212 - h, x1, 212, fill_tab[c]);
    } else if (h > o) gfx_rect(&S, x, 212 - h, x1, 212 - o, fill_tab[c]);
    else gfx_rect(&S, x, 212 - o, x1, 212 - h - 1, fill_tab[F_DKGREY]);
    s_bar[idx] = h;
}

/* palanca de motor (gases, paso, mezcla): una barra de 6 px, o dos de 2 px (motor izquierdo y derecho) en los bimotores */
static void lever(int slot, const s32 *v, int c)
{
    int x = slot_x[slot];
    if (x < 0) return;
    if (AC->n_eng > 1) { bar(slot, x, 2, v[0], c, 0); bar(4 + slot, x + 4, 2, v[1], c, 0); }
    else bar(slot, x, 6, v[0], c, 0);
}

/* luces del tren (centro, izquierda, derecha): al bajar se ponen verdes por ese orden segun avanza el tren; al subir, al reves
 * (primero las laterales, luego la central). Con el tren recogido y bloqueado, rojas unos segundos y luego apagadas (gris). */
static void draw_gear(const Flight *f)
{
    static const int gx[3] = { 443, 434, 452 }, gy[3] = { 164, 167, 167 };
    static const u8 col[3] = { F_DKGREY, F_RED, F_GREEN };
    s32 g = f->gear_pos;
    int l[3], i, key, lv = f->gear_down ? 1 : 0, y;
    l[0] = g >= FXF(55, 100) ? 2 : 1;
    l[1] = g >= FXF(80, 100) ? 2 : 1;
    l[2] = g >= FX_ONE ? 2 : 1;
    if (g <= 0 && !f->gear_down) {
        if (!s_up_on) { s_up_on = 1; s_up_t = plat_ticks() - (s_gear == NONE ? 1000UL : 0UL); }
        if (plat_ticks() - s_up_t > 150UL) l[0] = l[1] = l[2] = 0;           /* 3 s */
    } else s_up_on = 0;
    key = l[0] + 3 * l[1] + 9 * l[2];
    if (key != s_gear) {
        for (i = 0; i < 3; i++) gfx_rect(&S, gx[i], gy[i], gx[i] + 6, gy[i] + 5, fill_tab[col[l[i]]]);
        s_gear = key;
    }
    if (lv != s_lever) {                                                    /* palanca: cuadrado arriba (recoger) o abajo (sacar) */
        restore_box(GEAR_LV_X - 4, GEAR_LV_Y0 - 3, GEAR_LV_X + 3, GEAR_LV_Y1 + 2);
        y = lv ? GEAR_LV_Y1 - 2 : GEAR_LV_Y0;
        gfx_rect(&S, GEAR_LV_X - 1, y, GEAR_LV_X + 1, y + 2, fill_tab[F_WHITE]);
        s_lever = lv;
    }
}

static void eng_txt(char *last, int y, const char *t, int c)
{
    if (same(last, t)) return;
    u_cpy(last, t, 24);
    restore_box(434, y, 434 + 6 * 11, y + 7);
    gfx_text(&S, 434, y, t, c);
}

static void draw_engine(const Flight *f, const Radios *r)
{
    char b[24];
    int gs;
    lever(0, f->thr, F_WHITE);
    lever(1, f->prop, F_GREEN);
    lever(2, f->mix, F_RED);
    if (slot_x[3] >= 0) bar(3, slot_x[3], 6, f->flap_pos, F_ORANGE, 1);
    if (AC->retract) draw_gear(f);
    if (AC->eng_type == ENG_PISTON) {
        u_cpy(b, "MAP ", 24); { char t[10]; u_dec1(t, fxmul(f->e[0].map, FXI(10)) >> 16, 4); u_cat(b, t, 24); }
        eng_txt(s_eng[0], 184, b, C_WHITE);
        u_cpy(b, "EGT ", 24); u_catn(b, f->e[0].egt >> 16, 4, ' ', 24);
        eng_txt(s_eng[1], 193, b, C_WHITE);
    } else if (AC->eng_type == ENG_JET) {
        u_cpy(b, "EGT ", 24); u_catn(b, f->e[0].egt >> 16, 4, ' ', 24);
        eng_txt(s_eng[0], 184, b, C_WHITE);
        u_cpy(b, "FF  ", 24); u_catn(b, f->e[0].ff >> 16, 4, ' ', 24);
        eng_txt(s_eng[1], 193, b, C_WHITE);
    }
    if (AC->eng_type != ENG_NONE) {
        u_cpy(b, "COMB ", 24); u_catn(b, f->fuel >> 16, 4, ' ', 24); u_cat(b, "L", 24);
        eng_txt(s_eng[2], 202, b, f->fuel < AC->fuel_max / 10 ? C_RED : C_WHITE);
    }
    if (r->marker != s_marker) {
        restore_box(434, 212, 434 + 6 * 5, 219);
        gfx_text(&S, 434, 212, "O", r->marker == NAV_OM ? C_WHITE : C_BLACK + 0);
        gfx_text(&S, 446, 212, "M", r->marker == NAV_MM ? C_RED : C_BLACK + 0);
        gfx_text(&S, 458, 212, "I", r->marker == NAV_IM ? C_WHITE : C_BLACK + 0);
        if (!r->marker) gfx_text(&S, 434, 212, "O M I", C_GREEN);
        s_marker = r->marker;
    }
    gs = (f->e[0].running == 0 && AC->eng_type != ENG_NONE);
    if (gs != s_stopped) {
        restore_box(470, 212, 470 + 6 * 6, 219);
        if (gs) gfx_text(&S, 470, 212, "PARADO", C_RED);
        s_stopped = gs;
    }
}

static void draw_msgs(const Atc *a, const PanelInfo *pi)
{
    char b[84];
    atc_window(a, b, 82);
    if (!same(s_msg, b)) {
        u_cpy(s_msg, b, 84);
        restore_box(4, MSG_Y, 507, MSG_Y + 7);
        gfx_text(&S, 4, MSG_Y, b, C_WHITE);
    }
    if (pi->status && (!same(s_stat, pi->status) || pi->warn != s_warn)) {
        u_cpy(s_stat, pi->status, 84);
        s_warn = pi->warn;
        restore_box(4, STAT_Y, 507, STAT_Y + 7);
        gfx_text(&S, 4, STAT_Y, pi->status, pi->warn ? C_RED : C_GREEN);
    }
}

/* indicador de FPS en la esquina inferior izquierda (filas 249-255, bajo la linea de estado): vistas 3D y simulacion por segundo */
static void draw_fps(const PanelInfo *pi)
{
    char b[24], t[10];
    int key = pi->fps10 * 7 + pi->lps10;
    if (key == s_fps) return;
    s_fps = key;
    gfx_rect(&S, 4, 249, 4 + 6 * 18, 255, fill_tab[F_BLACK]);
    if (pi->fps10 == -2) return;
    if (pi->fps10 < 0) { gfx_text(&S, 4, 249, "FPS ...", C_GREEN); return; }
    u_cpy(b, "3D ", 24); u_dec1(t, pi->fps10, 4); u_cat(b, t, 24);
    u_cat(b, "  SIM ", 24); u_dec1(t, pi->lps10, 4); u_cat(b, t, 24);
    gfx_text(&S, 4, 249, b, C_GREEN);
}

void panel_update(u8 *scr, const Flight *f, const Radios *r, const Atc *a,
                  const PanelInfo *pi, int part)
{
    S.base = scr;
    draw_ai(f);
    switch (part) {
    case 0: draw_asi(f); draw_alt(f); break;
    case 1: draw_vsi(f); draw_hdg(f); break;
    case 2: draw_tc(f); draw_rpm(f); draw_cdi(r); break;
    case 3: draw_radios(r, pi); draw_engine(f, r); break;
    }
    draw_msgs(a, pi);
    draw_fps(pi);
}

/* El panel en seis trozos independientes (estudio de interrupciones): 0 horizonte, 1 velocidad y altitud, 2 variometro y rumbo,
 * 3 viraje, rpm y CDI, 4 radios y motor, 5 mensajes ATC y FPS. Cada llamada cuesta una fraccion del panel entero. */
void panel_piece(u8 *scr, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi, int k)
{
    S.base = scr;
    switch (k) {
    case 0: draw_ai(f); break;
    case 1: draw_asi(f); draw_alt(f); break;
    case 2: draw_vsi(f); draw_hdg(f); break;
    case 3: draw_tc(f); draw_rpm(f); draw_cdi(r); break;
    case 4: draw_radios(r, pi); draw_engine(f, r); break;
    default: draw_msgs(a, pi); draw_fps(pi); break;
    }
}

void panel_all(u8 *scr, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi)
{
    int i;
    for (i = 0; i < PANEL_PARTS; i++) panel_update(scr, f, r, a, pi, i);
}

/* El panel entero como trabajo reanudable (estudio de interrupciones): cada llamada pinta instrumentos hasta que cambia el contador
 * de 50 Hz respecto a tick0 y sigue en la siguiente. El horizonte va por bandas de AI_BAND filas; el resto, de instrumento en
 * instrumento. Devuelve 1 al terminar una vuelta completa. */
#ifndef AI_BAND
#define AI_BAND 6
#endif
static int pj_u, pj_y;

int panel_job_step(u8 *scr, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi, u32 tick0)
{
    int qb, qh;
    s32 hh;
    S.base = scr;
    for (;;) {
        switch (pj_u) {
        case 0:                                   /* horizonte: solo si cambia el alabeo (grados) o el cabeceo (pixeles) */
            qb = (int)(((u16)fxmul(f->phi, RAD2BAM) + 91U) / 182U);
            hh = fxmul(f->the, 57L * 65536L) * RY / 25 >> 16;
            qh = (int)CLAMP(hh, -RY * 2, RY * 2);
            if (qb == s_ai_b && qh == s_ai_h) { pj_u = 3; break; }
            s_ai_b = qb; s_ai_h = qh;
            ai_begin(f); pj_y = ROW1 - RY + 1; pj_u = 1;
            break;
        case 1:
            ai_rows(pj_y, MIN(pj_y + AI_BAND - 1, ROW1 + RY - 1));
            pj_y += AI_BAND;
            if (pj_y > ROW1 + RY - 1) pj_u = 2;
            break;
        case 2: ai_end(); pj_u = 3; break;
        case 3: draw_asi(f); pj_u++; break;
        case 4: draw_alt(f); pj_u++; break;
        case 5: draw_vsi(f); pj_u++; break;
        case 6: draw_hdg(f); pj_u++; break;
        case 7: draw_tc(f); pj_u++; break;
        case 8: draw_rpm(f); pj_u++; break;
        case 9: draw_cdi(r); pj_u++; break;
        case 10: draw_radios(r, pi); pj_u++; break;
        case 11: draw_engine(f, r); pj_u++; break;
        case 12: draw_msgs(a, pi); draw_fps(pi); pj_u = 0; return 1;
        }
        if (plat_ticks() != tick0) return 0;
    }
}

void panel_surface(u8 *scr) { S.base = scr; }     /* para src/bench.c */
