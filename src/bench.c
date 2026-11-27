/* bench.c - estudio de las interrupciones de 50 Hz (doc/optimizacion.md, «Planificador por turnos»). Solo con -DQFS_PROF -DBENCH_MODE=n; ver tools/ql_bench.sh.
 *   BENCH_MODE 1  coste de cada tarea (fisica, teclado, trozos del panel, vista solida y de alambre): marcas por llamada
 *   BENCH_MODE 2  perfil por funcion de 4 cuadros de alambre        BENCH_MODE 3  igual de la vista solida
 *   BENCH_MODE 4  perfil de 100 pasos de fisica                      BENCH_MODE 5  perfil del panel entero (x4)
 *   BENCH_MODE 6  coste de cada unidad de trabajo de la vista de alambre (compilar con -DBENCH_UNITS)
 *   BENCH_MODE 10 planificador por turnos de 20 ms (B_PAT = patron de 8 o 16 turnos, B_RUN = turnos que dura la prueba)
 * Letras del patron:  F fisica (+ nav/ATC cada 6 pasos)   K una fila del teclado (IPC) y controles   N nada
 *   P panel entero    A mitad (horizonte, velocidad/altitud, variometro/rumbo)    B otra mitad (viraje/rpm/CDI, radios/motor, mensajes)
 *   0..5 un trozo del panel   Q panel entero reanudable (por turnos)   Y navegacion+ATC   E motores   V vista 3D de alambre en trozos (trabaja hasta el final del turno)   S vista solida en trozos
 * Un turno cuyo hueco ha pasado mientras la tarea anterior seguia ocupada se SALTA (se cuenta como "saltado"). */
#if defined(QFS_PROF) && defined(BENCH_MODE)
#include "sim.h"
#include "platform.h"
#include "sound.h"
#include "util.h"
#include "qfs_prof.h"

extern void scan_row(int r);
extern void read_keys(int full);
extern void controls_step(void);
extern void controls_frame(void);
extern int  build_status(void);
extern void view_part_a(int split);
extern void view_part_b(void);

static PanelInfo bpi;

static void pinfo(void)
{
    bpi.clock = sim.clock;
    bpi.warn = build_status();
    bpi.status = sim.status;
    bpi.fps10 = -2;
    bpi.lps10 = 0;
    bpi.pps10 = 0;
}

static void rep(int id, int what, long v)       /* 6 marcas: 250, id, que, y el valor en 4 bytes */
{
    PROF_MARK(250); PROF_MARK(id); PROF_MARK(what);
    PROF_MARK((int)((v >> 24) & 255)); PROF_MARK((int)((v >> 16) & 255)); PROF_MARK((int)((v >> 8) & 255)); PROF_MARK((int)(v & 255));
}

static int jig_dir = 1;
static void jiggle(void)         /* maniobra: alabeo en onda triangular de +-20 grados a 3 grados por actualizacion del panel (y cabeceo +-4) */
{
    sim.f.phi += jig_dir * 3 * FX_DEG;
    if (sim.f.phi > 20 * FX_DEG) jig_dir = -1;
    if (sim.f.phi < -20 * FX_DEG) jig_dir = 1;
    sim.f.the += jig_dir * FX_DEG / 2;
}

#ifndef B_NAVX
#define B_NAVX 0                       /* 1: la navegacion y el ATC no van dentro de la fisica sino en su propia tarea (letra Y) */
#endif
static int nav_n;
static void nav_task(void)
{
    nav_update(&sim.r, &sim.w, &sim.f);
    atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 0);
}
static void phys_step(void)
{
    controls_step();
    flight_step(&sim.f, &sim.w);
    if (!B_NAVX && ++nav_n >= 6) {
        nav_n = 0;
        nav_update(&sim.r, &sim.w, &sim.f);
        atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 0);
    }
}

/* ---------------------------------------------------------------- BENCH_MODE 1: costes sueltos */
#if BENCH_MODE == 1
#define BEG(id) PROF_MARK(id)
#define FIN(id) PROF_MARK((id) + 1)
static void costs(void)
{
    int i, k;
    for (i = 0; i < 24; i++) { BEG(10); controls_step(); flight_step(&sim.f, &sim.w); FIN(10); }
    for (i = 0; i < 4; i++) { BEG(12); nav_update(&sim.r, &sim.w, &sim.f); atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 0); FIN(12); }
