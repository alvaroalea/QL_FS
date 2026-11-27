/* map.c - vista de mapa: suelo, agua, pistas, carreteras y edificios desde arriba (norte arriba), balizas con su nombre, frecuencia y
 * morse, aeropuertos con sus frecuencias y el avion en el centro. Se dibuja una vez (no se actualiza sola). */
#include "map.h"
#include "gfx.h"
#include "util.h"
#include "nav.h"
#include "platform.h"
#include <string.h>

static Surface S;
static Tile *tl[TILE_CACHE];
static int ntl;
static int msh;                                  /* log2 de los metros por pixel */
static s32 mcx, mcy;                             /* centro del mapa (m) */

#define PX(x) (SCR_W / 2 + (int)(((x) - mcx) >> msh))
#define PY(y) (SCR_H / 2 - (int)(((y) - mcy) >> msh))
#define MAXG 500

static const Obj *gl[MAXG];
static int ngl;

static const char *const morse_l[26] = { ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
                                         "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.." };
static const char *const morse_d[10] = { "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----." };

static void morse(int x, int y, const char *id, int c)
{
    for (; *id; id++) {
        const char *m = 0;
        if (*id >= 'A' && *id <= 'Z') m = morse_l[*id - 'A'];
        else if (*id >= '0' && *id <= '9') m = morse_d[*id - '0'];
        if (!m) continue;
        for (; *m; m++) {
            if (*m == '.') { gfx_rect(&S, x, y, x + 1, y + 1, fill_tab[c]); x += 4; }
            else { gfx_rect(&S, x, y, x + 5, y + 1, fill_tab[c]); x += 8; }
        }
        x += 4;
    }
}

static s32 vx(const Obj *o, int k) { return o->ox + (((s32)o->v[k * 3] << o->vshift) >> 3); }
static s32 vy(const Obj *o, int k) { return o->oy + (((s32)o->v[k * 3 + 1] << o->vshift) >> 3); }

static int on_screen(const Obj *o)
{
    int x = PX(o->ox), y = PY(o->oy), r = (int)(o->radius >> msh) + 1;
    return x + r >= 0 && x - r < SCR_W && y + r >= 0 && y - r < SCR_H;
}

void map_prepare(World *w, const Flight *f, int lvl)
{
    msh = 5 + CLAMP(lvl, 0, MAP_LEVELS - 1);
    mcx = f->x >> 8; mcy = f->y >> 8;
    ntl = world_tiles_near(w, mcx, mcy, (s32)(SCR_W / 2) << msh, tl, TILE_CACHE);
}

static void label(int x, int y, const char *id, const char *fq, int c)
{
    char b[24];
    u_cpy(b, id, 24); u_cat(b, " ", 24); u_cat(b, fq, 24);
    gfx_text(&S, x + 9, y - 8, b, C_WHITE);
    morse(x + 9, y + 2, id, c);
}

static void draw_nav(const Nav *n)
{
    int x = PX((s32)n->x), y = PY((s32)n->y);
    char fq[12];
    s32 dx, dy;
    if (n->type == NAV_ILS) {                    /* rumbo de aproximacion: del localizador hacia atras, 25 km */
        dx = (isin(n->course) * 25000L) >> 14; dy = (icos(n->course) * 25000L) >> 14;
        gfx_line(&S, x, y, PX((s32)n->x - dx), PY((s32)n->y - dy), C_GREEN);
    }
    if (x < -70 || x > SCR_W + 70 || y < -12 || y > SCR_H + 12) return;
    if (n->type == NAV_VOR || n->type == NAV_VORDME) {
        gfx_ellipse(&S, x, y, 7, 7, C_WHITE);
        gfx_line(&S, x, y, x, y - 7, C_WHITE);            /* radio hacia el norte */
        if (n->type == NAV_VORDME) gfx_rect(&S, x - 1, y - 1, x + 1, y + 1, fill_tab[F_WHITE]);
        freq_str(n->freq, fq);
        label(x + 3, y, n->ident, fq, C_WHITE);
    } else if (n->type == NAV_NDB) {
        gfx_ellipse(&S, x, y, 3, 3, C_WHITE);
        gfx_pixel(&S, x, y, C_WHITE);
        u_num(fq, n->freq, 1, ' ');
        label(x - 3, y, n->ident, fq, C_WHITE);
    } else if (n->type == NAV_ILS) {
        gfx_ellipse(&S, x, y, 2, 2, C_GREEN);
        freq_str(n->freq, fq);
        label(x - 3, y, n->ident, fq, C_GREEN);
    }
}

