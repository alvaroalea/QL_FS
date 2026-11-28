/* flight.c - modelo de vuelo "trayectoria + incidencia" en punto fijo
 *
 * Estados: V (TAS), gamma (trayectoria), alfa (incidencia), theta = gamma+alfa,
 * psi (rumbo), phi (alabeo). El elevador fija la incidencia de equilibrio
 * (estabilidad estatica), el cabeceo tiene amortiguamiento, el alabeo es de
 * primer orden y el giro sale de la componente horizontal de la sustentacion.
 * Asi salen solos el fugoide, la perdida, el efecto de flaps/tren y el
 * empuje asimetrico de los bimotores.
 */
#include "flight.h"
#include <string.h>

#define G_Q16      642908L          /* 9.81 */
#define RHO_HALF   40141L           /* 0.6125 = rho0/2 */

/* factor de potencia segun mezcla relativa r = mezcla/sigma, pasos de 0.25 */
static const s32 mix_tab[13] = {
    0, 0, 26214L, 62259L, 63570L, 60948L, 57672L, 53740L, 48497L, 41943L, 32768L, 19661L, 0
};

static u32 rnd(Flight *f) { f->rnd = f->rnd * 1103515245UL + 12345UL; return f->rnd >> 16; }

void flight_init(Flight *f, const AircraftDef *ac)
{
    int i;
    memset(f, 0, sizeof(*f));
    f->ac = ac;
    f->KL = fxmul(ac->S, RHO_HALF) / ac->mass;
    f->fuel = ac->fuel_max;
    f->auto_coord = 1;
    f->gear_down = 1; f->gear_pos = FX_ONE;
    f->rnd = 12345;
    f->imass = (1L << 24) / ac->mass;
    f->hk_last = -1;
    f->eng_n = 0;
    for (i = 0; i < 2; i++) {
        f->mix[i] = FX_ONE; f->prop[i] = 0;      /* paso fino */
        f->e[i].running = (ac->eng_type != ENG_NONE && i < ac->n_eng);
        f->e[i].rpm = ac->rpm_idle;
        f->e[i].n1 = FXF(22, 100);
    }
}

void flight_place(Flight *f, s32 x_m, s32 y_m, s32 z_m, u16 hdg_bam, s32 v_ms, int on_ground)
{
    f->x = x_m << 8; f->y = y_m << 8; f->z = z_m << 8;
    f->psi = fxdiv((s32)hdg_bam, RAD2BAM);
    f->v = v_ms << 16;
    f->gam = 0; f->q = 0; f->p = 0; f->phi = 0; f->inv = 0;
    f->on_ground = on_ground;
    f->alf = on_ground ? f->ac->pitch_gnd : 3 * FX_DEG;
    f->the = f->alf;
    f->crash = CRASH_NONE;
    f->hit_wait = 0;
    f->eng_n = 0; f->hk_last = -1;       /* recalcula atmosfera y motores en el primer paso */
}