#ifdef ENG_EXTERNAL
    for (i = 0; i < 8; i++) { BEG(48); flight_engines(&sim.f); FIN(48); }
#endif
    for (i = 0; i < 4; i++) { BEG(14); nav_update(&sim.r, &sim.w, &sim.f); FIN(14); }
    for (k = 0; k < NAV_PARTS; k++) for (i = 0; i < 4; i++) { BEG(50 + 2 * k); nav_part(&sim.r, &sim.w, &sim.f, k); FIN(50 + 2 * k); }
    for (i = 0; i < 4; i++) { BEG(60); atc_update(&sim.atc, &sim.w, &sim.f, &sim.r, 0); FIN(60); }
    for (i = 0; i < 4; i++) { BEG(16); scan_row(1); FIN(16); }
    for (i = 0; i < 4; i++) { BEG(18); read_keys(0); FIN(18); }
    for (i = 0; i < 2; i++) { BEG(20); read_keys(1); FIN(20); }
    for (i = 0; i < 4; i++) { BEG(40); controls_frame(); FIN(40); }
    for (i = 0; i < 4; i++) { BEG(42); snd_update(&sim.f, &sim.r, 0, 0); FIN(42); }
    for (i = 0; i < 4; i++) { BEG(44); pinfo(); FIN(44); }
    for (i = 0; i < 40; i++) {                    /* panel con el avion maniobrando: 2 pasos de fisica entre actualizaciones (6,25 Hz con la fisica a 12,5) */
        phys_step(); phys_step(); jiggle(); pinfo();
        for (k = 0; k < 6; k++) { BEG(22 + 2 * k); panel_piece(sim.scr, &sim.f, &sim.r, &sim.atc, &bpi, k); FIN(22 + 2 * k); }
    }
    for (i = 0; i < 3; i++) { BEG(36); view_part_a(0); view_part_b(); FIN(36); }
    sim_vjob_wire = 1;
    for (i = 0; i < 3; i++) { BEG(38); while (!sim_vjob_step(0xFFFFFFFFUL)) ; FIN(38); }
    sim_vjob_wire = 0;
    for (i = 0; i < 3; i++) { BEG(46); while (!sim_vjob_step(0xFFFFFFFFUL)) ; FIN(46); }
    rep(1, 0, vstats.objs); rep(1, 1, vstats.faces); rep(1, 2, vstats.lines); rep(1, 3, vstats.tiles);
}
#endif

/* ---------------------------------------------------------------- BENCH_MODE 10: planificador */
#if BENCH_MODE == 10
#define B_STR2(x) #x
#define B_STR(x) B_STR2(x)
#ifndef B_PATX
#define B_PATX FNNNFNNN               /* se pasa sin comillas: -DB_PATX=FKPNFKNN (letras y cifras, empezando por una letra) */
#endif
#define B_PAT B_STR(B_PATX)
#ifndef B_RUN
#define B_RUN 1000
#endif
static const char pat[] = B_PAT;
#define PL (sizeof(pat) - 1)
static long nexec[128], nskip[128], nover[128], maxd[128];
static long idle, vframes, pframes, rowscan;
static int krow;
static const u8 krows[8] = { 1, 0, 1, 2, 1, 3, 1, 4 };      /* la fila 1 (cursores) mas a menudo, y el resto en rotacion */

static void panel_pieces(int a, int b)
{
    int k;
    for (k = a; k <= b; k++) panel_piece(sim.scr, &sim.f, &sim.r, &sim.atc, &bpi, k);
}

#ifndef B_ISR
#define B_ISR 0
#endif
#if B_ISR                              /* la fisica (letra F) corre en la interrupcion de 50 Hz, en el turno 0 de cada 4 */
extern void (*plat_isr_fn)(void);
extern void *plat_isr_sp;
extern volatile long plat_tickcnt;
static long isr_stack[1024];
static long isr_f;
static void isr_fn(void)
{
    if ((plat_tickcnt & 3) == 0) { isr_f++; PROF_MARK(60); phys_step(); PROF_MARK(60); }
}
#endif

