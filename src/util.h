/* util.h - cadenas y numeros sin printf (mas pequeno y rapido en el QL) */
#ifndef UTIL_H
#define UTIL_H
#include "qfs.h"
/* escribe v en b con 'w' digitos minimo (relleno con 'pad'); devuelve longitud */
int  u_num(char *b, s32 v, int w, char pad);
/* v en decimas -> "123.4" */
int  u_dec1(char *b, s32 v10, int w);
void u_cat(char *d, const char *s, int max);
void u_catn(char *d, s32 v, int w, char pad, int max);
int  u_len(const char *s);
void u_cpy(char *d, const char *s, int max);
#endif
