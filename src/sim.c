/* sim.c - bucle principal: teclado, fisica a paso fijo, vistas, panel y sonido */
#include "sim.h"
#include "platform.h"
#include "sound.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

Sim sim;
static u8 *backbuf;                      /* vista 3D: se dibuja aqui (RAM alta, sin contencion) y se vuelca de golpe a la pantalla */

static int down(int k) { return sim.keys[k]; }
static int hit(int k) { return sim.keys[k] && !sim.prev[k]; }

/* Lectura del teclado. Cada fila cuesta una transaccion con el IPC (MT.IPCOM, ~125000 ciclos), asi que en el QL solo se leen
 * 3 filas por cuadro y el resto conserva su ultimo valor (sim.keys es una foto que se refresca por filas):
 *   - fila 1 (cursores, ESPACIO, ENTER, ESC, \) en todos los cuadros;
 *   - fila 0 (F1-F4, 5, 4) y fila 2 (M, punto, Z, C, ]) alternadas: cada una un cuadro si y otro no;
 *   - una de las otras cinco (3, 4, 5, 6, 7) por cuadro, en rotacion: cada una cada 5 cuadros.
 * full = 1 lee las 8 filas (el menu no tiene prisa; en el PC siempre se leen todas). */
#if defined(PLAT_QL) || defined(KEY_PARTIAL)
#define KEY_SCAN_PARTIAL 1
#endif


static void scan_row(int r)
{
    int b, v = plat_keyrow(r);
    for (b = 0; b < 8; b++) sim.keys[r * 8 + b] = (u8)((v >> b) & 1);
}

static void read_keys(int full)
{
    int r;
#ifdef KEY_SCAN_PARTIAL
    static int kc;
    static const u8 rest[5] = { 3, 4, 5, 6, 7 };
#endif
    memcpy(sim.prev, sim.keys, 64);
#ifdef KEY_SCAN_PARTIAL
    if (!full) {
        scan_row(1);
        scan_row((kc & 1) ? 2 : 0);
        scan_row(rest[kc % 5]);
        kc++;
        return;
    }
#endif
    for (r = 0; r < 8; r++) scan_row(r);
}

int sim_start_apt, sim_start_mode, sim_start_rwy, sim_start_pt;   /* los fija sim_menu: aeropuerto, tipo de salida (START_*), pista/cabecera y punto de la senda */
int sim_restore;                         /* 1: sim_init restaura el vuelo guardado (snap) tras colocar el avion */

static u32 air_rnd(void)                 /* 16 bits pseudoaleatorios; semilla: reloj de 50 Hz */
{
    static u32 st;
    if (!st) st = plat_ticks() * 2654435761UL + 1;
    st = st * 1103515245UL + 12345UL;
    return (st >> 16) & 0xFFFFUL;
}

static s32 air_pos(s32 org, s32 span)    /* posicion al azar en el mapa, dejando un 10% de margen en cada borde */
{
    s32 m = span / 10;
    return org + m + (((span - 2 * m) >> 8) * (s32)air_rnd() >> 8);
}

/* Avion en vuelo a velocidad de mejor planeo (CL = raiz(CD0/k): minimo cociente CD/CL), tren y flaps arriba. La incidencia y el
 * compensador se ponen para que el vuelo empiece equilibrado a esa velocidad. */
static void place_glide(s32 x, s32 y, s32 z, u16 hdg)
{
    const AircraftDef *a = &sim.ac;
    Flight *f = &sim.f;
    s32 sg, cl, v2, alf;
    sg = fxlerp_tab(atm_sigma, 17, (z << 16) / 1000);
    cl = a->k_ind > 0 ? isqrt32((u32)fxdiv(a->cd0, a->k_ind)) << 8 : FXF(7, 10);
    if (cl < FXF(2, 10)) cl = FXF(2, 10);
    v2 = fxdiv(642908L, fxmul(fxmul(f->KL, sg), cl));            /* g / (KL sigma CL) */
    alf = fxdiv(cl - a->cl0, a->cla);
    alf = CLAMP(alf, -3 * FX_DEG, fxmul(a->alpha_stall, FXF(8, 10)));
    flight_place(f, x, y, z, hdg, 0, 0);
    f->v = isqrt32((u32)v2) << 8;
    f->alf = f->the = alf;
    f->trim = CLAMP(fxdiv(alf - 2 * FX_DEG, a->pitch_auth), -FX_ONE, FX_ONE);
    if (a->retract) { f->gear_down = 0; f->gear_pos = 0; }
    if (a->eng_type != ENG_NONE) f->thr[0] = f->thr[1] = FXF(6, 10);
}