static void run_task(int ch, u32 t)
{
    switch (ch) {
    case 'F': if (!B_ISR) phys_step(); break;
    case 'Y': nav_task(); break;
#ifdef ENG_EXTERNAL
    case 'E': flight_engines(&sim.f); break;
#endif
    case 'K': scan_row(krows[krow & 7]); krow++; controls_frame(); rowscan++; break;
    case 'P': jiggle(); pinfo(); panel_pieces(0, 5); pframes++; break;
    case 'A': jiggle(); pinfo(); panel_pieces(0, 2); break;
    case 'B': panel_pieces(3, 5); pframes++; break;
    case '0': case '1': case '2': case '3': case '4': case '5':
        if (ch == '0') { jiggle(); pinfo(); }
        panel_piece(sim.scr, &sim.f, &sim.r, &sim.atc, &bpi, ch - '0');
        if (ch == '5') pframes++;
        break;
    case 'Q': if (panel_job_step(sim.scr, &sim.f, &sim.r, &sim.atc, &bpi, t)) { pframes++; jiggle(); pinfo(); } break;
    case 'V': sim_vjob_wire = 1; if (sim_vjob_step(t)) vframes++; break;
    case 'S': sim_vjob_wire = 0; if (sim_vjob_step(t)) vframes++; break;
    default: break;
    }
}

static void sched(void)
{
    u32 t0 = plat_ticks(), t, last = t0, u, now;
    int ch;
    long d;
#if B_ISR
    plat_isr_sp = (void *)(isr_stack + 1024);
    plat_isr_fn = isr_fn;
#endif
    PROF_MARK(240);
    while ((t = plat_ticks()) - t0 < B_RUN) {
        if (t == last) { idle++; continue; }
        for (u = last + 1; u != t; u++) { ch = pat[u & (PL - 1)]; if (ch != 'N') nskip[ch]++; }   /* turnos que pasaron con la CPU ocupada */
        last = t;
        ch = pat[t & (PL - 1)];
        if (ch == 'N' || (B_ISR && ch == 'F')) continue;
        PROF_MARK(128 + ch);
        run_task(ch, t);
        PROF_MARK(128 + ch);
        nexec[ch]++;
        now = plat_ticks();
        d = (long)(now - t);
        if (d > 0) nover[ch]++;
        if (d > maxd[ch]) maxd[ch] = d;
    }
    PROF_MARK(241);
#if B_ISR
    plat_isr_fn = 0;
    nexec['F'] = isr_f;
#endif
    rep(200, 0, idle); rep(201, 0, vframes); rep(202, 0, pframes); rep(203, 0, (long)(plat_ticks() - t0)); rep(204, 0, rowscan);
    for (ch = 0; ch < 128; ch++) {
        if (B_ISR && ch == 'F') nskip[ch] = 0;      /* la fisica de la interrupcion no depende de los turnos libres de la CPU principal */
        if (nexec[ch] || nskip[ch]) { rep(ch, 0, nexec[ch]); rep(ch, 1, nskip[ch]); rep(ch, 2, maxd[ch]); rep(ch, 3, nover[ch]); }
    }
}
#endif