static void draw_airport(const Airport *a)
{
    int i, x = PX((s32)a->x), y = PY((s32)a->y);
    char b[40], fq[12];
    for (i = 0; i < a->nrwy; i++) {              /* pistas: raya doble blanca (cada cabecera es una entrada) */
        const Runway *r = &a->rwy[i];
        s32 ex = r->tx + ((isin(r->hdg) * (s32)r->len) >> 14), ey = r->ty + ((icos(r->hdg) * (s32)r->len) >> 14);
        int x0 = PX(r->tx), y0 = PY(r->ty), x1 = PX(ex), y1 = PY(ey);
        gfx_line(&S, x0, y0, x1, y1, C_WHITE);
        gfx_line(&S, x0 + 1, y0, x1 + 1, y1, C_WHITE);
    }
    if (x < -150 || x > SCR_W + 20 || y < -20 || y > SCR_H + 20) return;
    gfx_ellipse(&S, x, y, 5, 5, C_WHITE);
    u_cpy(b, a->ident, 40); u_cat(b, " ", 40); u_cat(b, a->name, 40);
    gfx_text(&S, x + 8, y - 16, b, C_WHITE);
    u_cpy(b, "TWR ", 40); freq_str(a->twr, fq); u_cat(b, fq, 40);
    u_cat(b, "  ATIS ", 40); freq_str(a->atis, fq); u_cat(b, fq, 40);
    gfx_text(&S, x + 8, y - 7, b, C_GREEN);
}

static void draw_obj_ground(const Obj *o)
{
    const u8 *f = o->f;
    s32 xy[48];
    int i, k, n;
    for (i = 0; i < o->nf; i++, f += 2 + n) {
        n = f[0];
        if ((f[1] & FA_NOFILL) || n > 20) continue;
        for (k = 0; k < n; k++) { xy[2 * k] = PX(vx(o, f[2 + k])); xy[2 * k + 1] = PY(vy(o, f[2 + k])); }
        gfx_poly32(&S, n, xy, fill_tab[(o->flags & OF_WATER) ? F_DKGREEN : FA_FILL(f[1])]);
    }
}

static void draw_obj_lines(const Obj *o)
{
    const u8 *f = o->f;
    int i, k, n;
    for (i = 0; i < o->nf; i++, f += 2 + n) {
        n = f[0];
        for (k = 0; k + 1 < n; k++)
            gfx_line(&S, PX(vx(o, f[2 + k])), PY(vy(o, f[2 + k])), PX(vx(o, f[3 + k])), PY(vy(o, f[3 + k])), f[1] & 3);
    }
}

static void draw_obj_solid(const Obj *o)
{
    const u8 *f = o->f;
    s32 xy[48];
    int i, k, n, nf = o->nff;
    if (o->flags & OF_MOUNTAIN) {                /* montana: disco marron de ~70 % del radio */
        int cx = PX(o->ox), cy = PY(o->oy);
        s32 r = ((s32)o->radius * 7 / 10) >> msh;
        for (k = 0; k < 12; k++) { xy[2 * k] = cx + ((isin((u16)(k * 5461)) * r) >> 14); xy[2 * k + 1] = cy - ((icos((u16)(k * 5461)) * r) >> 14); }
        gfx_poly32(&S, 12, xy, fill_tab[F_BROWN]);
        return;
    }
    for (i = 0; i < nf; i++, f += 2 + n) {       /* edificios: solo los techos (los primeros nff caras) */
        n = f[0];
        if (n > 8) continue;
        for (k = 0; k < n; k++) { xy[2 * k] = PX(vx(o, f[2 + k])); xy[2 * k + 1] = PY(vy(o, f[2 + k])); }
        gfx_poly32(&S, n, xy, fill_tab[F_LTGREY]);
    }
}