/* Salida en vuelo (tecla S del menu): 33% del techo (o 300 m sobre el terreno si es mas alto), rumbo y posicion al azar. */
static void place_air(void)
{
    const AircraftDef *a = &sim.ac;
    World *w = &sim.w;
    s32 x = air_pos(w->org_x, (s32)w->ntx << w->tile_log2);
    s32 y = air_pos(w->org_y, (s32)w->nty << w->tile_log2);
    s32 z = a->max_alt / 3, gnd = world_ground(w, x, y);
    u16 hdg = (u16)air_rnd();
    if (z < gnd + 300) z = gnd + 300;
    place_glide(x, y, z, hdg);
}

/* Puntos de la senda de descenso (distancia al umbral en m, sobre la prolongacion del eje): 20 NM, IF (10 NM), FAF (5 NM) y
 * corta final (1,5 NM). Los de las cartas de aproximacion (tools/procdesign.py) son el IF y el FAF. */
const s32 land_dist[LAND_PTS] = { 37040L, 18520L, 9260L, 2778L };

/* Aterrizando: sobre la prolongacion del eje de la pista, en la senda de 3 grados (tan 3 = 0,0524: 1 m por cada 19 m) hacia un
 * punto a 300 m del umbral, con 60 m de margen sobre el terreno. */
static void place_land(const Airport *ap, const Runway *rw, s32 sx, s32 cx)
{
    s32 d = land_dist[CLAMP(sim.start_pt, 0, LAND_PTS - 1)];
    s32 x = rw->tx - ((sx * d) >> 14), y = rw->ty - ((cx * d) >> 14);
    s32 z = ap->elev + ((d + 300) * 54 >> 10), gnd = world_ground(&sim.w, x, y);
    if (z < gnd + 60) z = gnd + 60;
    place_glide(x, y, z, rw->hdg);
    if (sim.ac.eng_type != ENG_NONE) sim.f.thr[0] = sim.f.thr[1] = FXF(sim.ac.eng_type == ENG_JET ? LAND_THR_JET : LAND_THR, 100);   /* potencia de descenso */
}

void sim_reset(void)
{
    const Airport *ap = &sim.w.apt[sim.start_apt];
    const Runway *rw;
    s32 sx, cx;
    s32 px, py;
    u16 ph;
    if (sim.start_rwy >= ap->nrwy) sim.start_rwy = 0;
    rw = &ap->rwy[sim.start_rwy];               /* pista y sentido elegidos (cada cabecera es una entrada) */
    sx = isin(rw->hdg); cx = icos(rw->hdg);
    sim.vpart = 0;                              /* la vista que estuviera a medias se abandona */
    flight_init(&sim.f, &sim.ac);
    if (sim.start_mode == START_AIR) place_air();
    else if (sim.start_mode == START_LAND) place_land(ap, rw, sx, cx);
    else if (sim.start_mode == START_RAMP) {    /* plataforma del aeropuerto (o, si no la tiene, junto a la pista) */
        if (ap->has_plat) {
            px = ap->x + ap->plat_dx; py = ap->y + ap->plat_dy;
            ph = ap->plat_hdg;
            if ((u16)(ph - rw->hdg) > 16384U && (u16)(ph - rw->hdg) < 49152U) ph += 32768U;   /* el sentido mas parecido al de la pista */
        } else {
            px = rw->tx + ((sx * (s32)rw->len) >> 15) + ((cx * ((s32)rw->wid / 2 + 40)) >> 14);
            py = rw->ty + ((cx * (s32)rw->len) >> 15) - ((sx * ((s32)rw->wid / 2 + 40)) >> 14);
            ph = rw->hdg;
        }
        flight_place(&sim.f, px, py, ap->elev + (sim.ac.gear_h >> 16), ph, 0, 1);
    }
    /* 60 m despues del umbral, en el eje */
    else if (sim.ac.eng_type == ENG_NONE)   /* planeador: empieza en el aire, 4 km antes del umbral */
        flight_place(&sim.f, rw->tx - (sx * 4000 >> 14), rw->ty - (cx * 4000 >> 14),
                     ap->elev + 600, rw->hdg, 30, 0);
    else
        flight_place(&sim.f, rw->tx + (sx * 60 >> 14), rw->ty + (cx * 60 >> 14),
                     ap->elev + (sim.ac.gear_h >> 16), rw->hdg, 0, 1);
    sim.f.wind_e = 0; sim.f.wind_n = -FXI(3);      /* 6 nudos del norte */
    nav_init(&sim.r);
    sim.r.com = ap->twr;
    sim.r.obs1 = rw->hdg;
    if (sim.start_mode == START_AIR || sim.start_mode == START_LAND) {
        s32 d;
        const Airport *na = world_nearest_airport(&sim.w, sim.f.x >> 8, sim.f.y >> 8, &d);
        if (na) sim.r.com = na->twr;
        if (sim.start_mode == START_LAND) sim.r.com = ap->twr;
        sim.r.obs1 = (u16)fxmul(sim.f.psi, RAD2BAM);
    }
    atc_init(&sim.atc);
    sim.crash_shown = 0;
    sim.nsteps = 0;
}

