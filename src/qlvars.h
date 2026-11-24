/* qlvars.h -- parametros del programa por VARIABLES de SuperBASIC (D$ = unidad/prefijo de datos, M$ = raton), no por argv.
 * Sin Toolkit II el texto que sigue a EXEC_W (EXEC_W win1_v2;"arg") cuelga el sistema: el programa no lee argc/argv.
 * Lectura de las variables de cadena de BASIC como en QL_XCom (xc_path.h) y QL_Mac (plat_ql.c). Se incluye en game.c y game3.c. */
#ifdef PLAT_QL
/* Variable de cadena de SuperBASIC (trabajo 0) sin Toolkit II, como en QL_Mac (plat_ql.c): SV_BASIC ($10 en las variables del
   sistema) apunta al bloque del trabajo de BASIC y su area de datos (A6) empieza $68 despues; tabla de nombres (entradas de
   8 bytes: tipo de nombre, tipo de valor, desplazamiento en la lista de nombres, desplazamiento en los valores), punteros
   relativos a A6. El nombre incluye el sufijo "$"; el valor es palabra de longitud + texto. Todo se lee byte a byte. */
extern long ql_sysvars(void);

static long ql_rd32(const char *p)
{
    const unsigned char *q = (const unsigned char *)p;

    return (long)(((unsigned long)q[0] << 24) | ((unsigned long)q[1] << 16) | ((unsigned long)q[2] << 8) | q[3]);
}

static int ql_rd16(const char *p) { return ((unsigned char)p[0] << 8) | (unsigned char)p[1]; }

static long ql_ram_hi = 0x100000L;                  /* SV_RAMT (variables del sistema + $20): fin de la RAM; con mas de 1 MB BASIC puede estar por encima */
#define QV_IN_RAM(a) ((long)(a) >= 0x20000L && (long)(a) < ql_ram_hi)

/* Copia en out el valor de la variable de cadena `name` (sin el $); 1 si existe */
static int ql_basic_str(const char *name, char *out, int max)
{
    char *a6, *nt, *ntp, *nlb, *vvb;
    long sv = ql_sysvars(), base;
    int n = (int)strlen(name), i, pass;

    out[0] = 0;
    if (sv == 0L) return 0;
    base = ql_rd32((const char *)(sv + 0x20));
    if (base > 0x20000L && base <= 0x1000000L) ql_ram_hi = base;
    base = ql_rd32((const char *)(sv + 0x10));
    if (!QV_IN_RAM(base)) return 0;
    a6 = (char *)base + 0x68;
    nt = a6 + ql_rd32(a6 + 0x18);
    ntp = a6 + ql_rd32(a6 + 0x1C);
    nlb = a6 + ql_rd32(a6 + 0x20);
    vvb = a6 + ql_rd32(a6 + 0x28);
    if (!QV_IN_RAM(nt) || !QV_IN_RAM(ntp) || ntp < nt || ntp - nt > 0x8000L || !QV_IN_RAM(nlb) || !QV_IN_RAM(vvb)) return 0;
    for (pass = 0; pass < 2; pass++)       /* 1a pasada: tipo de nombre 2 (variable); 2a: cualquier nombre con valor de cadena */
    for (nt = a6 + ql_rd32(a6 + 0x18); nt + 8 <= ntp; nt += 8) {
        const char *nl;
        int len;

        if ((pass == 0 && nt[0] != 2) || nt[1] != 1 || ql_rd32(nt + 4) < 0L) continue;       /* variable de cadena */
        nl = nlb + ql_rd16(nt + 2);
        len = (unsigned char)nl[0];
        if (len == n + 1 && nl[len] == '$') len = n;
        if (len != n) continue;
        for (i = 0; i < n; i++) {
            char a = nl[1 + i], b = name[i];

            if (a >= 'a' && a <= 'z') a = (char)(a - 32);
            if (b >= 'a' && b <= 'z') b = (char)(b - 32);
            if (a != b) break;
        }
        if (i == n) {
            const char *v = vvb + ql_rd32(nt + 4);
            int l;

            if (!QV_IN_RAM(v)) return 0;
            l = ql_rd16(v);
            if (l >= max) l = max - 1;
            memcpy(out, v + 2, (size_t)l);
            out[l] = 0;
            return 1;
        }
    }
    return 0;
}
#endif