static void engines_step(Flight *f, s32 *t_left, s32 *t_right, s32 dt)
{
    const AircraftDef *a = f->ac;
    int i;
    s32 vint = f->v >> 16;
    *t_left = *t_right = 0;
    for (i = 0; i < a->n_eng && i < 2; i++) {
        Engine *e = &f->e[i];
        s32 thr = f->thr[i], T = 0;
        if (f->fuel <= 0) e->running = 0;
        if (a->eng_type == ENG_PISTON) {
            s32 r, mf, pf, pfs, rpm_t, eta, pw, gov;
            int k;
            r = fxdiv(f->mix[i], f->sigma);              /* mezcla relativa */
            mf = fxlerp_tab(mix_tab, 13, r << 2);
            mf = MIN(fxmul(mf, 67563L), FX_ONE);   /* mf / 63570 en Q16 (65536*65536/63570) */         /* normalizada: con la mezcla justa (maximo de la tabla) es 1 y se llega a plena potencia */
            if (f->mix[i] < FXF(5, 100) || mf < FXF(1, 10)) e->running = 0;
            else if (!e->running && f->starter && f->fuel > 0) e->running = 1;
            e->map = fxmul(fxmul(FXF(2992, 100), f->delta), FXF(3, 10) + fxmul(FXF(7, 10), thr));
            pfs = e->running ? fxmul(fxmul(e->map, 2190L), mf) : 0;   /* map / 29.92 (2190 = 65536/29.92) */
            /* rpm de equilibrio: paso fijo -> a gas maximo y mezcla justa parte de rpm_static y llega a rpm_max (linea roja del
             * cuentarrevoluciones) al coger velocidad (a 40 m/s, unos 78 kt) */
            gov = MIN(pfs + pfs / 20, FX_ONE);           /* carga de la helice: a gas maximo y mezcla justa sale 1.0 aunque la densidad reste algo */
            rpm_t = a->rpm_idle + fxmul(a->rpm_static - a->rpm_idle, gov)
                  + fxmul(a->rpm_max - a->rpm_static, fxmul(MIN(fxmul(f->v, 1638L), FX_ONE), FXF(3, 10) + fxmul(FXF(7, 10), gov)));
            if (!e->running) rpm_t = vint * 25L << 16;
            if (a->prop_type == PROP_CS && e->running) {
                /* palanca P = paso de la pala (0 fino .. 1 grueso). Un paso mas grueso carga mas la helice y frena el motor: tope de
                 * rpm de rpm_max (fino) a 1800 (grueso). Por debajo del tope las rpm siguen a la potencia que dan gas y mezcla
                 * (ralenti con gas 0, rpm_max a gas maximo y mezcla justa) y suben algo con la velocidad, que descarga la helice */
                s32 x = fxdiv(MAX(pfs - FXF(3, 10), 0), FXF(7, 10));         /* 0 en ralenti, 1 a plena potencia */
                x = MIN(x + fxmul(MIN(fxmul(f->v, 1638L), FX_ONE), FXF(1, 2)), FX_ONE);
                rpm_t = a->rpm_idle + fxmul(a->rpm_max - a->rpm_idle, x);
                gov = a->rpm_max - fxmul(a->rpm_max - FXI(1800), f->prop[i]);
                if (rpm_t > gov) rpm_t = gov;
            }
            rpm_t = MIN(rpm_t, a->rpm_max + a->rpm_max / 10);
            e->rpm += fxmul(rpm_t - e->rpm, dt * 2);
            pf = fxmul(pfs, fxdiv(e->rpm, a->rpm_max));
            e->power = fxmul(a->power, pf);
            pw = (e->power >> 16) * 1000L + (((e->power & 0xFFFF) * 1000L) >> 16);
            if (a->prop_type == PROP_CS) {
                /* rendimiento: cada paso rinde al maximo (0.86) a su velocidad (fino 25 m/s, grueso 90 m/s) y cae con el cuadrado
                 * de la diferencia (/8000); la traccion estatica es mayor con paso fino (19 N/kW) que con grueso (11 N/kW) */
                s32 vp = FXI(25) + fxmul(FXI(65), f->prop[i]), dv = (f->v - vp) >> 16;
                eta = MAX(FXF(86, 100) - dv * dv * 8L, FXF(40, 100));         /* dv * dv / 8000 */
                k = 19 - (int)((8L * f->prop[i]) >> 16);
            } else { eta = FXF(80, 100); k = 26; }
            T = fxmul(pw, eta) / MAX(vint, 1);
            T = MIN(T, (pw / 1000L) * k);                                    /* traccion estatica maxima, N por kW */
            if (!e->running) T = -(vint * vint) / 8;  /* helice en molinete: resistencia */
            r = ABS(r - FXF(8, 10));
            e->egt = FXI(250) + fxmul(FXI(550), fxmul(pfs, MAX(FX_ONE - fxmul(r, FXF(6, 5)), FXF(3, 10))));
            e->ff = fxmul(a->ff_max, fxmul(pf, MAX(fxdiv(r + FXF(8, 10), FXF(8, 10)), FXF(1, 2))));
            if (!e->running) e->ff = 0;
        } else if (a->eng_type == ENG_JET) {
            s32 n1t = e->running ? FXF(22, 100) + fxmul(FXF(78, 100), thr) : 0, d, x;
            if (!e->running && f->starter && f->fuel > 0 && f->mix[i] > FXF(5, 100)) e->running = 1;
            if (f->mix[i] < FXF(5, 100)) e->running = 0;   /* palanca de corte */
            d = n1t - e->n1;
            x = fxmul(a->spool, dt);
            e->n1 += CLAMP(d, -x * 2, x);
            x = MAX(e->n1 - FXF(2, 10), 0);
            x = fxdiv(x, FXF(8, 10));
            T = fxmul(fxmul(a->thrust, f->sigma), fxmul(x, x)) * 1000L >> 16;
            T -= vint * 8;                                /* arrastre de admision */
            e->rpm = e->n1 * 100;                         /* % N1 para el panel */
            e->egt = FXI(350) + fxmul(FXI(450), e->n1);
            e->ff = fxmul(a->ff_max, MAX(x, FXF(5, 100)));
            if (!e->running) e->ff = 0;
            e->map = 0;
        }
        e->thrust = T;
        if (i == 0) *t_left = T; else *t_right = T;
    }
    if (a->n_eng == 1) *t_right = 0;
}