/* --------------------------------------------------------------- partida
 * Una partida es una foto del vuelo: avion, radios, hora, vista y escenario. Se toma al volver al menu (para CONTINUAR VUELO) y
 * se puede guardar en disco (PARTIDA.SAV) o cargar de el. El fichero es el volcado crudo de la estructura: solo vale en la
 * maquina (y version) que lo escribio, y se descarta si cambia su tamano. */
#define SNAP_MAGIC 0x51465331UL            /* "QFS1" */

typedef struct {
    u32 magic, size;
    int sel, esc;                          /* avion y escenario (indices del menu) */
    Flight f;
    Radios r;
    u32 clock;
    int clock_sub, view_mode, look, zoom;
    int start_apt, start_mode, start_rwy, start_pt;
} Snap;

static Snap sim_snap;                             /* ultima foto */
static int sim_snap_ok;                           /* sim_snap contiene un vuelo */

static void snap_take(int sel, int esc)
{
    Snap *p = &sim_snap;
    p->magic = SNAP_MAGIC; p->size = sizeof(Snap);
    p->sel = sel; p->esc = esc;
    p->f = sim.f; p->r = sim.r;
    p->clock = sim.clock; p->clock_sub = sim.clock_sub;
    p->view_mode = sim.view_mode; p->look = sim.look; p->zoom = sim.zoom;
    p->start_apt = sim.start_apt; p->start_mode = sim.start_mode; p->start_rwy = sim.start_rwy; p->start_pt = sim.start_pt;
    sim_snap_ok = 1;
}

static void snap_restore(void)
{
    Snap *p = &sim_snap;
    sim.f = p->f;
    sim.f.ac = &sim.ac;                    /* punteros que no valen de una ejecucion a otra */
    sim.f.hit_wait = 0; sim.f.hit_gen = 0;
    sim.r = p->r;
    sim.r.n1 = sim.r.n2 = NULL;
    sim.clock = p->clock; sim.clock_sub = p->clock_sub;
    sim.view_mode = p->view_mode; sim.look = p->look; sim.zoom = p->zoom;
    sim.nsteps = 25 * 6;                   /* sin el rotulo del aeropuerto en la linea de estado */
}

static int snap_save(void)
{
    char path[48];
    FILE *fp;
    int ok;
    if (!sim_snap_ok) return 0;
    plat_path(path, "partida.sav");
    fp = fopen(path, "wb");
    if (!fp) return 0;
    ok = fwrite(&sim_snap, sizeof(Snap), 1, fp) == 1;
    fclose(fp);
    return ok;
}

static int snap_load(void)                        /* 1 = hay partida valida y queda en sim_snap */
{
    static Snap t;
    char path[48];
    FILE *fp;
    int ok;
    plat_path(path, "partida.sav");
    fp = fopen(path, "rb");
    if (!fp) return 0;
    ok = fread(&t, sizeof(Snap), 1, fp) == 1 && t.magic == SNAP_MAGIC && t.size == sizeof(Snap) &&
         t.sel >= 0 && t.sel < 4 && t.esc >= 0 && t.esc < 2;
    fclose(fp);
    if (ok) { sim_snap = t; sim_snap_ok = 1; }
    return ok;
}

int sim_init(const char *scn, const char *acf, int hour)
{
    memset(&sim, 0, sizeof(sim));
    if (!world_open(&sim.w, scn)) return -1;
    if (!aircraft_load(&sim.ac, acf)) return -2;
    sim.scr = plat_screen();
    if (!backbuf) backbuf = (u8 *)plat_alloc_hi((long)VIEW_H * SCR_STRIDE);
    if (!backbuf) return -3;
    gfx_init_surface(&sim.view, backbuf, VIEW_H);
    gfx_clip(&sim.view, VIEW_X0, 0, VIEW_X1, VIEW_H - 1);
    memset(backbuf, 0, (size_t)VIEW_H * SCR_STRIDE);
    memset(sim.scr, 0, SCR_BYTES);
    cam_zoom(&sim.cam, 256);
    sim.zoom = 256;
    sim.cam.vis = sim.w.vis_km ? (s32)sim.w.vis_km * 1000L : 24000L;
    sim.clock = (u32)hour * 3600UL;
    sim.start_apt = (sim_start_apt >= 0 && sim_start_apt < sim.w.napt) ? sim_start_apt : 0;
    sim.start_mode = sim_start_mode;
    sim.start_rwy = sim_start_rwy;
    sim.start_pt = sim_start_pt;
    sim_reset();
    if (sim_restore) { sim_restore = 0; snap_restore(); }
    panel_init(sim.scr, &sim.ac, acf);
    snd_init();
    sim.tick_phys = plat_ticks();
    return 0;
}

/* --------------------------------------------------------------- mandos */
static s32 step_toward(s32 v, s32 target, s32 rate)
{
    if (v < target) return MIN(v + rate, target);
    return MAX(v - rate, target);
}

