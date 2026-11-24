/* panel.c - instrumentos. Fondo estatico guardado en RAM: cada instrumento se
 * restaura desde esa copia limpia (solo su caja) y luego se pintan agujas. */
#include "panel.h"
#include "util.h"
#include <stdlib.h>

#define RX 40
#define RY 27
#define ROW1 130
#define ROW2 191
#define COL1 52
#define COL2 144
#define COL3 236
#define COL4 328
#define RB_X 376               /* bloque derecho */
#define MSG_Y 229
#define STAT_Y 241

static u8 *clean;              /* copia del panel limpio (filas PANEL_Y0..255) */
static Surface S;
static const AircraftDef *AC;
static int asi_max;
static s16 hw_tab[RY + 1];     /* semiancho del horizonte artificial por fila */

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

static void needle(int cx, int cy, u16 a, int k, int c)
{
    s32 x, y;
    dpt(cx, cy, a, k, &x, &y);
    gfx_line(&S, cx, cy, x, y, c);
    gfx_line(&S, cx + 1, cy, x + 1, y, c);
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

/* ---------------------------------------------------------- fondo estatico */
void panel_init(u8 *scr, const AircraftDef *ac)
{
    int i, k;
    char b[8];
    s32 v;
    AC = ac;
    gfx_init_surface(&S, scr, SCR_H);
    gfx_rect(&S, 0, PANEL_Y0 - 2, SCR_W - 1, SCR_H - 1, fill_tab[F_BLACK]);
    gfx_rect(&S, 0, PANEL_Y0 - 2, SCR_W - 1, PANEL_Y0 - 2, fill_tab[F_RED]);
    gfx_rect(&S, 0, MSG_Y - 3, SCR_W - 1, MSG_Y - 3, fill_tab[F_RED]);
    for (i = 0; i <= RY; i++) hw_tab[i] = (s16)(isqrt32((u32)(RY * RY - i * i) * 256 * 9 / 4) * 1 / 16 - 1);

    asi_max = ((ac->vne ? ac->vne : 160) * 6 / 5 + 19) / 20 * 20;

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
    gfx_text(&S, RB_X + 1, 215, "T P M F", C_GREEN);
    gfx_text(&S, 434, 176, "TREN", C_GREEN);

    /* guardar copia limpia */
    if (clean) free(clean);
    clean = (u8 *)malloc((size_t)(SCR_H - PANEL_Y0) * SCR_STRIDE);
    if (clean) gfx_memcpy(clean, scr + (s32)PANEL_Y0 * SCR_STRIDE, (s32)(SCR_H - PANEL_Y0) * SCR_STRIDE);
}

/* ---------------------------------------------------------- instrumentos */
static void draw_asi(const Flight *f)
{
    s32 kt = MS2KT(f->ias) >> 16;
    char b[8];
    restore(COL1, ROW1);
    needle(COL1, ROW1, asi_ang(MIN(kt, asi_max)), 230, C_WHITE);
    u_num(b, kt, 3, ' ');
    gfx_text(&S, COL1 - 8, ROW1 + 16, b, C_WHITE);
}

static void draw_ai(const Flight *f)
{
    u16 bb = (u16)fxmul(f->phi, RAD2BAM);
    s32 sa = isin(bb), ca = icos(bb), h, B, xb;
    int y, x0, x1, dy, w, cx = COL2, cy = ROW1;
    Fill sky = fill_tab[F_WHITE], gnd = fill_tab[F_BROWN];
    h = fxmul(f->the, 57L * 65536L) * RY / 25 >> 16;         /* grados -> pixeles */
    h = CLAMP(h, -RY * 2, RY * 2);
    for (y = cy - RY + 1; y <= cy + RY - 1; y++) {
        dy = y - cy;
        w = hw_tab[ABS(dy)];
        x0 = cx - w; x1 = cx + w;
        B = (dy - h) * 3 * ca / 2;            /* cielo si (x-cx)*sa + B < 0 */
        if (ABS(sa) < 64) {
            gfx_hspan(&S, y, x0, x1, B < 0 ? sky : gnd);
            continue;
        }
        xb = cx - B / sa;
        if (sa > 0) {                          /* cielo a la izquierda de xb */
            if (xb <= x0) gfx_hspan(&S, y, x0, x1, gnd);
            else if (xb > x1) gfx_hspan(&S, y, x0, x1, sky);
            else { gfx_hspan(&S, y, x0, (int)xb - 1, sky); gfx_hspan(&S, y, (int)xb, x1, gnd); }
        } else {
            if (xb <= x0) gfx_hspan(&S, y, x0, x1, sky);
            else if (xb > x1) gfx_hspan(&S, y, x0, x1, gnd);
            else { gfx_hspan(&S, y, x0, (int)xb - 1, gnd); gfx_hspan(&S, y, (int)xb, x1, sky); }
        }
    }
    /* avion de referencia */
    gfx_line(&S, cx - 24, cy, cx - 8, cy, C_RED);
    gfx_line(&S, cx - 8, cy, cx - 4, cy + 3, C_RED);
    gfx_line(&S, cx + 8, cy, cx + 24, cy, C_RED);
    gfx_line(&S, cx + 8, cy, cx + 4, cy + 3, C_RED);
    gfx_line(&S, cx - 24, cy + 1, cx - 8, cy + 1, C_RED);
    gfx_line(&S, cx + 8, cy + 1, cx + 24, cy + 1, C_RED);
    gfx_rect(&S, cx - 1, cy - 1, cx + 1, cy, fill_tab[F_RED]);
    ring(cx, cy);
}

static void draw_alt(const Flight *f)
{
    s32 ft = M2FT(f->z) >> 8;                 /* z esta en Q8 m */
    char b[8];
    restore(COL3, ROW1);
    gfx_rect(&S, COL3 - 16, ROW1 + 9, COL3 + 16, ROW1 + 17, fill_tab[F_BLACK]);
    u_num(b, ft, 5, ' ');
    gfx_text(&S, COL3 - 14, ROW1 + 10, b, C_WHITE);
    needle(COL3, ROW1, (u16)((ft % 10000) * 65536L / 10000), 140, C_GREEN);
    needle(COL3, ROW1, (u16)((ft % 1000) * 65536L / 1000), 230, C_WHITE);
}

static void draw_rpm(const Flight *f)
{
    char b[8];
    int i;
    restore(COL4, ROW1);
    if (AC->eng_type == ENG_NONE) return;
    for (i = AC->n_eng - 1; i >= 0; i--) {
        s32 r = f->e[i].rpm >> 16;
        if (AC->eng_type == ENG_JET)
            needle(COL4, ROW1, rpm_ang(MIN(fxmul(f->e[i].n1, FXI(100)) >> 16, 110), 110), i ? 190 : 230, i ? C_RED : C_WHITE);
        else
            needle(COL4, ROW1, rpm_ang(MIN(r, 3500), 3500), i ? 190 : 230, i ? C_RED : C_WHITE);
    }
    if (AC->eng_type == ENG_JET) u_num(b, fxmul(f->e[0].n1, FXI(100)) >> 16, 3, ' ');
    else u_num(b, f->e[0].rpm >> 16, 4, ' ');
    gfx_text(&S, COL4 - 11, ROW1 + 16, b, C_WHITE);
}

static void draw_tc(const Flight *f)
{
    s32 bank = CLAMP(fxmul(f->turn, FXI(5)), -FXF(7, 10), FXF(7, 10));   /* 3 g/s -> 15 g */
    u16 a = (u16)fxmul(bank, RAD2BAM);
    s32 ca = icos(a), sa = isin(a), x, y, bx;
    int cx = COL1, cy = ROW2 - 2;
    restore(COL1, ROW2);
    x = (ca * 26) >> 14; y = (sa * 17) >> 14;
    gfx_line(&S, cx - x, cy + y, cx + x, cy - y, C_WHITE);
    gfx_line(&S, cx - x, cy + y + 1, cx + x, cy - y + 1, C_WHITE);
    gfx_line(&S, cx, cy, cx - ((sa * 6) >> 14), cy - ((ca * 5) >> 14), C_WHITE);
    bx = cx + fxmul(f->slip, FXI(10));
    gfx_rect(&S, (int)bx - 2, ROW2 + 14, (int)bx + 2, ROW2 + 18, fill_tab[F_WHITE]);
}

static void draw_hdg(const Flight *f)
{
    u16 h = (u16)fxmul(f->psi, RAD2BAM), a;
    int i;
    static const char *lab[12] = { "N", "3", "6", "E", "12", "15", "S", "21", "24", "W", "30", "33" };
    char b[6];
    restore(COL2, ROW2);
    for (i = 0; i < 36; i++) {
        a = (u16)(i * 1820L - h);
        tick(COL2, ROW2, a, (i % 3) ? 226 : 196, 250, C_WHITE);
        if (i % 3 == 0) label_at(COL2, ROW2, a, 150, lab[i / 3], i % 9 ? C_WHITE : C_GREEN);
    }
    u_num(b, (((s32)h * 360L + 32768L) >> 16) % 360, 3, '0');
    gfx_rect(&S, COL2 - 10, ROW2 - 4, COL2 + 10, ROW2 + 4, fill_tab[F_BLACK]);
    gfx_text(&S, COL2 - 8, ROW2 - 3, b, C_RED);
}

static void draw_vsi(const Flight *f)
{
    s32 fpm = M2FT(f->vs) * 60 >> 16;
    restore(COL3, ROW2);
    needle(COL3, ROW2, vsi_ang(fpm), 230, C_WHITE);
}

static void draw_cdi(const Radios *r)
{
    int x = COL4 + r->cdi1 * 30 / 127, y;
    char b[10];
    restore(COL4, ROW2);
    gfx_line(&S, x, ROW2 - 20, x, ROW2 + 20, r->n1_flag ? C_WHITE : C_RED);
    if (r->gs_valid) {
        y = ROW2 - r->gs * 16 / 127;
        gfx_line(&S, COL4 - 22, y, COL4 + 22, y, C_GREEN);
    }
    if (r->n1_loc) u_cpy(b, "LOC", 10);
    else { u_cpy(b, "OBS ", 10); u_num(b + 4, (((s32)r->obs1 * 360L + 32768L) >> 16) % 360, 3, '0'); }
    ctext(COL4, ROW2 - 22, b, C_GREEN);
    ctext(COL4 + 22, ROW2 + 6, r->n1_flag > 0 ? "TO" : (r->n1_flag < 0 ? "FR" : "OFF"),
          r->n1_flag ? C_WHITE : C_RED);
}

static void line_txt(int x, int y, const char *t, int c)
{
    restore_box(x, y, x + 6 * 22, y + 7);
    gfx_text(&S, x, y, t, c);
}

static void draw_radios(const Radios *r, const PanelInfo *pi)
{
    char b[40], fq[8];
    u_cpy(b, "COM1 ", 40); freq_str(r->com, fq); u_cat(b, fq, 40);
    line_txt(RB_X, 104, b, C_WHITE);
    u_cpy(b, "NAV1 ", 40); freq_str(r->nav1, fq); u_cat(b, fq, 40);
    if (r->n1) { u_cat(b, " ", 40); u_cat(b, r->n1->ident, 40); }
    line_txt(RB_X, 113, b, C_WHITE);
    u_cpy(b, "NAV2 ", 40); freq_str(r->nav2, fq); u_cat(b, fq, 40);
    if (r->n2) { u_cat(b, r->n2_flag > 0 ? " TO " : " FR ", 40); u_catn(b, r->cdi2, 1, ' ', 40); }
    line_txt(RB_X, 122, b, C_WHITE);
    u_cpy(b, "ADF  ", 40); u_catn(b, r->adf, 4, ' ', 40);
    if (r->adf_valid) { u_cat(b, " ", 40); u_catn(b, (((s32)r->adf_rel * 360L + 32768L) >> 16) % 360, 3, '0', 40); }
    line_txt(RB_X, 131, b, C_WHITE);
    u_cpy(b, "DME  ", 40);
    if (r->dme10 >= 0) { char t[12]; u_dec1(t, r->dme10, 5); u_cat(b, t, 40); u_cat(b, " NM", 40); }
    else u_cat(b, "---", 40);
    line_txt(RB_X, 140, b, C_WHITE);
    u_cpy(b, "XPDR ", 40); u_catn(b, r->squawk, 4, '0', 40);
    u_cat(b, "  ", 40); u_catn(b, (pi->clock / 3600) % 24, 2, '0', 40); u_cat(b, ":", 40);
    u_catn(b, (pi->clock / 60) % 60, 2, '0', 40);
    line_txt(RB_X, 149, b, C_GREEN);
}

static void bar(int x, s32 v, int c)
{
    int h = (int)(fxmul(CLAMP(v, 0, FX_ONE), FXI(44)) >> 16);
    gfx_rect(&S, x, 168, x + 5, 212, fill_tab[F_DKGREY]);
    gfx_rect(&S, x, 212 - h, x + 5, 212, fill_tab[c]);
}

static void draw_engine(const Flight *f, const Radios *r)
{
    char b[24];
    int i, gc;
    restore_box(RB_X, 164, 511, 222);
    bar(RB_X + 1, f->thr[0], F_WHITE);
    bar(RB_X + 13, AC->prop_type == PROP_CS ? f->prop[0] : 0, F_GREEN);
    bar(RB_X + 25, f->mix[0], F_RED);
    bar(RB_X + 37, f->flap_pos, F_ORANGE);
    for (i = 0; i < 3; i++) {
        gc = f->gear_pos >= FX_ONE ? F_GREEN : (f->gear_pos > 0 ? F_RED : F_DKGREY);
        if (!AC->retract) gc = F_GREEN;
        gfx_rect(&S, 434 + i * 9, 167, 440 + i * 9, 172, fill_tab[gc]);
    }
    if (AC->eng_type == ENG_PISTON) {
        u_cpy(b, "MAP ", 24); { char t[10]; u_dec1(t, fxmul(f->e[0].map, FXI(10)) >> 16, 4); u_cat(b, t, 24); }
        gfx_text(&S, 434, 184, b, C_WHITE);
        u_cpy(b, "EGT ", 24); u_catn(b, f->e[0].egt >> 16, 4, ' ', 24);
        gfx_text(&S, 434, 193, b, C_WHITE);
    } else if (AC->eng_type == ENG_JET) {
        u_cpy(b, "EGT ", 24); u_catn(b, f->e[0].egt >> 16, 4, ' ', 24);
        gfx_text(&S, 434, 184, b, C_WHITE);
        u_cpy(b, "FF  ", 24); u_catn(b, f->e[0].ff >> 16, 4, ' ', 24);
        gfx_text(&S, 434, 193, b, C_WHITE);
    }
    if (AC->eng_type != ENG_NONE) {
        u_cpy(b, "COMB ", 24); u_catn(b, f->fuel >> 16, 4, ' ', 24); u_cat(b, "L", 24);
        gfx_text(&S, 434, 202, b, f->fuel < AC->fuel_max / 10 ? C_RED : C_WHITE);
    }
    gfx_text(&S, 434, 212, "O", r->marker == NAV_OM ? C_WHITE : C_BLACK + 0);
    gfx_text(&S, 446, 212, "M", r->marker == NAV_MM ? C_RED : C_BLACK + 0);
    gfx_text(&S, 458, 212, "I", r->marker == NAV_IM ? C_WHITE : C_BLACK + 0);
    if (!r->marker) gfx_text(&S, 434, 212, "O M I", C_GREEN);
    if (f->e[0].running == 0 && AC->eng_type != ENG_NONE) gfx_text(&S, 470, 212, "PARADO", C_RED);
}

static void draw_msgs(const Atc *a, const PanelInfo *pi)
{
    char b[84];
    restore_box(4, MSG_Y, 507, STAT_Y + 7);
    atc_window(a, b, 82);
    gfx_text(&S, 4, MSG_Y, b, C_WHITE);
    if (pi->status) gfx_text(&S, 4, STAT_Y, pi->status, pi->warn ? C_RED : C_GREEN);
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
}

void panel_all(u8 *scr, const Flight *f, const Radios *r, const Atc *a, const PanelInfo *pi)
{
    int i;
    for (i = 0; i < PANEL_PARTS; i++) panel_update(scr, f, r, a, pi, i);
}
