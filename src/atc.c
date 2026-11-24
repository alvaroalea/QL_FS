/* atc.c - ATC basico. La fraseologia va en ingles (como en la aviacion real
 * y en el simulador original); la interfaz del programa esta en espanol. */
#include "atc.h"
#include "util.h"

void atc_init(Atc *a)
{
    a->apt = NULL; a->mode = 0; a->phase = PH_PARKED; a->rwy = 0;
    a->text[0] = 0; a->pos = a->len = a->hold = 0; a->letter = 'A'; a->new_msg = 0;
}

void atc_say(Atc *a, const char *s)
{
    u_cpy(a->text, s, ATC_TEXT);
    a->len = u_len(a->text);
    a->pos = 0; a->hold = 0; a->new_msg = 1;
}

static s32 dist(s32 dx, s32 dy)
{
    int s = 0;
    while (ABS(dx) > 30000L || ABS(dy) > 30000L) { dx >>= 1; dy >>= 1; s++; }
    return isqrt32((u32)(dx * dx + dy * dy)) << s;
}

static int rwy_num(u16 hdg)
{
    int n = (int)(((s32)hdg * 36 + 32768L) >> 16);
    return n == 0 ? 36 : n;
}

/* viento: de donde sopla (grados) y nudos */
static void wind_from(const Flight *f, int *deg, int *kt)
{
    s32 e = f->wind_e, n = f->wind_n;
    u16 to = iatan2(n, e);
    *deg = (int)(((s32)(u16)(to + 32768U) * 360L) >> 16);
    if (*deg == 0) *deg = 360;
    *kt = (int)(MS2KT(isqrt32((u32)(MULS16(e >> 8, e >> 8) + MULS16(n >> 8, n >> 8))) << 8) >> 16);
}