static void controls_step(void)
{
    Flight *f = &sim.f;
    s32 r = PHYS_DT;                  /* 1 unidad por segundo */
    int sh = down(KEY_SHIFT), i;
    if (down(KEY_UP)) f->elev = MAX(f->elev - r, -FX_ONE);
    if (down(KEY_DOWN)) f->elev = MIN(f->elev + r, FX_ONE);
    if (down(KEY_LEFT)) f->ail = MAX(f->ail - 2 * r, -FX_ONE);
    else if (down(KEY_RIGHT)) f->ail = MIN(f->ail + 2 * r, FX_ONE);
    else f->ail = step_toward(f->ail, 0, 2 * r);
    if (down(KEY_Z)) f->rud = MAX(f->rud - 2 * r, -FX_ONE);
    else if (down(KEY_X)) f->rud = MIN(f->rud + 2 * r, FX_ONE);
    else f->rud = step_toward(f->rud, 0, 2 * r);
    if (down(KEY_SPACE)) { f->elev = 0; f->ail = 0; f->rud = 0; }
    if (down(KEY_LBR)) f->trim = MAX(f->trim - r / 4, -FX_ONE);
    if (down(KEY_RBR)) f->trim = MIN(f->trim + r / 4, FX_ONE);
    for (i = 0; i < 2; i++) {
        if (down(KEY_F1)) f->thr[i] = 0;
        if (down(KEY_F2)) f->thr[i] = MAX(f->thr[i] - r / 2, 0);
        if (down(KEY_F3)) f->thr[i] = MIN(f->thr[i] + r / 2, FX_ONE);
        if (down(KEY_F4)) f->thr[i] = FX_ONE;
        if (down(KEY_M)) f->mix[i] = sh ? MIN(f->mix[i] + r / 4, FX_ONE) : MAX(f->mix[i] - r / 4, 0);
        if (down(KEY_P)) f->prop[i] = sh ? MIN(f->prop[i] + r / 4, FX_ONE) : MAX(f->prop[i] - r / 4, 0);
    }
    f->brakes = down(KEY_DOT);
    f->starter = down(KEY_E);
}

static void controls_frame(void)
{
    Flight *f = &sim.f;
    Radios *rd = &sim.r;
    int sh = down(KEY_SHIFT);
    if (hit(KEY_F)) {
        if (sh) { if (f->flaps < sim.ac.n_flaps - 1) f->flaps++; }      /* F: hacia 0 (recoge); Mayus+F: mas flaps */
        else if (f->flaps > 0) f->flaps--;
    }
    if (hit(KEY_G) && sim.ac.retract) f->gear_down = !f->gear_down;
    if (hit(KEY_V)) sim.view_mode = (sim.view_mode + 1) & 3;
    if (hit(KEY_5)) sim.look = 0;
    if (hit(KEY_4)) sim.look = -16384;
    if (hit(KEY_6)) sim.look = 16384;
    if (hit(KEY_8)) sim.look = 32767;
    if (hit(KEY_EQ)) sim.zoom = MIN(sim.zoom * 2, 2048);
    if (hit(KEY_MINUS)) sim.zoom = MAX(sim.zoom / 2, 128);
    if (hit(KEY_N)) rd->nav1 = (u16)(sh ? (rd->nav1 > 10800 ? rd->nav1 - 5 : 11795) : (rd->nav1 < 11795 ? rd->nav1 + 5 : 10800));
    if (hit(KEY_H)) rd->nav2 = (u16)(sh ? (rd->nav2 > 10800 ? rd->nav2 - 5 : 11795) : (rd->nav2 < 11795 ? rd->nav2 + 5 : 10800));
    if (hit(KEY_C)) rd->com = (u16)(sh ? (rd->com > 11800 ? rd->com - 5 : 13695) : (rd->com < 13695 ? rd->com + 5 : 11800));
    if (hit(KEY_A)) rd->adf = (u16)(sh ? (rd->adf > 190 ? rd->adf - 5 : 1750) : (rd->adf < 1750 ? rd->adf + 5 : 190));
    if (down(KEY_O)) rd->obs1 = (u16)(rd->obs1 + (sh ? -182 : 182));
    if (hit(KEY_Y)) f->auto_coord = !f->auto_coord;
    if (hit(KEY_T)) sim.clock = (sim.clock + (sh ? 23UL : 1UL) * 3600UL) % 86400UL;
    if (hit(KEY_TAB)) sim.paused = !sim.paused;
    if (hit(KEY_BSL)) { sim.show_fps = !sim.show_fps; sim.fps_t0 = plat_ticks(); sim.fps_n = sim.fps_v = 0; sim.fps10 = sim.lps10 = -1; }   /* \ : indicador de FPS */
    if (hit(KEY_ESC)) sim.quit = 1;
    if (f->crash && hit(KEY_ENTER)) sim_reset();
    if (hit(KEY_R)) {                       /* siguiente aeropuerto de salida */
        sim.start_apt = (sim.start_apt + (sh ? sim.w.napt - 1 : 1)) % sim.w.napt;
        sim.start_mode = START_RWY;         /* R: salida desde pista */
        sim.start_rwy = 0;
        sim_reset();
    }
}

