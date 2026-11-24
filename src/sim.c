/* sim.c - bucle principal: teclado, fisica a paso fijo, vistas, panel y sonido */
#include "sim.h"
#include "platform.h"
#include "sound.h"
#include "util.h"
#include <string.h>

Sim sim;
static u8 backbuf[VIEW_H * SCR_STRIDE];

static int down(int k) { return sim.keys[k]; }
static int hit(int k) { return sim.keys[k] && !sim.prev[k]; }

static void read_keys(void)
{
    int r, b, v;
    memcpy(sim.prev, sim.keys, 64);
    for (r = 0; r < 8; r++) {
        v = plat_keyrow(r);
        for (b = 0; b < 8; b++) sim.keys[r * 8 + b] = (u8)((v >> b) & 1);
    }
}

void sim_reset(void)
{
    const Airport *ap = &sim.w.apt[sim.start_apt];
    const Runway *rw = &ap->rwy[0];
    s32 sx = isin(rw->hdg), cx = icos(rw->hdg);
    flight_init(&sim.f, &sim.ac);
    /* 60 m despues del umbral, en el eje */
    if (sim.ac.eng_type == ENG_NONE)   /* planeador: empieza en el aire, 4 km antes del umbral */
        flight_place(&sim.f, rw->tx - (sx * 4000 >> 14), rw->ty - (cx * 4000 >> 14),
                     ap->elev + 600, rw->hdg, 30, 0);
    else
        flight_place(&sim.f, rw->tx + (sx * 60 >> 14), rw->ty + (cx * 60 >> 14),
                     ap->elev + (sim.ac.gear_h >> 16), rw->hdg, 0, 1);
    sim.f.wind_e = 0; sim.f.wind_n = -FXI(3);      /* 6 nudos del norte */
    nav_init(&sim.r);
    sim.r.com = ap->twr;
    sim.r.obs1 = rw->hdg;
    atc_init(&sim.atc);
    sim.crash_shown = 0;
    sim.nsteps = 0;
}

