/* lang.h - idioma de los textos por preprocesador (sin herramientas externas), como QL_Mac (src/sdk/qm_i18n.h).
 * Los textos del menu principal estan en una cabecera por idioma:
 *     src/qfs_es.h:  #define S_MENU_TITLE  "QFS - SIMULADOR DE VUELO PARA SINCLAIR QL"
 *     src/qfs_en.h:  #define S_MENU_TITLE  "QFS - FLIGHT SIMULATOR FOR THE SINCLAIR QL"
 * y el fuente los usa por su nombre con  #include QFS_TEXTS(qfs).  El idioma se elige al compilar con -DQFS_LANG=_en
 * (make QLANG=en); sin el, espanol (_es). Todo se resuelve al compilar: sin tablas ni busquedas, mismo tamano y velocidad.
 * Reglas: los formatos y los separadores se conservan en todos los idiomas, y las letras de las teclas del menu (T E A S D G L R)
 * tambien: el texto es otro pero la tecla es la misma. make lang-check comprueba que las dos cabeceras definen los mismos textos. */
#ifndef LANG_H
#define LANG_H
#ifndef QFS_LANG
#define QFS_LANG _es
#endif
#define QFS_STR_(x) #x
#define QFS_STR(x) QFS_STR_(x)
#define QFS_CAT_(a, b) a##b
#define QFS_CAT(a, b) QFS_CAT_(a, b)
#define QFS_TEXTS(name) QFS_STR(QFS_CAT(name, QFS_LANG).h)
#endif
