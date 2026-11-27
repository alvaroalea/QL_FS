/* nav.c - calculo de las radioayudas (enteros y tablas, sin flotantes) */
#include "nav.h"

void nav_init(Radios *r)
{
    r->com = 11830; r->nav1 = 11030; r->nav2 = 11320; r->adf = 380;
    r->obs1 = 0; r->obs2 = 0; r->squawk = 1200;
    r->n1 = r->n2 = NULL;
    r->dme10 = -1;
}

void freq_str(u16 f, char *b)
{
    int mhz = f / 100, k = f % 100;
    b[0] = (char)('0' + mhz / 100); b[1] = (char)('0' + mhz / 10 % 10); b[2] = (char)('0' + mhz % 10);
    b[3] = '.'; b[4] = (char)('0' + k / 10); b[5] = (char)('0' + k % 10); b[6] = 0;
}

static s32 dist_m(s32 dx, s32 dy)
{
    s32 s = 0;
    while (ABS(dx) > 30000L || ABS(dy) > 30000L) { dx >>= 1; dy >>= 1; s++; }
    return isqrt32((u32)(dx * dx + dy * dy)) << s;
}

const Nav *nav_find(World *w, u16 freq, int ndb, s32 x, s32 y)
{
    int i;
    const Nav *best = NULL;
    s32 bd = 0x7FFFFFFFL, d;
    const Nav *n = w->nav;                       /* (puntero que avanza: n[i] con struct grande es un producto de 32 bits en el 68000) */
    for (i = w->nnav; i > 0; i--, n++) {
        if (n->freq != freq) continue;
        if (ndb != (n->type == NAV_NDB)) continue;
        if (n->type >= NAV_OM) continue;
        d = dist_m(n->x - x, n->y - y);
        if (d > (s32)n->range * 1000L) continue;
        if (d < bd) { bd = d; best = n; }
    }
    return best;
}

/* VOR: radial, indicacion TO/FROM y desviacion del CDI (10 grados a fondo) */
static void vor_calc(const Nav *n, s32 x, s32 y, u16 obs, int *flag, s16 *cdi)
{
    u16 radial = iatan2(y - n->y, x - n->x);
    s16 d = (s16)(radial - obs);
    s32 c;
    if (d > -16384 && d < 16384) { *flag = -1; c = -d; }          /* FROM */
    else { *flag = 1; c = (s16)(radial - (u16)(obs + 32768U)); }  /* TO */
    c = c * 127 / 1820;
    *cdi = (s16)CLAMP(c, -127, 127);
}

/* nav_update en NAV_PARTS partes independientes (0 NAV1, 1 NAV2, 2 ADF, 3 y 4 balizas, cada mitad de la lista): cada una recorre la lista de radioayudas (3-5 ms en el QL)
 * y deja sus campos de Radios completos, de modo que el planificador puede repartirlas entre varios turnos (sim.c, letra Y). */
void nav_part(Radios *r, World *w, const Flight *f, int k)
{
    s32 x = f->x >> 8, y = f->y >> 8, z = f->z >> 8, d, c;
    int i;
    u16 b;

    switch (k) {
    case 0:
    /* NAV1: VOR o ILS */
    r->n1 = nav_find(w, r->nav1, 0, x, y);
    r->n1_flag = 0; r->cdi1 = 0; r->gs_valid = 0; r->gs = 0; r->dme10 = -1; r->n1_loc = 0;
    if (r->n1) {
        const Nav *n = r->n1;
        d = dist_m(n->x - x, n->y - y);
        if (n->type == NAV_ILS) {
            s16 dev;
            r->n1_loc = 1;
            b = iatan2(n->y - y, n->x - x);                 /* rumbo hacia la antena */
            dev = (s16)(b - n->course);
            if (dev > -16384 && dev < 16384) {
                r->n1_flag = 1;
                c = (s32)dev * 127 / 455;                   /* 2.5 grados a fondo */
                r->cdi1 = (s16)CLAMP(-c, -127, 127);
                for (i = 0; i < w->nnav; i++)
                    if (w->nav[i].type == NAV_GS && w->nav[i].freq == n->freq) break;
                if (i < w->nnav && ABS(dev) < 1820 && d < 20000L) {   /* senda: 0.7 grados a fondo */
                    const Nav *g = &w->nav[i];
                    s16 el = (s16)iatan2(dist_m(g->x - x, g->y - y), z - g->z);
                    r->gs_valid = 1;
                    c = (s32)((s16)(g->gs - el));
                    r->gs = (s16)CLAMP(c, -127, 127);
                }
            }
        } else vor_calc(n, x, y, r->obs1, &r->n1_flag, &r->cdi1);
        if (n->type == NAV_VORDME || n->type == NAV_ILS) {
            s32 dz = z - n->z;
            d = dist_m(d, dz);
            r->dme10 = d * 10 / 1852;
        }
    }
    break;
    case 1:
    /* NAV2: solo VOR */
    r->n2 = nav_find(w, r->nav2, 0, x, y);
    r->n2_flag = 0; r->cdi2 = 0;
    if (r->n2 && r->n2->type != NAV_ILS) vor_calc(r->n2, x, y, r->obs2, &r->n2_flag, &r->cdi2);

    break;
    case 2:
    /* ADF */
    {
        u16 hdg = (u16)fxmul(f->psi, RAD2BAM);
        const Nav *n = nav_find(w, r->adf, 1, x, y);
        r->adf_valid = n != NULL;
        if (n) r->adf_rel = (u16)(iatan2(n->y - y, n->x - x) - hdg);
    }
    break;
    case 3:
    case 4:
    /* balizas: cono vertical de unos 300 m + 1/4 de la altura (parte 3: primera mitad de la lista, parte 4: la segunda si no hubo baliza) */
    if (k == 3) r->marker = 0;
    else if (r->marker) break;
    {
        int half = w->nnav / 2;
        const Nav *n = w->nav + (k == 3 ? 0 : half);
        s32 lim;
        for (i = k == 3 ? half : w->nnav - half; i > 0; i--, n++) {
            if (n->type < NAV_OM || n->type > NAV_IM) continue;
            lim = 300 + (z - n->z) / 4;          /* rechazo rapido sin raiz: la distancia no baja de max(|dx|, |dy|) */
            if (ABS(n->x - x) >= lim || ABS(n->y - y) >= lim) continue;
            d = dist_m(n->x - x, n->y - y);
            if (d < lim) { r->marker = n->type; break; }
        }
    }
    break;
    }
}

void nav_update(Radios *r, World *w, const Flight *f)
{
    int k;
    for (k = 0; k < NAV_PARTS; k++) nav_part(r, w, f, k);
}