void map_draw(u8 *scr, World *w, const Flight *f, int lvl)
{
    int i, k, j, x, y, lm;
    u16 h = (u16)fxmul(f->psi, RAD2BAM);
    static const int km[MAP_LEVELS] = { 2, 5, 10, 20 };
    char b[24];
    gfx_init_surface(&S, scr, SCR_H);
    gfx_clip(&S, 0, 0, SCR_W - 1, SCR_H - 1);
    gfx_clear(&S, fill_tab[F_BLACK]);                 /* suelo negro; el mar (agua), verde oscuro */
    msh = 5 + CLAMP(lvl, 0, MAP_LEVELS - 1);
    /* suelo: de mayor a menor radio (el orden del pintor de la vista 3D) */
    ngl = 0;
    for (i = 0; i < ntl; i++) {
        const Obj *o = tl[i]->obj;
        for (k = tl[i]->nobj; k > 0; k--, o++) {
            if (o->type != OBJ_GROUND || !on_screen(o) || (o->radius >> msh) < 1 || ngl >= MAXG) continue;
            for (j = ngl; j > 0 && gl[j - 1]->radius < o->radius; j--) gl[j] = gl[j - 1];
            gl[j] = o; ngl++;
        }
    }
    for (i = 0; i < ngl; i++) draw_obj_ground(gl[i]);
    for (i = 0; i < ntl; i++) {                  /* montanas, luego carreteras y rios, luego edificios */
        const Obj *o = tl[i]->obj;
        for (k = tl[i]->nobj; k > 0; k--, o++)
            if (o->type == OBJ_SOLID && (o->flags & OF_MOUNTAIN) && on_screen(o)) draw_obj_solid(o);
    }
    for (i = 0; i < ntl; i++) {
        const Obj *o = tl[i]->obj;
        for (k = tl[i]->nobj; k > 0; k--, o++) if (o->type == OBJ_LINES && on_screen(o)) draw_obj_lines(o);
    }
    for (i = 0; i < ntl; i++) {
        const Obj *o = tl[i]->obj;
        for (k = tl[i]->nobj; k > 0; k--, o++)
            if (o->type == OBJ_SOLID && !(o->flags & OF_MOUNTAIN) && (o->radius >> msh) >= 1 && on_screen(o)) draw_obj_solid(o);
    }
    for (i = 0; i < w->napt; i++) draw_airport(&w->apt[i]);
    for (i = 0; i < w->nnav; i++) draw_nav(&w->nav[i]);
    /* el avion: cruz con el morro hacia el rumbo */
    {
        s32 sn = isin(h), cs = icos(h), sw = isin((u16)(h + 16384)), cw = icos((u16)(h + 16384));
        int cx = SCR_W / 2, cy = SCR_H / 2;
        for (k = 0; k < 2; k++) {
            gfx_line(&S, cx + k + (int)((-sn * 9) >> 14), cy - (int)((-cs * 9) >> 14), cx + k + (int)((sn * 11) >> 14), cy - (int)((cs * 11) >> 14), C_RED);
            gfx_line(&S, cx + k + (int)((sw * 9) >> 14), cy - (int)((cw * 9) >> 14), cx + k + (int)((-sw * 9) >> 14), cy - (int)((-cw * 9) >> 14), C_RED);
            gfx_line(&S, cx + k + (int)(((-sn * 8) + (sw * 4)) >> 14), cy - (int)(((-cs * 8) + (cw * 4)) >> 14),
                     cx + k + (int)(((-sn * 8) - (sw * 4)) >> 14), cy - (int)(((-cs * 8) - (cw * 4)) >> 14), C_RED);
        }
    }
    /* norte, escala y ayuda */
    gfx_line(&S, SCR_W - 20, 26, SCR_W - 20, 8, C_WHITE);
    gfx_line(&S, SCR_W - 20, 8, SCR_W - 23, 13, C_WHITE);
    gfx_line(&S, SCR_W - 20, 8, SCR_W - 17, 13, C_WHITE);
    gfx_text(&S, SCR_W - 23, 29, "N", C_WHITE);
    lm = km[CLAMP(lvl, 0, MAP_LEVELS - 1)];
    x = (int)((lm * 1000L) >> msh); y = SCR_H - 14;
    gfx_line(&S, 10, y, 10 + x, y, C_WHITE);
    gfx_line(&S, 10, y - 3, 10, y + 3, C_WHITE);
    gfx_line(&S, 10 + x, y - 3, 10 + x, y + 3, C_WHITE);
    u_num(b, lm, 1, ' '); u_cat(b, " KM", 24);
    gfx_text(&S, 16 + x, y - 3, b, C_WHITE);
    gfx_text(&S, 270, SCR_H - 17, "MAPA   Q = VOLVER   + - = ESCALA", C_WHITE);
}