/* --------------------------------------------------------------- camara */
static void cam_look(Camera *c, const s16 *A, u16 off)
{
    s32 co = icos(off), so = isin(off);
    int k;
    for (k = 0; k < 3; k++) {
        c->m[6 + k] = (s16)((A[6 + k] * co + A[k] * so) >> 14);
        c->m[k] = (s16)((A[k] * co - A[6 + k] * so) >> 14);
        c->m[3 + k] = A[3 + k];
    }
}

static void setup_camera(s16 *am)
{
    Flight *f = &sim.f;
    Camera *c = &sim.cam;
    u16 h = (u16)fxmul(f->psi, RAD2BAM);
    s16 p = (s16)fxmul(f->the, RAD2BAM), b = (s16)fxmul(f->phi, RAD2BAM);
    s32 sh = isin(h), ch = icos(h), dx, dy, dz, d;
    int z = sim.zoom;
    u32 hr = (sim.clock / 3600UL) % 24;
    attitude_matrix(h, p, b, am);
    c->light = (hr < 6 || hr >= 21) ? LIGHT_NIGHT : ((hr < 7 || hr >= 19) ? LIGHT_DUSK : LIGHT_DAY);
    c->ground = (s32)world_ground(&sim.w, f->x >> 8, f->y >> 8) << 8;
    switch (sim.view_mode) {
    case VIEW_COCKPIT:
        cam_set(c, f->x, f->y, f->z, h, p, b);
        if (sim.look) cam_look(c, am, (u16)sim.look);
        break;
    case VIEW_CHASE:
        {   /* detras y algo a la izquierda (15 grados), mirando al avion */
            u16 hc = (u16)(h + 2731);
            s32 sc = isin(hc), cc = icos(hc);
            cam_set(c, f->x - ((sc * 40) >> 6), f->y - ((cc * 40) >> 6), f->z + (8L << 8), hc, -1100, 0);
        }
        break;
    case VIEW_SPOT:
        cam_set(c, f->x + ((ch * 30) >> 6) + ((sh * 10) >> 6), f->y - ((sh * 30) >> 6) + ((ch * 10) >> 6),
                f->z + (3L << 8), (u16)(h - 18000U), -700, 0);
        break;
    default: {
        const Airport *ap = world_nearest_airport(&sim.w, f->x >> 8, f->y >> 8, NULL);
        s32 tx = (s32)ap->x << 8, ty = (s32)ap->y << 8, tz = ((s32)ap->elev + 27) << 8;  /* la torre esta en el punto de referencia */
        dx = (f->x - tx) >> 8; dy = (f->y - ty) >> 8; dz = (f->z - tz) >> 8;
        d = isqrt32((u32)(MULS16(dx >> 4, dx >> 4) + MULS16(dy >> 4, dy >> 4))) << 4;
        cam_set(c, tx, ty, tz, iatan2(dy, dx), (s16)iatan2(d, dz), 0);
        z = (int)CLAMP(d * 256 / 250, 256, 4096);
        break; }
    }
    cam_zoom(c, z);
}

/* --------------------------------------------------------------- pantalla */
static const char *crash_msg(int c)
{
    switch (c) {
    case CRASH_GEAR: return "CRASH! - BAJE EL TREN LA PROXIMA VEZ";
    case CRASH_BUILDING: return "BUILDING CRASH";
    case CRASH_MOUNTAIN: return "MOUNTAIN CRASH";
    case CRASH_WATER: return "SPLASH!";
    case CRASH_OVERSTRESS: return "ROTURA ESTRUCTURAL - VNE EXCEDIDA";
    default: return "CRASH!";
    }
}

static int build_status(void)
{
    static const char *vn[4] = { "CABINA", "PERSECUCION", "TORRE", "LATERAL" };
    Flight *f = &sim.f;
    int warn = 0;
    sim.status[0] = 0;
    if (f->crash) {
        u_cat(sim.status, crash_msg(f->crash), 84);
        u_cat(sim.status, "   ENTER = REINICIAR", 84);
        return 1;
    }
    u_cat(sim.status, vn[sim.view_mode], 84);
    u_cat(sim.status, "  ", 84);
    u_cat(sim.status, sim.ac.name, 84);
    if (sim.paused) u_cat(sim.status, "  PAUSA", 84);
    if (sim.nsteps < 25 * 6) { u_cat(sim.status, "  ", 84); u_cat(sim.status, sim.w.apt[sim.start_apt].name, 84); }
    if (f->stall_warn) { u_cat(sim.status, "  PERDIDA!", 84); warn |= 1; }
    if (sim.ac.retract && !f->gear_down && f->agl < FXI(150) && !f->on_ground) { u_cat(sim.status, "  TREN!", 84); warn |= 2; }
    if (!f->auto_coord) u_cat(sim.status, "  TIMON MANUAL", 84);
    if (sim.ac.n_flaps > 1) {
        u_cat(sim.status, "  FLAPS ", 84);
        u_catn(sim.status, sim.ac.flap_deg[f->flaps], 1, ' ', 84);
    }
    return warn;
}

