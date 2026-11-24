# Rendimiento en el QL: medidas y optimizaciones

## Cómo se mide

`make prof ESCENA=n` (o `tools/ql_prof.sh n`) compila una copia instrumentada con `-DQFS_PROF`, la ejecuta en sQLux y
saca los **ciclos del 68008 por función**: el emulador acumula, instrucción a instrucción, los ciclos de cada dirección
(con los estados de espera por la contención del ZX8301 a `SPEED = 1.0`) y `tools/ql_prof.py` los reparte por funciones
con el mapa de enlace de `qcc -ms`. El código de la biblioteca de C (`.Xlmul`, `.Xldiv`, `.Xlrem`...) se atribuye a la
función que lo llamó. Hace falta el sQLux con el parche de perfil (el de `../QL_SCUMM/tmp/sQLux`; otra ruta con
`SQLUX=`) y las ROM en `~/src/sQLux/build/roms` (`ROMDIR=`).

* **Siempre SPEED = 1,0** (con la contención del ZX8301 y las interrupciones de QDOS). A toda marcha (SPEED 0) el emulador no
  cuenta esos ciclos y equivale a medir el C compilado para el host: sirve para iterar rápido pero no para decidir.
* `PROFFLAGS="-DPROF_N=1 -DPROF_DUMPS=3"`: cuadros por volcado y número de volcados. El primero tras la carga lleva la
  carga de teselas; el estacionario es el último.
* Escenas fijas (`src/prof.c`): 0 cabina en cabecera, 1 final a 5 km sobre Madrid, 2 igual en persecución, 3 final
  nocturno, 4 reactor en persecución a 12 km.
* `SHOT=1 PROFFLAGS="-DPROF_PAUSE=1 -DPROF_HOLD=1"` captura la pantalla del QL (`build/prof/shot_n.bin`) con la física
  parada, para compararla byte a byte con la del PC (`tmp/shot.c`): así se comprobó que el ensamblador nuevo pinta
  **exactamente** lo mismo que la versión en C.

## Qué se encontró

El cuello de botella **no era el panel**, sino el pintado 3D: un fotograma costaba ~104 M de ciclos (14 s) en la
escena 1, unas 100 veces lo previsto. El 68008 solo multiplica 16×16 y divide 32/16; todo lo que c68 hace en 32 bits
(`s32 * s32`, `/`, `%`, `idx*3` con enteros de 4 bytes) llama a la biblioteca (600-1500 ciclos) y `muldiv` contaba bits
de uno en uno. El panel sumaba un ~2 %.

## Qué se hizo (en orden de efecto)

