/* fixmath.c - aritmetica entera y punto fijo (sin coma flotante) */
#include "qfs.h"

#ifndef USE_ASM_FXMUL
/* Version C portable de (a*b)>>16. En el QL se sustituye por la de fx68k.s
 * (cuatro MULU de 16x16). Aqui se hace igual, por partes de 16 bits, para que
 * el resultado sea identico en el PC y en el QL. */
s32 fxmul(s32 a, s32 b)
{
    int neg = 0;
    u32 ua, ub, ah, al, bh, bl, r;
    if (a < 0) { a = -a; neg = 1; }
    if (b < 0) { b = -b; neg ^= 1; }
    ua = (u32)a; ub = (u32)b;
    ah = ua >> 16; al = ua & 0xFFFFUL;
    bh = ub >> 16; bl = ub & 0xFFFFUL;
    r = ((ah * bh) << 16) + ah * bl + al * bh + ((al * bl) >> 16);
    return neg ? -(s32)r : (s32)r;
}
#endif

#ifndef USE_ASM_MDQ
/* a*b/d por la via rapida del 68000: |a|,|b| <= 32767, divisor de 16 bits (si no, se desplaza d y a a la vez) y cociente de
 * 16 bits con signo; si algo no cabe devuelve MDQ_BIG y el que llama usa la version lenta exacta. */
s32 mdq(s32 a, s32 b, s32 d)
{
    s32 q;
    while (d > 32767L || d < -32768L) { d >>= 1; a >>= 1; }
    if (d == 0 || a > 32767L || a < -32768L || b > 32767L || b < -32768L) return MDQ_BIG;
    q = MULS16(a, b) / d;
    if (q > 32767L || q < -32768L) return MDQ_BIG;
    return q;
}
#endif

/* (a<<16)/b sin 64 bits: se normaliza a hacia arriba y b hacia abajo */
s32 fxdiv(s32 a, s32 b)
{
    int s = 16, neg = 0;
    if (a < 0) { a = -a; neg = 1; }
    if (b < 0) { b = -b; neg ^= 1; }
    while (s > 0 && a < 0x20000000L) { a <<= 1; s--; }
    b >>= s;
    if (b == 0) return neg ? -0x7FFFFFFFL : 0x7FFFFFFFL;
    a = a / b;
    return neg ? -a : a;
}

s32 isqrt32(u32 v)
{
    u32 r = 0, bit = 1UL << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (s32)r;
}

s16 isin(u16 bam) { return sin_tab[(bam >> 6) & 1023]; }
s16 icos(u16 bam) { return sin_tab[((u16)(bam + 16384) >> 6) & 1023]; }

s32 fxsin(s32 rad) { return (s32)isin((u16)fxmul(rad, RAD2BAM)) << 2; }
s32 fxcos(s32 rad) { return (s32)icos((u16)fxmul(rad, RAD2BAM)) << 2; }

/* rumbo BAM16 del vector (este, norte): 0 = norte, 16384 = este */
u16 iatan2(s32 north, s32 east)
{
    s32 ax = ABS(east), ay = ABS(north);
    u16 a;
    if (ax == 0 && ay == 0) return 0;
    while (ax > 0x7FFFFL || ay > 0x7FFFFL) { ax >>= 1; ay >>= 1; }
    if (ax <= ay) a = atan_tab[(ax << 8) / ay];
    else a = (u16)(16384 - atan_tab[(ay << 8) / ax]);
    if (east >= 0) return (north >= 0) ? a : (u16)(32768U - a);
    return (north < 0) ? (u16)(32768U + a) : (u16)(0U - a);
}

/* interpolacion en tabla equiespaciada; x en "pasos" Q16 */
s32 fxlerp_tab(const s32 *tab, int n, s32 x)
{
    int i;
    s32 f;
    if (x <= 0) return tab[0];
    i = (int)(x >> 16);
    if (i >= n - 1) return tab[n - 1];
    f = x & 0xFFFFL;
    return tab[i] + fxmul(tab[i + 1] - tab[i], f);
}