/* La vista 3D se parte en DOS cuadros para que ni el teclado ni el panel esperen a que termine entera:
 *   cuadro A: camara, cielo, reunir y ordenar objetos y dibujar los primeros hasta gastar ~VIEW_SPLIT % del coste;
 *   cuadro B: el resto de objetos, el avion (vistas exteriores), el aviso de choque y el volcado del buffer a la pantalla.
 * Teclado, fisica, radios, ATC y panel corren en todos los cuadros. La camara y el avion se fijan en A, asi que la imagen
 * que sale en B es coherente (aunque el avion se haya movido entre A y B). VIEW_SPLIT = 0: la vista entera en un cuadro. */
#ifndef VIEW_SPLIT
#define VIEW_SPLIT 45
#endif
#if VIEW_SPLIT
#define PHYS_MAX 2          /* pasos de fisica por cuadro: con la vista en dos cuadros siguen siendo 4 por vista */
#else
#define PHYS_MAX 4
#endif

static s16 view_am[9];                        /* actitud y posicion del avion fijadas en el cuadro A */
static s32 view_ax, view_ay, view_az;
static int view_mode_a;

static void view_part_a(int split)
{
    s32 total;
    setup_camera(view_am);
    view_ax = sim.f.x; view_ay = sim.f.y; view_az = sim.f.z - (sim.ac.gear_h >> 8);
    view_mode_a = sim.view_mode;
    view_sky(&sim.view, &sim.cam);
    total = view_world_begin(&sim.cam, &sim.w);
    view_world_step(&sim.view, &sim.cam, split ? total / 100 * VIEW_SPLIT : 0x7FFFFFFFL);
}

static void view_part_b(void)
{
    view_world_step(&sim.view, &sim.cam, 0x7FFFFFFFL);
    if (view_mode_a != VIEW_COCKPIT)
        view_model(&sim.view, &sim.cam, &sim.ac.model, view_ax, view_ay, view_az, view_am,
                   !(sim.ac.retract && sim.f.gear_pos <= 0));      /* tren recogido del todo: no se ve */
    if (sim.f.crash) {
        const char *m = crash_msg(sim.f.crash);
        gfx_rect(&sim.view, VIEW_CX - gfx_textw(m) / 2 - 4, 40, VIEW_CX + gfx_textw(m) / 2 + 4, 52, fill_tab[F_BLACK]);
        gfx_text(&sim.view, VIEW_CX - gfx_textw(m) / 2, 43, m, C_RED);
    }
    gfx_memcpy(sim.scr + (s32)VIEW_Y0 * SCR_STRIDE, backbuf, (s32)VIEW_H * SCR_STRIDE);
}

static void render_panel(void)
{
    PanelInfo pi;
    pi.clock = sim.clock;
    pi.warn = build_status();
    pi.status = sim.status;
    pi.fps10 = sim.show_fps ? sim.fps10 : -2;     /* -2 oculto, -1 midiendo */
    pi.lps10 = sim.lps10;
    panel_update(sim.scr, &sim.f, &sim.r, &sim.atc, &pi, sim.part);
    sim.part = (sim.part + 1) % PANEL_PARTS;
}

void sim_render(void)                            /* vista entera y panel (lo usa el PC para las capturas) */
{
    view_part_a(0);
    view_part_b();
    render_panel();
}

static int nav_sub;                              /* pasos de fisica desde la ultima actualizacion de navegacion y ATC */

void sim_frame(void)
{
    u32 now;
    int n = 0, contact;
    read_keys(0);
    controls_frame();
    contact = hit(KEY_K);
    now = plat_ticks();
    while ((s32)(now - sim.tick_phys) >= 2 && n < PHYS_MAX) {
        sim.tick_phys += 2;
        if (sim.paused) continue;
        controls_step();
        flight_step(&sim.f, &sim.w);
        sim.nsteps++;
        if (++sim.clock_sub >= PHYS_HZ) { sim.clock_sub = 0; sim.clock = (sim.clock + 1) % 86400UL; }
        if (++nav_sub >= 12) {                   /* (sin % : en el 68000 es una llamada a la biblioteca) */
            nav_sub = 0;
            nav_update(&sim.r, &sim.w, &sim.f);
            atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 0);
        }
        n++;
    }
    if ((s32)(now - sim.tick_phys) > 8) sim.tick_phys = now;      /* no acumular retraso */
    if (contact) {
        nav_update(&sim.r, &sim.w, &sim.f);
        atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 1);
    }
    atc_scroll(&sim.atc);
#if VIEW_SPLIT
    if (sim.vpart == 0) { view_part_a(1); sim.vpart = 1; }                     /* cuadro A de la vista */
    else { view_part_b(); sim.vpart = 0; sim.fps_v++; }                        /* cuadro B: la vista queda completa */
