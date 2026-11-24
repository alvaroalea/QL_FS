#include "util.h"

int u_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

int u_num(char *b, s32 v, int w, char pad)
{
    char t[12];
    int n = 0, neg = v < 0, i, k = 0;
    if (neg) v = -v;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11);
    if (neg) { if (pad == '0') { b[k++] = '-'; w--; } else t[n++] = '-'; }
    for (i = n; i < w; i++) b[k++] = pad;
    while (n) b[k++] = t[--n];
    b[k] = 0;
    return k;
}

int u_dec1(char *b, s32 v, int w)
{
    int n;
    s32 a = ABS(v);
    if (v < 0) { *b++ = '-'; w--; }
    n = u_num(b, a / 10, w > 2 ? w - 2 : 1, ' ');
    b[n] = '.'; b[n + 1] = (char)('0' + a % 10); b[n + 2] = 0;
    return n + 2 + (v < 0);
}

void u_cat(char *d, const char *s, int max)
{
    int n = u_len(d);
    while (*s && n < max - 1) d[n++] = *s++;
    d[n] = 0;
}

void u_catn(char *d, s32 v, int w, char pad, int max)
{
    char t[14];
    u_num(t, v, w, pad);
    u_cat(d, t, max);
}

void u_cpy(char *d, const char *s, int max)
{
    int n = 0;
    while (s[n] && n < max - 1) { d[n] = s[n]; n++; }
    d[n] = 0;
}