int sim_init(const char *scn, const char *acf, int hour)
{
    memset(&sim, 0, sizeof(sim));
    if (!world_open(&sim.w, scn)) return -1;
    if (!aircraft_load(&sim.ac, acf)) return -2;
    sim.scr = plat_screen();
    gfx_init_surface(&sim.view, backbuf, VIEW_H);
    gfx_clip(&sim.view, VIEW_X0, 0, VIEW_X1, VIEW_H - 1);
    memset(backbuf, 0, sizeof(backbuf));
    memset(sim.scr, 0, SCR_BYTES);
    cam_zoom(&sim.cam, 256);
    sim.zoom = 256;
    sim.cam.vis = sim.w.vis_km ? (s32)sim.w.vis_km * 1000L : 24000L;
    sim.clock = (u32)hour * 3600UL;
    sim.start_apt = 0;
    sim_reset();
    panel_init(sim.scr, &sim.ac);
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
        if (sh) { if (f->flaps > 0) f->flaps--; }
        else if (f->flaps < sim.ac.n_flaps - 1) f->flaps++;
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
    if (hit(KEY_ESC)) sim.quit = 1;
    if (f->crash && hit(KEY_ENTER)) sim_reset();
    if (hit(KEY_R)) {                       /* siguiente aeropuerto de salida */
        sim.start_apt = (sim.start_apt + (sh ? sim.w.napt - 1 : 1)) % sim.w.napt;
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

void sim_render(void)
{
    s16 am[9];
    PanelInfo pi;
    setup_camera(am);
    view_sky(&sim.view, &sim.cam);
    view_world(&sim.view, &sim.cam, &sim.w);
    if (sim.view_mode != VIEW_COCKPIT)
        view_model(&sim.view, &sim.cam, &sim.ac.model, sim.f.x, sim.f.y, sim.f.z - (sim.ac.gear_h >> 8), am);
    if (sim.f.crash) {
        const char *m = crash_msg(sim.f.crash);
        gfx_rect(&sim.view, VIEW_CX - gfx_textw(m) / 2 - 4, 40, VIEW_CX + gfx_textw(m) / 2 + 4, 52, fill_tab[F_BLACK]);
        gfx_text(&sim.view, VIEW_CX - gfx_textw(m) / 2, 43, m, C_RED);
    }
    gfx_memcpy(sim.scr + (s32)VIEW_Y0 * SCR_STRIDE, backbuf, (s32)VIEW_H * SCR_STRIDE);
    pi.clock = sim.clock;
    pi.warn = build_status();
    pi.status = sim.status;
    panel_update(sim.scr, &sim.f, &sim.r, &sim.atc, &pi, sim.part);
    sim.part = (sim.part + 1) % PANEL_PARTS;
}

void sim_frame(void)
{
    u32 now;
    int n = 0, contact;
    read_keys();
    controls_frame();
    contact = hit(KEY_K);
    now = plat_ticks();
    while ((s32)(now - sim.tick_phys) >= 2 && n < 4) {
        sim.tick_phys += 2;
        if (sim.paused) continue;
        controls_step();
        flight_step(&sim.f, &sim.w);
        sim.nsteps++;
        if (++sim.clock_sub >= PHYS_HZ) { sim.clock_sub = 0; sim.clock = (sim.clock + 1) % 86400UL; }
        if ((sim.nsteps % 12) == 0) {
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
    sim_render();
    snd_update(&sim.f, &sim.r, sim.atc.new_msg, sim.f.crash);
    sim.atc.new_msg = 0;
    sim.frames++;
    plat_frame();
}

/* --------------------------------------------------------------- menu */
int sim_menu(char *acf, char *scn, int *hour)
{
    static const char *names[4] = { "planeador.acf", "mono.acf", "bimotor.acf", "reactor.acf" };
    static const char *desc[4] = { "1  PLANEADOR (SIN MOTOR)", "2  MONOMOTOR 160 HP, PASO FIJO",
                                   "3  BIMOTOR 2x300 HP, PASO VARIABLE", "4  REACTOR BIMOTOR" };
    static const char *scns[2] = { "demo.scn", "iberia.scn" };
    static const char *scnd[2] = { "E  ESCENARIO: VALLE DEL QL (DEMO)", "E  ESCENARIO: PENINSULA IBERICA" };
    static int esc = 1;
    Surface s;
    int sel = 1, i, done = 0;
    char b[40];
    u8 *scr = plat_screen();
    gfx_init_surface(&s, scr, SCR_H);
    *hour = 12;
    while (!done) {
        read_keys();
        if (hit(KEY_1)) sel = 0;
        if (hit(KEY_2)) sel = 1;
        if (hit(KEY_3)) sel = 2;
        if (hit(KEY_4)) sel = 3;
        if (hit(KEY_T)) *hour = (*hour + (down(KEY_SHIFT) ? 23 : 1)) % 24;
        if (hit(KEY_E)) esc ^= 1;
        if (hit(KEY_ENTER)) done = 1;
        if (hit(KEY_ESC)) return 0;
        gfx_clear(&s, fill_tab[F_BLACK]);
        gfx_text(&s, 150, 20, "QFS - SIMULADOR DE VUELO PARA SINCLAIR QL", C_WHITE);
        gfx_text(&s, 150, 30, "=========================================", C_RED);
        for (i = 0; i < 4; i++) gfx_text(&s, 150, 60 + i * 12, desc[i], i == sel ? C_GREEN : C_WHITE);
        u_cpy(b, "T  HORA DE SALIDA: ", 40); u_catn(b, *hour, 2, '0', 40); u_cat(b, ":00", 40);
        gfx_text(&s, 150, 120, b, C_WHITE);
        gfx_text(&s, 150, 132, scnd[esc], C_WHITE);
        gfx_text(&s, 150, 175, "EN VUELO: R = SIGUIENTE AEROPUERTO", C_GREEN);
        gfx_text(&s, 150, 150, "ENTER = VOLAR     ESC = SALIR", C_GREEN);
        plat_frame();
    }
    plat_path(acf, names[sel]);
    plat_path(scn, scns[esc]);
    return 1;
}