| Cambio | Dónde |
| --- | --- |
| `mdq`: a·b/d con `MULS`+`DIVS` (camino lento exacto solo si no cabe); `nb` por búsqueda binaria | `ql68k.s`, `fixmath.c`, `view3d.c`, `gfx.c` |
| Proyección de todos los vértices del objeto una vez (`proj_all`, ensamblador) y comprobación/copia/descarte trasero de cada cara (`poly_fast`, ensamblador); `area2` con `MULS` | `ql68k.s`, `view3d.c` |
| Caras traseras descartadas **antes** de ordenar (`draw_sorted`: clave y ordenación solo de las visibles) | `view3d.c` |
| Caras de un píxel y sólidos de menos de un píxel no se pintan; alcance de los edificios 5/8 de su `lod`; luces lejanas 1 de cada 2^k | `view3d.c` (`TINY_PX`, `MIN_PX`, `SOLID_LOD`, `LIGHT_U0`) |
| Manzanas de ciudad compactas (celdas de 1500 m, antes franjas de 3-4 km de radio que no se podían descartar) | `tools/mkscen.py` |
| Preparación de vértices fusionada con la transformación (`xform_obj3`): `rel = r + ((v << vshift) >> U)` y `m * rel` en un solo bucle sin vector intermedio | `ql68k.s`, `view3d.c` |
| Relleno de polígonos convexos en ensamblador (`gfx_fill_convex`: convexidad, límites por fila arista a arista con la pendiente en dos DIVU de 16 bits, y las filas con la lógica de `hspan` en registros); el caso recortado o no convexo sigue en C (referencia idéntica para el PC) | `ql68k.s`, `gfx.c` |
| Texto por palabras (`gfx_text_raw`) en vez de un `gfx_pixel` por punto | `ql68k.s`, `gfx.c` |
| Panel: cada instrumento repinta solo si cambia su valor cuantizado; agujas en XOR (`gfx_line_xor`); bola por diferencia; barras por filas; cadenas comparadas | `panel.c` |
| Teclado: 3 filas por cuadro en vez de 8 (cursores siempre; F y M/punto/Z alternadas; las otras 5 en rotación). Cada fila es una transacción IPC de ~125000 ciclos. Retraso medido: fila 0 y 2 ≤ 1 cuadro, fila 7 (Mayús) hasta 4 | `sim.c` (`read_keys`) |
| Indicador de FPS (tecla `\`) abajo a la izquierda | `sim.c`, `panel.c` |
| Buffer 3D y copia limpia del panel en RAM de expansión (≥ $40000, sin contención) con `plat_alloc_hi` | `platform_ql.c`, `sim.c`, `panel.c` |

## Resultado (SPEED 1, ciclos del 68008 por fotograma estacionario)

| Escena | Antes | Ahora, vista entera | Ahora, vista en 2 cuadros (A + B) |
| --- | ---: | ---: | ---: |
| 1 final sobre Madrid | 104 M (14 s) | 9,9 M (1,31 s) | 5,1-6,9 M + 3,9-6,2 M (~1,5 s por vista) |
| 3 final nocturno | ~40 M | 14,4 M (1,93 s) | 7,8-8,6 M + 6,7-8,3 M (~2,1 s por vista) |
| 4 reactor, 12 km | — | 9,1 M (1,21 s) | 5,4-6,3 M + 3,7-4,3 M (~1,4 s por vista) |

## La vista 3D en dos cuadros

`sim.c` parte la vista en dos trabajos que se ejecutan en cuadros consecutivos (`VIEW_SPLIT`, 45 por omisión; 0 = vista entera):

* **Cuadro A:** cámara, cielo, reunir y ordenar los objetos (`view_world_begin`) y dibujar los primeros hasta gastar el 45 % del
  coste estimado (vértices + caras, `view_world_step`).
* **Cuadro B:** el resto de objetos, el avión (vistas exteriores), el aviso de choque y el volcado del buffer a pantalla.

Teclado, física, radios, ATC y panel corren en **todos** los cuadros (la física, a 2 pasos por cuadro: siguen siendo 4 por
vista). La cámara y el avión se fijan en A, así que la imagen de B es coherente. Los tiles de la vista no pueden desalojarse
entre A y B (`world_tiles_near` los marca y solo la vista lo llama). Con la física parada la imagen es idéntica byte a byte a
la de la vista entera en las 5 escenas.

A y B quedan equilibrados (A 5,1-8,6 M, B 3,7-8,3 M). Cada cuadro dura 0,5-1,2 s en vez de 1,2-1,9 s, así que **teclado,
panel y simulación se atienden al doble de ritmo** (`3D 0.6  SIM 1.2` en el indicador), y a cambio la vista tarda un 9-16 %
más (cada cuadro repite el coste fijo de panel, teclado y 2 pasos de física, ~0,2 s). Sin limitar la física a 2 pasos por
cuadro salía un 30-40 % peor.

## Lo que queda (por orden de rentabilidad estimada)

1. **Normales por cara** (el truco de Elite): descartar las caras traseras con un producto escalar antes de proyectar y
   proyectar solo los vértices que usa alguna cara visible. Aquí es más fácil que en Elite porque los edificios y
   montañas no giran: la normal y el plano de cada cara son constantes. Ahorra ~5 % y hasta ~15 % si se evita proyectar
   los vértices huérfanos. Pide guardar el plano en el escenario (`mkscen.py`, `world.c`, teselas).
2. (hecho) Preparación de vértices de `xform_obj` fusionada con `xform3` (`xform_obj3`: 0,15 s → 0,10 s) y el prólogo de descarte
   (`xform_pre`: distancia, escala U, esfera envolvente contra la pirámide de visión y tamaño) en ensamblador: la etapa completa
   (`xform_obj` + `xform_pre` + `xform3`) baja de ~0,040 s a 0,032 s por fotograma (escena 1, SPEED 1), un 0,6 % del fotograma.
   Se llama ~63 veces por fotograma a ~2,3 k ciclos cada una, que es casi el mínimo del 68008 con bus de 8 bits (cada
   instrucción con operandos en memoria cuesta 25-45 ciclos); no hay mucho más que sacar de aquí. Las multiplicaciones de 32x32
   de la biblioteca no eran el problema (cambiarlas por una 32x16 en ensamblador no movió la medida); sí lo es el número de
   instrucciones de c68. Ojo: `xform_pre` lee `Camera` y `Obj` por desplazamientos fijos, comprobados en compilación (`FOFF`).
   Lo que pesa de verdad aquí es `view_world_begin` (0,12 s: reunir y ordenar ~300 objetos por inserción), 4 veces más.
3. (hecho) Rasterizador de polígonos en ensamblador: −8 a −11 % del fotograma; lo que queda en C (recorte `clip_edge`, no convexos) es ~2 %.
4. `view_world` (reunir y ordenar objetos, ~11 %): ordenar índices, no estructuras.
5. (hecho) Lectura de teclado parcial. Con cuadros de ~1,5 s una pulsación corta puede caerse entre dos lecturas: mantener la tecla.
6. La física cuesta ~35 ms por paso (`flight_step` y `world_hit`): a 25 Hz necesitaría más de un segundo de CPU por segundo, así que
   la simulación va muy por debajo del tiempo real; abaratar `world_hit` o bajar la frecuencia de la física. Menos detalle de
   terreno (más `lod` en `iberia.py`).