#else
    view_part_a(0); view_part_b(); sim.fps_v++;
#endif
    render_panel();
    if (sim.show_fps) {                     /* vistas y cuadros de simulacion por segundo (decimas), en ventanas de al menos 1 s */
        u32 dt = plat_ticks() - sim.fps_t0;
        sim.fps_n++;
        if (dt >= 50) {
            sim.fps10 = (int)((u32)sim.fps_v * 500UL / dt); sim.lps10 = (int)((u32)sim.fps_n * 500UL / dt);
            sim.fps_v = sim.fps_n = 0; sim.fps_t0 += dt;
        }
    }
    snd_update(&sim.f, &sim.r, sim.atc.new_msg, sim.f.crash);
    sim.atc.new_msg = 0;
    sim.frames++;
    plat_frame();
}

/* --------------------------------------------------------------- menu */
static World mw;                                 /* escenario del menu, abierto solo para leer la lista de aeropuertos */

static void menu_open(const char *name)
{
    char path[48];
    if (mw.fp) world_close(&mw);
    plat_path(path, name);
    if (!world_open(&mw, path)) { mw.napt = 0; world_close(&mw); }
}

int sim_menu(char *acf, char *scn, int *hour)
{
    static const char *names[4] = { "planeador.acf", "mono.acf", "bimotor.acf", "reactor.acf" };
    static const char *desc[4] = { "1  PLANEADOR (SIN MOTOR)", "2  MONOMOTOR 160 HP, PASO FIJO",
                                   "3  BIMOTOR 2x300 HP, PASO VARIABLE", "4  REACTOR BIMOTOR" };
    static const char *scns[2] = { "demo.scn", "iberia.scn" };
    static const char *scnd[2] = { "E  ESCENARIO: VALLE DEL QL (DEMO)", "E  ESCENARIO: PENINSULA IBERICA" };
    static const char *ptd[LAND_PTS] = { "A 20 NM", "IF 10 NM", "FAF 5 NM", "EN CORTA FINAL" };
    static const int ord[5] = { START_CONT, START_RWY, START_RAMP, START_AIR, START_LAND };   /* orden de la tecla S */
    static int esc = 1, apt = 0, mode = START_RWY, rwy = 0, pt = 0, sel = 1;   /* se recuerdan al volver al menu */
    const char *msg = "";
    Surface s;
    int i, k, done = 0, dirty = 1, o_sel, o_hour, o_esc, o_apt, o_mode, o_rwy, o_pt, n;
    char b[64];
    u8 *scr = plat_screen();
    gfx_init_surface(&s, scr, SCR_H);
    if (sim.frames) {                                               /* se vuelve de un vuelo: se guarda y se ofrece continuarlo */
        snap_take(sel, esc);
        mode = START_CONT;
    }
    if (mode == START_CONT && !sim_snap_ok) mode = START_RWY;
    *hour = sim.frames ? (int)((sim.clock / 3600UL) % 24) : 12;     /* tras un vuelo: la hora actual del simulador */
    menu_open(scns[esc]);
    if (apt >= mw.napt) apt = 0;
    while (!done) {
        read_keys(1);
        o_sel = sel; o_hour = *hour; o_esc = esc; o_apt = apt; o_mode = mode; o_rwy = rwy; o_pt = pt;
        if (hit(KEY_1)) sel = 0;
        if (hit(KEY_2)) sel = 1;
        if (hit(KEY_3)) sel = 2;
        if (hit(KEY_4)) sel = 3;
        if (hit(KEY_T)) *hour = (*hour + (down(KEY_SHIFT) ? 23 : 1)) % 24;
        if (hit(KEY_E)) { esc ^= 1; apt = rwy = pt = 0; menu_open(scns[esc]); }
        if (hit(KEY_A) && mode != START_AIR && mode != START_CONT && mw.napt > 0) {
            apt = (apt + (down(KEY_SHIFT) ? mw.napt - 1 : 1)) % mw.napt; rwy = pt = 0;
        }
        if (hit(KEY_S)) {
            for (k = 0; ord[k] != mode; k++) ;
            do k = (k + (down(KEY_SHIFT) ? 4 : 1)) % 5; while (ord[k] == START_CONT && !sim_snap_ok);
            mode = ord[k];
        }
        n = mw.napt > 0 ? mw.apt[apt].nrwy : 0;
        if (hit(KEY_D) && mode == START_RWY && n > 0) rwy = (rwy + (down(KEY_SHIFT) ? n - 1 : 1)) % n;
        if (hit(KEY_D) && mode == START_LAND && n > 0) {            /* punto de la senda; al acabar los de una pista, la siguiente */
            if (down(KEY_SHIFT)) { if (--pt < 0) { pt = LAND_PTS - 1; rwy = (rwy + n - 1) % n; } }
            else if (++pt >= LAND_PTS) { pt = 0; rwy = (rwy + 1) % n; }
        }
        if (hit(KEY_G)) {
            msg = !sim_snap_ok ? "NO HAY VUELO QUE GUARDAR" : (snap_save() ? "PARTIDA GUARDADA" : "NO SE PUDO GUARDAR LA PARTIDA");
            dirty = 1;
        }
        if (hit(KEY_L)) {
            if (snap_load()) { sel = sim_snap.sel; esc = sim_snap.esc; mode = START_CONT; done = 1; menu_open(scns[esc]); }
            else { msg = "NO SE PUDO CARGAR LA PARTIDA"; dirty = 1; }
        }
        if (hit(KEY_ENTER)) done = 1;
        if (hit(KEY_ESC)) { world_close(&mw); return 0; }
        if (sel != o_sel || *hour != o_hour || esc != o_esc || apt != o_apt || mode != o_mode || rwy != o_rwy || pt != o_pt) dirty = 1;
        if (!dirty) { plat_frame(); continue; }     /* solo se redibuja al cambiar algo (sin parpadeo en el QL) */
        dirty = 0;
        gfx_clear(&s, fill_tab[F_BLACK]);
        gfx_text(&s, 150, 20, "QFS - SIMULADOR DE VUELO PARA SINCLAIR QL", C_WHITE);
        gfx_text(&s, 150, 30, "=========================================", C_RED);
        for (i = 0; i < 4; i++) gfx_text(&s, 150, 60 + i * 12, desc[i], i == sel ? C_GREEN : C_WHITE);
        u_cpy(b, "T  HORA DE SALIDA: ", 40); u_catn(b, *hour, 2, '0', 40); u_cat(b, ":00", 40);
        gfx_text(&s, 150, 120, b, C_WHITE);
        gfx_text(&s, 150, 132, scnd[esc], C_WHITE);
        if (mode != START_AIR && mode != START_CONT) {
            u_cpy(b, "A  AEROPUERTO: ", 64);
            if (mw.napt > 0) { u_cat(b, mw.apt[apt].ident, 64); u_cat(b, " ", 64); u_cat(b, mw.apt[apt].name, 64); }
            else u_cat(b, "---", 64);
            gfx_text(&s, 150, 144, b, C_WHITE);
        }
        gfx_text(&s, 150, 156, mode == START_CONT ? "S  SALIDA: CONTINUAR VUELO" :
                               mode == START_AIR ? "S  SALIDA: EN VUELO (33% DEL TECHO)" :
                               mode == START_LAND ? "S  SALIDA: ATERRIZANDO" :
                               (mode == START_RAMP ? "S  SALIDA: EN PLATAFORMA" : "S  SALIDA: EN PISTA"), C_WHITE);
        if ((mode == START_RWY || mode == START_LAND) && n > 0) {
            const Runway *r = &mw.apt[apt].rwy[rwy];
            int deg = (int)((((s32)r->hdg * 360L + 32768L) >> 16) % 360), num = (deg + 5) / 10 % 36;
            u_cpy(b, mode == START_LAND ? "D  FINAL: " : "D  PISTA: ", 64);
            if (mode == START_LAND) { u_cat(b, ptd[pt], 64); u_cat(b, " PISTA ", 64); }
            u_catn(b, num ? num : 36, 2, '0', 64);
            u_cat(b, " RUMBO ", 64); u_catn(b, deg, 3, '0', 64);
            if (mode == START_RWY) { u_cat(b, "  ", 64); u_catn(b, r->len, 4, ' ', 64); u_cat(b, " M", 64); }
            gfx_text(&s, 150, 168, b, C_WHITE);
        }
        gfx_text(&s, 150, 186, "G  GUARDAR PARTIDA", sim_snap_ok ? C_WHITE : C_RED);
        gfx_text(&s, 150, 198, "L  CARGAR PARTIDA", C_WHITE);
        gfx_text(&s, 150, 216, "ENTER = VOLAR     ESC = SALIR", C_GREEN);
        gfx_text(&s, 150, 228, "EN VUELO: R = SIGUIENTE AEROPUERTO", C_RED);
        if (msg[0]) gfx_text(&s, 150, 242, msg, C_GREEN);
        plat_frame();
    }
    world_close(&mw);
    if (mode == START_CONT) {                       /* el vuelo guardado manda: avion, escenario y salida de origen */
        sel = sim_snap.sel; esc = sim_snap.esc;
        sim_start_apt = sim_snap.start_apt; sim_start_mode = sim_snap.start_mode;
        sim_start_rwy = sim_snap.start_rwy; sim_start_pt = sim_snap.start_pt;
        sim_restore = 1;
        *hour = (int)((sim_snap.clock / 3600UL) % 24);
    } else {
        sim_start_apt = apt;
        sim_start_mode = mode;
        sim_start_rwy = rwy;
        sim_start_pt = pt;
        sim_restore = 0;
    }
    plat_path(acf, names[sel]);
    plat_path(scn, scns[esc]);
    return 1;
}