static int best_runway(const Airport *ap, const Flight *f)
{
    int i, best = 0;
    s16 bd = 32767, d;
    u16 from = (u16)(iatan2(f->wind_n, f->wind_e) + 32768U);
    for (i = 0; i < ap->nrwy; i++) {
        d = (s16)(ap->rwy[i].hdg - from);
        if (d < 0) d = (s16)-d;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void add_wind_rwy(char *m, const Airport *ap, int r, const Flight *f)
{
    int wd, wk;
    wind_from(f, &wd, &wk);
    u_cat(m, "RUNWAY ", ATC_TEXT);
    u_catn(m, rwy_num(ap->rwy[r].hdg), 2, '0', ATC_TEXT);
    u_cat(m, ", WIND ", ATC_TEXT);
    if (wk < 2) u_cat(m, "CALM", ATC_TEXT);
    else { u_catn(m, wd, 3, '0', ATC_TEXT); u_cat(m, " AT ", ATC_TEXT); u_catn(m, wk, 1, ' ', ATC_TEXT); }
}

static void build_atis(Atc *a, const Flight *f)
{
    char m[ATC_TEXT];
    const Airport *ap = a->apt;
    char l[2];
    int r = best_runway(ap, f);
    l[0] = a->letter; l[1] = 0;
    m[0] = 0;
    u_cat(m, ap->name, ATC_TEXT); u_cat(m, " INFORMATION ", ATC_TEXT); u_cat(m, l, ATC_TEXT);
    u_cat(m, ". ", ATC_TEXT);
    add_wind_rwy(m, ap, r, f);
    u_cat(m, ". ALTIMETER 2992. ", ATC_TEXT);
    u_cat(m, ap->atis_text ? ap->atis_text : "", ATC_TEXT);
    u_cat(m, " ADVISE YOU HAVE ", ATC_TEXT); u_cat(m, l, ATC_TEXT); u_cat(m, ".   ", ATC_TEXT);
    atc_say(a, m);
    a->rwy = r;
}

void atc_update(Atc *a, World *w, const Flight *f, const Radios *rd, int contact)
{
    int i, mode = 0;
    const Airport *ap = NULL;
    s32 x = f->x >> 8, y = f->y >> 8, d = 0, kt;
    char m[ATC_TEXT];

    for (i = 0; i < w->napt; i++) {
        const Airport *p = &w->apt[i];
        s32 dd = dist(p->x - x, p->y - y);
        if (dd > 60000L) continue;
        if (p->atis == rd->com) { ap = p; mode = 1; d = dd; break; }
        if (p->twr == rd->com) { ap = p; mode = 2; d = dd; break; }
    }
    if (ap != a->apt || mode != a->mode) {
        a->apt = ap; a->mode = mode;
        a->text[0] = 0; a->len = a->pos = 0;
        if (mode == 1) build_atis(a, f);
    }
    if (!ap) return;
    kt = MS2KT(f->gs) >> 16;

    if (mode == 2) {
        /* eventos automaticos de la torre */
        if (a->phase == PH_TAKEOFF && !f->on_ground && d > 3000) {
            atc_say(a, "CONTACT DEPARTURE. GOOD DAY.");
            a->phase = PH_DEPART;
        } else if (a->phase == PH_LAND && f->on_ground && kt < 25) {
            m[0] = 0; u_cat(m, "WELCOME TO ", ATC_TEXT); u_cat(m, ap->name, ATC_TEXT);
            u_cat(m, ". TAXI TO PARKING.", ATC_TEXT);
            atc_say(a, m);
            a->phase = PH_LANDED;
        } else if (a->phase != PH_LAND && a->phase != PH_LANDED && f->on_ground && kt > 30 &&
                   a->phase != PH_TAKEOFF && d < 4000) {
            atc_say(a, "YOU WERE NOT CLEARED. CONTACT THE TOWER.");
            a->phase = PH_LANDED;
        }
    }
    if (!contact) return;
    if (mode != 2) { atc_say(a, "...NO REPLY ON THIS FREQUENCY."); return; }

    m[0] = 0;
    u_cat(m, ap->name, ATC_TEXT); u_cat(m, " TOWER, ", ATC_TEXT);
    a->rwy = best_runway(ap, f);
    if (f->on_ground && kt < 20) {
        add_wind_rwy(m, ap, a->rwy, f);
        u_cat(m, ", CLEARED FOR TAKEOFF.", ATC_TEXT);
        a->phase = PH_TAKEOFF;
    } else if (!f->on_ground && d < 9000) {
        s16 dh = (s16)((u16)fxmul(f->psi, RAD2BAM) - ap->rwy[a->rwy].hdg);
        add_wind_rwy(m, ap, a->rwy, f);
        if (ABS(dh) < 5461) { u_cat(m, ", CLEARED TO LAND.", ATC_TEXT); a->phase = PH_LAND; }
        else { u_cat(m, ", ENTER DOWNWIND, REPORT FINAL.", ATC_TEXT); a->phase = PH_APPROACH; }
    } else if (!f->on_ground && d < 25000L) {
        add_wind_rwy(m, ap, a->rwy, f);
        u_cat(m, ", ALTIMETER 2992, REPORT 5 MILES FINAL.", ATC_TEXT);
        a->phase = PH_APPROACH;
    } else if (!f->on_ground) {
        int hdg = (int)(((s32)iatan2(ap->y - y, ap->x - x) * 360L + 32768L) >> 16);
        if (hdg == 0) hdg = 360;
        u_cat(m, "RADAR CONTACT, SQUAWK ", ATC_TEXT); u_catn(m, rd->squawk, 4, '0', ATC_TEXT);
        u_cat(m, ". FLY HEADING ", ATC_TEXT); u_catn(m, hdg, 3, '0', ATC_TEXT);
        u_cat(m, ", VECTORS TO ", ATC_TEXT); u_cat(m, ap->ident, ATC_TEXT); u_cat(m, ".", ATC_TEXT);
        a->phase = PH_ENROUTE;
    } else {
        u_cat(m, "HOLD SHORT, STOP BEFORE CALLING.", ATC_TEXT);
    }
    atc_say(a, m);
}

void atc_scroll(Atc *a)
{
    if (!a->len) return;
    if (a->pos < a->len) { a->pos++; return; }
    if (++a->hold > 120) {
        if (a->mode == 1) { a->pos = 0; a->hold = 0; }          /* el ATIS se repite */
        else { a->len = a->pos = 0; a->text[0] = 0; }
    }
}

void atc_window(const Atc *a, char *out, int n)
{
    int s = a->pos - n, i;
    if (s < 0) s = 0;
    for (i = 0; i < n; i++) out[i] = (s + i < a->pos) ? a->text[s + i] : ' ';
    out[n] = 0;
}
