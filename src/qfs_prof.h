/* qfs_prof.h - marcas para medir tiempos en el sQLux de pruebas (el parcheado de QL_XCom/QL_SCUMM, con SQLUX_PROF=fichero).
 * Solo con -DQFS_PROF: escribe en la ROM (direcciones 4 a 11), que el emulador intercepta y un QL real ignora.
 *   PROF_BASE(f)  direccion de la funcion f (para situar el programa respecto al mapa de enlace)
 *   PROF_MARK(n)  marca de tiempo n (ciclos del 68008)
 *   PROF_DUMP(n)  vuelca y borra el perfil por direccion acumulado desde el volcado anterior, con la etiqueta n */
#ifndef QFS_PROF_H
#define QFS_PROF_H
#if defined(QFS_PROF)
#define PROF_BASE(f) { unsigned long pb_ = (unsigned long)(f); *(volatile unsigned char *)8L = (unsigned char)(pb_ >> 24); \
    *(volatile unsigned char *)9L = (unsigned char)(pb_ >> 16); *(volatile unsigned char *)10L = (unsigned char)(pb_ >> 8); \
    *(volatile unsigned char *)11L = (unsigned char)pb_; }
#define PROF_MARK(n) (*(volatile unsigned char *)4L = (unsigned char)(n))
#define PROF_DUMP(n) (*(volatile unsigned char *)5L = (unsigned char)(n))
#else
#define PROF_BASE(f)
#define PROF_MARK(n)
#define PROF_DUMP(n)
#endif
#endif