#ifndef B_RUN
#define B_RUN 1000
#endif
void bench_run(void)
{
    int i, k;
    pinfo();
    PROF_DUMP(99);
#if BENCH_MODE == 1
    costs();
#elif BENCH_MODE == 2
    sim_vjob_wire = 1;
    for (k = 0; k < 4; k++) while (!sim_vjob_step(0xFFFFFFFFUL)) ;
    PROF_DUMP(2);
#elif BENCH_MODE == 3
    sim_vjob_wire = 0;
    for (k = 0; k < 4; k++) while (!sim_vjob_step(0xFFFFFFFFUL)) ;
    PROF_DUMP(3);
#elif BENCH_MODE == 4
    for (i = 0; i < 100; i++) phys_step();
    PROF_DUMP(4);
#elif BENCH_MODE == 5
    for (i = 0; i < 8; i++) { jiggle(); sim.f.z += 256 * 15; sim.f.v += 3000; sim.f.psi += 3 * FX_DEG; pinfo(); for (k = 0; k < 6; k++) panel_piece(sim.scr, &sim.f, &sim.r, &sim.atc, &bpi, k); }
    PROF_DUMP(5);
#elif BENCH_MODE == 6
    sim_vjob_wire = 1;
    for (k = 0; k < 3; k++) while (!sim_vjob_step(0xFFFFFFFFUL)) ;
#elif BENCH_MODE == 7               /* coste de cada unidad de trabajo del panel reanudable, con todo cambiando */
    for (i = 0; i < 12; i++) {
        int done;
        jiggle(); sim.f.z += 256 * 15; sim.f.v += 3000; sim.f.psi += 3 * FX_DEG; pinfo();
        do { PROF_MARK(70); done = panel_job_step(sim.scr, &sim.f, &sim.r, &sim.atc, &bpi, 0xFFFFFFFFUL); PROF_MARK(70); } while (!done);
    }
#elif BENCH_MODE == 8               /* partes del horizonte y de la rosa, cada una por separado */
    {
        extern void ai_begin(const Flight *f); extern void ai_rows(int ya, int yb); extern void ai_plane(void);
        extern void ring(int cx, int cy); extern void hdg_base(int hd); extern void hdg_ticks(int hd);
        extern void hdg_labels(int hd); extern void hdg_num(int hd); extern void hdg_paint(int hd); extern void panel_surface(u8 *scr);
        panel_surface(sim.scr);
        {   /* memoria: filas de 24 bytes a la pantalla (con contencion) o a RAM alta; lineas visibles o no (despues de la fila 256 no hay contencion) */
            u8 *hi = (u8 *)plat_alloc_hi(8192L), *hi2 = (u8 *)plat_alloc_hi(8192L);
            int y;
            for (i = 0; i < 6; i++) {
                PROF_MARK(90); for (y = 0; y < 56; y++) gfx_memcpy(sim.scr + (s32)(130 + y) * SCR_STRIDE + 24, hi + y * 24, 24); PROF_MARK(90);
                PROF_MARK(91); for (y = 0; y < 56; y++) gfx_memcpy(hi2 + y * 24, hi + y * 24, 24); PROF_MARK(91);
                PROF_MARK(92); for (y = 0; y < 56; y++) gfx_memcpy(hi + y * 24, sim.scr + (s32)(130 + y) * SCR_STRIDE + 24, 24); PROF_MARK(92);
                PROF_MARK(93); gfx_text(&sim.view, 10, 10, "123", 3); PROF_MARK(93);
                PROF_MARK(94); { Surface sc; gfx_init_surface(&sc, sim.scr, SCR_H); gfx_text(&sc, 10, 200, "123", 3); } PROF_MARK(94);
            }
        }
        for (i = 0; i < 12; i++) {
            sim.f.phi = (i * 7 - 40) * FX_DEG; sim.f.the = (i - 6) * FX_DEG;
            PROF_MARK(80); ai_begin(&sim.f); PROF_MARK(80);
            PROF_MARK(81); ai_rows(130 - 26, 130 + 26); PROF_MARK(81);
            PROF_MARK(82); ai_plane(); PROF_MARK(82);
            sim.f.phi += 3 * FX_DEG; PROF_MARK(89); ai_begin(&sim.f); ai_rows(130 - 26, 130 + 26); ai_plane(); PROF_MARK(89);
            PROF_MARK(83); ring(144, 130); PROF_MARK(83);
            PROF_MARK(84); hdg_base(i * 31 % 360); PROF_MARK(84);
            PROF_MARK(85); hdg_ticks(i * 31 % 360); PROF_MARK(85);
            PROF_MARK(86); hdg_labels(i * 31 % 360); PROF_MARK(86);
            PROF_MARK(87); hdg_num(i * 31 % 360); PROF_MARK(87);
            { static u16 spr[32]; int g; PROF_MARK(95); for (g = 0; g < 18; g++) gfx_glyph(sim.scr + 130 * 128 + 40 + (g & 3) * 2, spr, 2); PROF_MARK(95);
              PROF_MARK(96); for (g = 0; g < 18; g++) gfx_glyph(sim.scr + 150 * 128 + 40 + (g & 3) * 2, spr, 1); PROF_MARK(96); }
            PROF_MARK(88); hdg_paint(i * 31 % 360 + 1); PROF_MARK(88);
        }
    }
#elif BENCH_MODE == 10
    sched();
#elif BENCH_MODE == 11              /* solo fisica y teclado, a toda marcha (sin esperar a ningun turno): pasos por segundo */
    {
        u32 t0 = plat_ticks();
        long n = 0;
        PROF_MARK(240);
        while (plat_ticks() - t0 < B_RUN) { read_keys(0); controls_frame(); phys_step(); n++; }
        PROF_MARK(241);
        rep(205, 0, n); rep(203, 0, (long)(plat_ticks() - t0));
    }
#endif
    (void)i; (void)k;
    PROF_DUMP(97);
}
#endif
