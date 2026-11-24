/* aircraft.c - lector del fichero de avion (texto CLAVE = valor)
 * Los numeros decimales se convierten a Q16 sin usar coma flotante. */
#include "aircraft.h"
#include "world.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MDL_MAXV 96
#define MDL_MAXF 1024

static const char *skipws(const char *p) { while (*p == ' ' || *p == '\t') p++; return p; }

/* lee un numero decimal "-12.345" -> Q16; avanza *pp */
static s32 parse_q16(const char **pp)
{
    const char *p = skipws(*pp);
    int neg = 0, nd = 0;
    s32 ip = 0, fp = 0, div = 1;
    if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;
    while (*p >= '0' && *p <= '9') { ip = ip * 10 + (*p - '0'); p++; }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            if (nd < 4) { fp = fp * 10 + (*p - '0'); div *= 10; nd++; }
            p++;
        }
    }
    p = skipws(p);
    if (*p == ',') p++;
    *pp = p;
    ip = (ip << 16) + (fp << 16) / div;
    return neg ? -ip : ip;
}

static int parse_int(const char **pp) { return (int)(parse_q16(pp) >> 16); }

static s32 deg2rad(s32 dq16) { return fxmul(dq16, FX_DEG); }

int aircraft_load(AircraftDef *a, const char *fname)
{
    FILE *fp = fopen(fname, "r");
    char line[160], key[24], *e;
    const char *p;
    int i, n;
    static s16 mv[MDL_MAXV * 3];
    static u8 mf[MDL_MAXF];
    int nv = 0, nfb = 0, nf = 0;

    if (!fp) return 0;
    memset(a, 0, sizeof(*a));
    a->n_eng = 1; a->n_flaps = 1; a->vref = FXI(50); a->roll_rate = FXF(3, 2);
    a->pitch_auth = deg2rad(FXI(12)); a->steer = FXF(1, 2); a->gear_h = FXF(3, 2);
    a->spool = FXF(1, 5); a->rpm_static = FXI(2300);
    while (fgets(line, sizeof(line), fp)) {
        e = strchr(line, ';'); if (e) *e = 0;
        p = skipws(line);
        for (i = 0; i < 23 && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_' || (*p >= '0' && *p <= '9')); i++)
            key[i] = *p++;
        key[i] = 0;
        if (!i) continue;
        p = skipws(p);
        if (*p != '=') continue;
        p = skipws(p + 1);
        if (!strcmp(key, "NOMBRE")) {
            for (i = 0; i < 23 && p[i] && p[i] != '\n' && p[i] != '\r'; i++) a->name[i] = p[i];
            a->name[i] = 0;
            while (i > 0 && a->name[i - 1] == ' ') a->name[--i] = 0;
        } else if (!strcmp(key, "MOTOR")) {
            if (!strncmp(p, "PISTON", 6)) a->eng_type = ENG_PISTON;
            else if (!strncmp(p, "REACTOR", 7)) a->eng_type = ENG_JET;
            else a->eng_type = ENG_NONE;
        } else if (!strcmp(key, "HELICE")) a->prop_type = !strncmp(p, "VARIABLE", 8) ? PROP_CS : PROP_FIXED;
        else if (!strcmp(key, "TREN")) a->retract = !strncmp(p, "RETRACTIL", 9);
        else if (!strcmp(key, "MOTORES")) a->n_eng = parse_int(&p);
        else if (!strcmp(key, "MASA")) a->mass = parse_int(&p);
        else if (!strcmp(key, "COMBUSTIBLE")) a->fuel_max = parse_q16(&p);
        else if (!strcmp(key, "SUPERFICIE")) a->S = parse_q16(&p);
        else if (!strcmp(key, "CL0")) a->cl0 = parse_q16(&p);
        else if (!strcmp(key, "CLA")) a->cla = parse_q16(&p);
        else if (!strcmp(key, "ALFA_PERDIDA")) a->alpha_stall = deg2rad(parse_q16(&p));
        else if (!strcmp(key, "CD0")) a->cd0 = parse_q16(&p);
        else if (!strcmp(key, "K")) a->k_ind = parse_q16(&p);
        else if (!strcmp(key, "DCL_FLAP")) a->dcl_flap = parse_q16(&p);
        else if (!strcmp(key, "DCD_FLAP")) a->dcd_flap = parse_q16(&p);
        else if (!strcmp(key, "DALFA_FLAP")) a->dalpha_flap = deg2rad(parse_q16(&p));
        else if (!strcmp(key, "DCD_TREN")) a->dcd_gear = parse_q16(&p);
        else if (!strcmp(key, "FLAPS")) {
            n = 0;
            while (*p && *p != '\n' && n < 6) { a->flap_deg[n++] = parse_int(&p); p = skipws(p); }
            a->n_flaps = n ? n : 1;
        }
        else if (!strcmp(key, "ALABEO")) a->roll_rate = deg2rad(parse_q16(&p));
        else if (!strcmp(key, "PROFUNDIDAD")) a->pitch_auth = deg2rad(parse_q16(&p));
        else if (!strcmp(key, "VREF")) a->vref = parse_q16(&p);
        else if (!strcmp(key, "DIRECCION")) a->steer = parse_q16(&p);
        else if (!strcmp(key, "POTENCIA")) a->power = parse_q16(&p);
        else if (!strcmp(key, "RPM_MAX")) a->rpm_max = parse_q16(&p);
        else if (!strcmp(key, "RPM_RALENTI")) a->rpm_idle = parse_q16(&p);
        else if (!strcmp(key, "RPM_ESTATICA")) a->rpm_static = parse_q16(&p);
        else if (!strcmp(key, "EMPUJE")) a->thrust = parse_q16(&p);
        else if (!strcmp(key, "ACELERACION_N1")) a->spool = parse_q16(&p);
        else if (!strcmp(key, "CONSUMO")) a->ff_max = parse_q16(&p);
        else if (!strcmp(key, "BRAZO_MOTOR")) a->eng_y = parse_q16(&p);
        else if (!strcmp(key, "VS0")) a->vs0 = parse_int(&p);
        else if (!strcmp(key, "VS1")) a->vs1 = parse_int(&p);
        else if (!strcmp(key, "VFE")) a->vfe = parse_int(&p);
        else if (!strcmp(key, "VNO")) a->vno = parse_int(&p);
        else if (!strcmp(key, "VNE")) a->vne = parse_int(&p);
        else if (!strcmp(key, "TECHO")) a->max_alt = parse_int(&p);
        else if (!strcmp(key, "ALTURA_OJO")) a->gear_h = parse_q16(&p);
        else if (!strcmp(key, "ACTITUD_SUELO")) a->pitch_gnd = deg2rad(parse_q16(&p));
        else if (!strcmp(key, "V") && nv < MDL_MAXV) {
            /* vertice del modelo exterior, en metros -> 1/8 m */
            for (i = 0; i < 3; i++) mv[nv * 3 + i] = (s16)(parse_q16(&p) >> 13);
            nv++;
        } else if ((!strcmp(key, "F") || !strcmp(key, "L")) && nfb < MDL_MAXF - 24) {
            /* F = relleno, contorno, i0, i1, ...   (-1 = ninguno)
             * L = color, i0, i1                    (segmento) */
            int fill, lc, idx[20], k = 0, attr;
            if (key[0] == 'F') { fill = parse_int(&p); lc = parse_int(&p); }
            else { fill = -1; lc = parse_int(&p); }
            p = skipws(p);
            while (*p && *p != '\n' && *p != '\r' && k < 20) { idx[k++] = parse_int(&p); p = skipws(p); }
            if (k < 2) continue;
            attr = (fill < 0 ? FA_NOFILL : (fill & 15)) | (lc >= 0 ? (FA_OUTLINE | ((lc & 3) << 4)) : 0);
            if (key[0] == 'L') attr = lc & 3;
            mf[nfb++] = (u8)k; mf[nfb++] = (u8)attr;
            for (i = 0; i < k; i++) mf[nfb++] = (u8)idx[i];
            nf++;
        }
    }
    fclose(fp);
    if (!a->mass) return 0;
    a->model.nv = nv; a->model.nf = nf; a->model.fbytes = nfb;
    a->model.v = (s16 *)malloc((size_t)(nv * 6 + 2));
    a->model.f = (u8 *)malloc((size_t)nfb + 2);
    if (!a->model.v || !a->model.f) return 0;
    memcpy(a->model.v, mv, (size_t)(nv * 6));
    memcpy(a->model.f, mf, (size_t)nfb);
    if (!a->rpm_max) a->rpm_max = FXI(2700);
    if (!a->rpm_idle) a->rpm_idle = FXI(700);
    return 1;
}
