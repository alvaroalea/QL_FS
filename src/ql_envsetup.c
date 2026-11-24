/* ql_envsetup.c -- copia de QL_Mac/src/sdk/qm_envsetup.c: sustituye a _envsetup del runtime de c68 4.24f (se enlaza en el ejecutable).
 * Cinit lo llama antes de main. El original copia los directorios por defecto de Toolkit II (punteros a cadenas QDOS
 * en SV+$AC, $B0 y $B4) a PROG_USE, DATA_USE y SPL_USE sin comprobar nada: sin Toolkit II los punteros son 0 y lee la
 * cadena de la direccion 0 de la ROM, cuya primera palabra hace de longitud. En la ROM JS vale 3 (basura inofensiva);
 * en Minerva vale $4C02: copia 19458 bytes en un bufer de 50 de la pila y el programa se cuelga ANTES de main.
 * (Analisis: ../QL_Elite/experimentos/libc_envsetup/.) Este hace lo mismo que el original (lista de entorno del
 * padre o de la extension *ENV* de SuperBASIC) pero solo acepta un puntero par dentro de la RAM y longitud de 1 a 49. */
#ifdef PLAT_QL
#include <stdlib.h>
#include <string.h>
#include <qdos.h>

static void tk2_default(char *name, long off)
{
    char *q = *(char **)(_sys_var + off);
    char *s, *e;
    int n, i;

    e = getenv(name);
    if (e && *e) return;                                   /* el entorno manda, como en el original */
    if (q <= _sys_var || q >= *(char **)(_sys_var + 0x20) || ((long)q & 1L)) return;   /* fuera de la RAM (SV_RAMT) */
    n = *(short *)q;
    if (n < 1 || n > 49) return;
    s = (char *)malloc(strlen(name) + n + 2);
    if (!s) return;
    strcpy(s, name);
    e = s + strlen(s);
    *e++ = '=';
    for (i = 0; i < n; i++) *e++ = q[2 + i];
    *e = 0;
    putenv(s);
}

void _envsetup(char *env)
{
    char *p = 0, *bv, *nt, *ntb, *nl;

    if (env && !((long)env & 1L) && *(short *)env == 0x4AFC)
        p = *(char **)(env + 2);                           /* entorno pasado por el trabajo padre */
    else {
        _super();                                          /* que SuperBASIC no mueva sus tablas mientras se recorren */
        bv  = *(char **)(_sys_var + 0x10) + 0x68;          /* SV_BASIC: variables de BASIC */
        ntb = bv + *(long *)(bv + 0x18);                   /* BV_NTBAS */
        nt  = bv + *(long *)(bv + 0x1C) - 8;               /* BV_NTP: ultima entrada */
        nl  = bv + *(long *)(bv + 0x20);                   /* BV_NLBAS */
        for (; nt >= ntb; nt -= 8)
            if (nt[0] == 8 && nt[1] == 0 && !strncmp("*ENV*", nl + *(short *)(nt + 2) + 1, 5)) {
                p = *(char **)(*(char **)(nt + 4) + 2);    /* funcion *ENV* de la extension ENV */
                break;
            }
        _user();
    }
    putenv("");
    if (p)
        for (; *p; p += strlen(p) + 1) putenv(p);
    tk2_default("PROG_USE", 0xACL);
    tk2_default("DATA_USE", 0xB0L);
    tk2_default("SPL_USE", 0xB4L);
}
#endif