#ifndef ENG_N
#define ENG_N 4             /* pasos de fisica por cada calculo de motores (y de la velocidad sobre el suelo) */
#endif
#ifndef HIT_SKIP_MAX
#define HIT_SKIP_MAX 25     /* maximo de pasos de fisica seguidos sin comprobar colision */
#endif

void flight_engines(Flight *f)
{
    const AircraftDef *a = f->ac;
    s32 tl, tr, tt, ff;
    engines_step(f, &tl, &tr, PHYS_DT * ENG_N);
    tt = tl + tr;
    f->aT = fxmul(tt << 4, f->imass) << 4;                  /* tt / mass en Q16 */
    f->asym = a->n_eng == 2 ? fxmul(fxmul((tl - tr) << 4, f->imass) << 4, a->eng_y / 3) : 0;   /* (T1-T2)/m * brazo/3 */
    ff = f->e[0].ff + (a->n_eng > 1 ? f->e[1].ff : 0);      /* combustible */
    f->fuel -= fxmul(ff, PHYS_DT * ENG_N) / 3600;
    if (f->fuel < 0) f->fuel = 0;
}

void flight_step(Flight *f, World *w)
{
    const AircraftDef *a = f->ac;
    s32 hk, gt, cl, cd, clmax, as, vi, v2, qa;
    s32 sg, cg, sp, cp, sa, ca, vc, dv, dg, dpsi, acmd, qn, pc, gz, vh, dx, dy, vr;
    s32 asym, fp;
    int was_ground = f->on_ground, hit, tick = (f->eng_n == 0);

    if (f->crash) return;
    /* atmosfera: solo se rehace al cambiar la altitud (en metros enteros); sigma y delta, y lo que sale de ellas, se guardan */
    hk = MAX(f->z >> 8, 0);
    if (hk != f->hk_last) {
        s32 hx = hk * 65536L / 1000;            /* (division poco frecuente: una vez por metro de altitud) */
        f->hk_last = hk;
        f->sigma = fxlerp_tab(atm_sigma, 17, hx);
        f->delta = fxlerp_tab(atm_delta, 17, hx);
        f->kls = fxmul(f->KL, f->sigma);
        f->sqs = isqrt32((u32)f->sigma) << 8;
    }

    /* flaps (0.2/s) y tren (0.25/s) */
    if (f->flaps != f->flaps_last) {            /* objetivo de los flaps: solo cuando cambia la palanca */
        f->flaps_last = f->flaps;
        f->flap_tgt = a->n_flaps > 1 ? (s32)f->flaps * FX_ONE / (a->n_flaps - 1) : 0;
    }
    f->flap_pos += CLAMP(f->flap_tgt - f->flap_pos, -PHYS_DT / 5, PHYS_DT / 5);
    gt = (f->gear_down || !a->retract) ? FX_ONE : 0;
    if (f->on_ground) gt = FX_ONE;
    f->gear_pos += CLAMP(gt - f->gear_pos, -PHYS_DT / 4, PHYS_DT / 4);
    fp = f->flap_pos;

    /* motores: cambian despacio (constantes de tiempo de medio segundo o mas), asi que se calculan cada ENG_N pasos con el
     * paso de tiempo multiplicado; en los demas se reutiliza el empuje (aT y asimetria, en Q16) del ultimo calculo.
     * Con -DENG_EXTERNAL los motores no se calculan aqui: los llama quien reparte las tareas (flight_engines, doc/optimizacion.md, «Planificador por turnos») */
#ifdef ENG_EXTERNAL
    tick = 1;
#else
    if (tick) flight_engines(f);
    if (++f->eng_n >= ENG_N) f->eng_n = 0;
#endif

    /* aerodinamica */
    as = a->alpha_stall - fxmul(a->dalpha_flap, fp);
    clmax = a->cl0 + fxmul(a->cla, as) + fxmul(a->dcl_flap, fp);
    cl = a->cl0 + fxmul(a->cla, f->alf) + fxmul(a->dcl_flap, fp);
    f->stalled = 0;
    if (f->alf > as) {
        f->stalled = 1;
        cl = MAX(clmax - fxmul(fxmul(a->cla, FXF(6, 10)), f->alf - as), fxmul(clmax, FXF(4, 10)));
    } else if (f->alf < -fxmul(as, FXF(8, 10))) {
        f->stalled = 1;
        cl = MIN(cl, -fxmul(clmax, FXF(6, 10)));
    }
    f->stall_warn = !f->on_ground && f->alf > as - 2 * FX_DEG;
    cd = a->cd0 + fxmul(a->k_ind, fxmul(cl, cl)) + fxmul(a->dcd_flap, fp);
    if (a->retract) cd += fxmul(a->dcd_gear, f->gear_pos);
    if (f->stalled) cd += fxmul(FXF(3, 10), ABS(f->alf - as));

    vi = f->v >> 12;                      /* Q4 */
    v2 = MULS16(vi, vi);                  /* V^2 Q8 (vi cabe en 16 bits: V < 2000 m/s) */
    qa = fxmul(f->kls, v2);               /* Q8 */
    f->aL = fxmul(qa, cl) << 8;
    f->aD = fxmul(qa, cd) << 8;

    sg = fxsin(f->gam); cg = fxcos(f->gam);
    sp = fxsin(f->phi); cp = fxcos(f->phi);
    sa = fxsin(f->alf); ca = fxcos(f->alf);
    vc = MAX(f->v, FXI(5));

    /* ecuaciones */
    dv = fxmul(f->aT, ca) - f->aD - fxmul(G_Q16, sg);
    dg = fxdiv(fxmul(f->aL, cp) + fxmul(f->aT, sa) - fxmul(G_Q16, cg), vc);
    dpsi = fxdiv(fxmul(f->aL, sp), fxmul(vc, MAX(cg, FXF(1, 10))));

    /* cabeceo: el elevador fija la incidencia de equilibrio */
    vr = fxdiv(f->v, a->vref);
    qn = MIN(fxmul(vr, vr), FXI(3));
    acmd = 2 * FX_DEG + fxmul(CLAMP(f->elev + f->trim, -FX_ONE, FX_ONE), a->pitch_auth);
    f->q += fxmul(PHYS_DT, fxmul(fxmul(FXI(18), acmd - f->alf), qn) - fxmul(FXI(5), f->q));

    /* alabeo */
    pc = fxmul(fxmul(f->ail, a->roll_rate), MIN(vr, FXF(3, 2)));
    if (f->stalled && !f->on_ground && f->alf > as + FX_DEG && (rnd(f) & 15) == 0)
        f->p += (rnd(f) & 1) ? FXF(3, 10) : -FXF(3, 10);      /* caida de ala */
    f->p += fxmul(PHYS_DT * 4, pc - f->p);

    /* guinada: timon (si no hay coordinacion automatica) y empuje asimetrico */
    asym = f->asym;
    if (!f->auto_coord) dpsi += fxmul(fxmul(f->rud, FXF(15, 100)), MIN(vr, FX_ONE));
    dpsi += fxdiv(asym, vc);
    f->slip = CLAMP((f->auto_coord ? 0 : f->rud) + fxmul(asym, FXF(1, 2)), -FX_ONE, FX_ONE);   /* guinada hacia la derecha > 0 (timon y empuje asimetrico suman) */

    if (f->on_ground) {
        s32 mu = f->brakes ? FXF(40, 100) : FXF(2, 100);
        s32 load = FX_ONE - fxdiv(f->aL, G_Q16);
        if (load < 0) load = 0;
        dv -= fxmul(fxmul(mu, G_Q16), load);
        dpsi = fxmul(fxmul(f->auto_coord ? f->ail : f->rud, a->steer), MIN(f->v >> 3, FXI(3)));
        f->phi = 0; f->p = 0;
        if (dg <= 0 && f->v < FXI(5)) dg = 0;
        if (dg <= 0) { dg = 0; f->gam = 0; }
        else f->on_ground = 0;                         /* despegue */
        if (f->v + fxmul(dv, PHYS_DT) < 0) { dv = 0; f->v = 0; }
    }

    f->v += fxmul(dv, PHYS_DT);
    if (f->v < 0) f->v = 0;
    f->gam += fxmul(dg, PHYS_DT);
    /* al pasar por la vertical (looping) se refleja la trayectoria: gam -> pi - gam, con rumbo y alabeo girados 180 grados (vuelo
     * invertido, inv alterna); asi gam se queda en +-90 y nunca se atasca. alf (incidencia respecto al avion) no cambia. */
    if (f->gam > FX_HALFPI || f->gam < -FX_HALFPI) {
        s32 alf0 = f->alf;
        f->gam = (f->gam > 0 ? FX_PI : -FX_PI) - f->gam;
        f->inv ^= 1;
        f->psi += FX_PI;
        f->phi += FX_PI;
        f->the = f->inv ? f->gam - alf0 : f->gam + alf0;
    }
    f->psi += fxmul(dpsi, PHYS_DT);
    if (f->psi < 0) f->psi += FX_2PI;
    if (f->psi >= FX_2PI) f->psi -= FX_2PI;
    f->phi += fxmul(f->p, PHYS_DT);
    if (f->phi > FX_PI) f->phi -= FX_2PI;
    if (f->phi < -FX_PI) f->phi += FX_2PI;
    f->the += f->inv ? -fxmul(f->q, PHYS_DT) : fxmul(f->q, PHYS_DT);
    if (f->on_ground) {
        if (f->the < a->pitch_gnd) { f->the = a->pitch_gnd; if (f->q < 0) f->q = 0; }
        if (f->the > a->pitch_gnd + 14 * FX_DEG) { f->the = a->pitch_gnd + 14 * FX_DEG; f->q = 0; }
        f->gam = 0;
    }
    f->alf = f->inv ? f->gam - f->the : f->the - f->gam;
    f->turn = dpsi;

    /* posicion */
    vh = fxmul(f->v, cg);
    dx = fxmul(vh, fxsin(f->psi)) + (f->on_ground ? 0 : f->wind_e);
    dy = fxmul(vh, fxcos(f->psi)) + (f->on_ground ? 0 : f->wind_n);
    f->vs = fxmul(f->v, sg);
    if (tick) f->gs = isqrt32((u32)(MULS16(dx >> 12, dx >> 12) + MULS16(dy >> 12, dy >> 12))) << 12;   /* solo se muestra: cada ENG_N pasos */
    /* integracion con resto: el paso puede ser menor que 1/256 m */
    f->accx += fxmul(dx, PHYS_DT); f->x += f->accx >> 8; f->accx &= 255;
    f->accy += fxmul(dy, PHYS_DT); f->y += f->accy >> 8; f->accy &= 255;
    f->accz += fxmul(f->vs, PHYS_DT); f->z += f->accz >> 8; f->accz &= 255;
    f->ias = fxmul(f->v, f->sqs);

    /* contacto con el suelo */
    gz = (s32)world_ground(w, f->x >> 8, f->y >> 8) << 16;
    f->agl = (f->z << 8) - gz - a->gear_h;
    if (f->agl < 0 || (f->on_ground && f->agl == 0)) {
        f->z = (gz + a->gear_h + 255) >> 8;
        f->accz = 0;
        f->agl = 0;
        if (!was_ground) {
            if (a->retract && f->gear_pos < FXF(9, 10)) f->crash = CRASH_GEAR;
            else if (f->vs < -FXF(35, 10) || ABS(f->phi) > 12 * FX_DEG || f->the < -4 * FX_DEG)
                f->crash = CRASH_GROUND;
            f->on_ground = 1;
            f->vs = 0; f->gam = 0; f->phi = 0; f->p = 0; f->inv = 0;
        }
    }
    /* colision con objetos: solo cuando el avion puede haber llegado a alguno. world_hit devuelve en hit_near la distancia
     * horizontal al objeto mas cercano; hasta recorrerla (a la velocidad del suelo, con margen) no hay nada que comprobar */
    if (f->hit_wait > 0 && f->hit_gen == w->gen) f->hit_wait--;
    else {
        s32 step, n;
        hit = world_hit(w, f->x >> 8, f->y >> 8, f->z >> 8);
        if (hit == 1) f->crash = CRASH_BUILDING;
        else if (hit == 2) f->crash = CRASH_MOUNTAIN;
        else if (hit == 3 && f->agl <= FXI(1)) f->crash = CRASH_WATER;
        step = ((f->gs >> 16) + (f->v >> 16) + 2) / PHYS_HZ + 1;    /* m por paso, por exceso */
        n = w->hit_near / step - 1;
        f->hit_wait = (int)CLAMP(n, 0L, (s32)HIT_SKIP_MAX);
        f->hit_gen = w->gen;
    }
    if (a->vne && (MS2KT(f->ias) >> 16) > a->vne + a->vne / 8) f->crash = CRASH_OVERSTRESS;
}
