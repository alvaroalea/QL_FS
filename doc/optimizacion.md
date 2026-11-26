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
   montañas no giran: la normal y el plano de cada cara son constantes. Medido (sección «Ronda de ensamblador»): ahorra 2-6 %
   (~8 % en ciudad densa), no el 5-15 % estimado, porque la mayoría de los vértices son de suelo. Pide guardar el plano
   en el escenario (`mkscen.py`, `world.c`, teselas). Sin aplicar.
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

## Reunión de objetos y colisiones (escena 1, SPEED 1, volcado estacionario)

Total de la ventana de perfil: 14,57 s → 12,57 s (-14 %).

| Cambio | Antes | Después |
| --- | ---: | ---: |
| `view_world_begin` (ahora `gather_tile` + resto) | 1,44 s | 0,27 + 0,15 s |
| `world_hit` | 0,71 s | ~0 |
| `tile_get` | 0,21 s | 0,10 s |

* **`gather_tile`**: el alcance de cada objeto (`radius + lod*256*SOLID_LOD/8`) se calcula una vez al cargar la tesela
  (`Obj.reach`) en vez de hacer una multiplicación y una división de 32 bits por objeto y cuadro; el bucle usa punteros.
* **`tile_get`**: tabla `slot` (tesela → entrada de la cache) en vez de buscar entre 49 entradas por cada tesela.
* **`world_hit`**: lista compacta de los objetos con `OF_HIT`/`OF_WATER` (se rehace solo cuando carga una tesela, `w->gen`)
  y, sobre todo, **comprobación espaciada adaptativa**: `world_hit` deja en `w->hit_near` la distancia horizontal al objeto
  más cercano; `flight_step` no vuelve a llamarlo hasta que el avión haya podido recorrerla (a `gs + v` con margen ×2),
  como máximo `HIT_SKIP_MAX` = 25 pasos (1 s). Es exacto: no se puede chocar con un objeto antes de entrar en su radio.
  `tests/hittest.c` (compilar con `-DHIT_SKIP_MAX=0` para la referencia) vuela contra 40 objetos a 30, 90 y 250 m/s y
  comprueba que el choque sale en el mismo paso.

## Cómo se dibuja la vista 3D

**Hay un único buffer intermedio** (`backbuf`, 96 filas × 128 bytes = 11.520 B, en RAM alta sin contención, `plat_alloc_hi`):
la vista se pinta entera ahí y se vuelca de golpe a la pantalla (`gfx_memcpy`, ~29 ms) al final del cuadro B. No hay dos
buffers que se intercambien: la pantalla del QL no permite cambiar de base de vídeo por hardware, así que se copia. El panel
(filas 100 en adelante) no pasa por ese buffer: se pinta directo en pantalla, un grupo de instrumentos por cuadro
(`PANEL_PARTS` = 4) y solo lo que cambia.

Orden de pintado (pintor, de lejos a cerca; `sim.c` `view_part_a`/`view_part_b` y `view3d.c`):

1. **Cielo** (`view_sky`): una fila a la vez, con 1-3 `gfx_hspan` por fila (suelo/bruma/cielo según el horizonte inclinado por el
   alabeo). Dentro de una nube, `gfx_clear` y nada más.
2. **Reunir** (`view_world_begin`): `gather_tile` recorre los objetos de las teselas cercanas y descarta por alcance (`reach`),
   visibilidad, luz (día/noche), nubes y detrás de la cámara. Quedan en `items[]` con una clave.
3. **Ordenar** por inserción, clave descendente: primero el **suelo** (`OBJ_GROUND`, los grandes antes: campos, lagos, pistas),
   luego las **líneas** (`OBJ_LINES`: carreteras, ríos; lejos → cerca) y por último **sólidos y luces** mezclados por distancia.
4. **Dibujar** (`view_world_step`), por objeto: `xform_obj` (descarte en `xform_pre`, transformación en `xform_obj3`,
   proyección de todos los vértices con `proj_all`) y luego `draw_obj`:
   suelo → `draw_poly` sin descarte trasero; sólido → `draw_poly` con descarte, o `draw_sorted` si lleva `OF_SORT` (edificios
   cóncavos y montañas: se descartan las caras traseras, se ordenan las visibles por profundidad media y se pintan);
   líneas → `draw_seg` (recorte al plano cercano + `gfx_line`); luces → un punto (dos si está cerca) por luz.
   Cada cara: `poly_fast` (todos los vértices delante + área con signo) o `clip_project` (recorte 3D contra el plano cercano);
   `paint_face` rellena (`gfx_poly32`, en ensamblador: caja y descarte, `clip_edge` si toca el borde de la ventana y `gfx_fill_convex`; un polígono no convexo va a `gfx_poly_gen`, en C) y, si procede, contornea.
5. **El propio avión** (`view_model`, solo en vistas exteriores): caras ordenadas por profundidad, sin ocultación; encima de todo.
6. Aviso de choque (rectángulo y texto) y **volcado** del buffer a la pantalla.

Con `VIEW_SPLIT` los pasos 1-4 (hasta el 45 % del coste) van en el cuadro A y el resto en el B; el buffer queda a medias entre
los dos cuadros, que es invisible porque solo se vuelca al final del B.

## Profile de ruta hacia ×10-×20 FPS (noviembre 2026)

Objetivo: de ~1 FPS a 10-20 FPS = **100 ms (750 k ciclos) a 50 ms (375 k ciclos) por vista**. Medido con `make prof` (SPEED 1)
en las escenas 0, 1, 3 y 4; un volcado son 25 cuadros = 12,5 vistas.

| Escena | ms por vista | FPS |
| --- | ---: | ---: |
| 1 final sobre Madrid | 1.006 | 1,0 |
| 3 final nocturno | 1.543 | 0,65 |
| 0 cabina en cabecera | ~1.290 | 0,8 |
| 4 reactor a 12 km | ~930 | 1,1 |

### Reparto por etapas, escena 1 (ms por vista)

| Etapa | ms | % |
| --- | ---: | ---: |
| Transformar y proyectar vértices (`proj_all`, `xform_obj3`, `xform_pre`, `xform_obj`) | 216 | 21 |
| Descarte y recorte de polígonos (`poly_fast`, `clip_edge`, `gfx_poly32`) | 148 | 15 |
| Relleno (`gfx_fill_convex`, `hspan`) | 97 | 10 |
| Física, motores, navegación, ATC | 86 | 9 |
| `fxmul`/`fxdiv` (en su mayoría la física) | 83 | 8 |
| Líneas y puntos (`gfx_line_raw`, `line_clip`, `draw_seg`) | 81 | 8 |
| Reunir y ordenar objetos y teselas | 78 | 8 |
| ROM/QDOS (teclado por IPC) | 69 | 7 |
| Cielo | 65 | 6 |
| Panel, texto, sonido | 37 | 4 |
| Volcado buffer → pantalla | 29 | 3 |
| Otros | 17 | 2 |

### Conclusiones

1. **La física cuesta ~230 ms por vista (23 %)**: con `-DPROF_PAUSE=1` la vista baja de 1.006 a 774 ms. Son ~58 ms (~435 k ciclos)
   por paso y hay 4 pasos por vista (2 por cuadro). Cada paso hace ~79 `fxmul` y ~14 `fxdiv`. A 25 Hz haría falta más de 1 s de
   CPU por segundo. Es lo mayor que no es render.
2. **La escena es pequeña y cada elemento es carísimo** (instrucciones, no bus). Por vista, escena 1 (contado en el host con
   `vstats` y gprof): ~24 objetos, ~306 vértices, ~110 caras probadas, 26 pintadas, 14 líneas, 20 recortes (`clip_edge`).
   Costes por elemento en el QL:
   * `proj_all`: ~1.900 ciclos por vértice. `pa_mdq` normaliza el divisor desplazando **bit a bit** en un bucle (`pm_nrm`).
   * `xform_obj3`: ~1.400 ciclos por vértice.
   * `clip_edge`: ~3 ms por llamada (61 ms por vista), `gfx_line`: ~3 ms por línea (44 ms por vista).
3. **Suelo fijo de ~190-200 ms por vista** antes de dibujar nada: cielo 63, teclado/QDOS 52-69, panel 31-37, volcado 27-29.
   El volcado solo ya es más de la mitad del presupuesto de 50 ms del ×20.
4. **La contención del ZX8301 solo pesa ~20 %**: con `ZX8301_CONTENTION = 0` en sQLux el mismo volcado pasa de 13,5 s a 10,8 s.
   El límite es la CPU. (`tools/ql_prof.sh` no pasa esa opción; se probó con una copia del script.)
5. **De noche (escena 3)** hay 45 objetos de luces con 3.365 vértices. Las luces se proyectan con `project`/`muldiv` en C y
   `gfx_pixel`: `MULDIV` + `MDQ` + `PROJECT` + `DRAW_OBJ` + `GFX_PIXEL` + `XFORM_OBJ3` son ~1 s por vista.
6. **Dos objetos de suelo (bloques urbanos de 96 vértices a 6-7 km) tienen 192 de los 306 vértices de la escena 1.** Se dibujan
   también montañas a 31-43 km con `vis` de 24 km.

### Qué hace falta (estimaciones, no medidas)

| Paquete | Ahora (ms/vista) | Objetivo |
| --- | ---: | ---: |
| Física: 1 paso por vista con dt mayor, o paso más barato (`fxmul` en ensamblador, `engines_step`) | ~230 | ~25 |
| Proyección con recíproco (1 `DIVU` por vértice, normalización por tabla) y transformación más barata | ~216 | ~60 |
| Recorte en 16 bits tras proyectar con salida rápida si el polígono cabe; líneas en ensamblador | ~230 | ~60 |
| Cielo por palabras o `MOVEM`, saltándose lo que no se ve | 65 | ~8 |
| Lista de objetos visibles reutilizada entre vistas y reordenación incremental | 78 | ~15 |
| Teclado: una fila IPC por vista (cada una ~17 ms) | 52-69 | ~17 |
| Contenido: `vis` de 8-10 km, LOD de los bloques urbanos, tope de vértices de luces | — | −50 % de vértices |

* **×10**: con todo lo anterior la optimización pura deja ~250-300 ms por vista (3-4 FPS). Para 10 FPS hace falta además un
  presupuesto duro de contenido (~100 vértices y ~30 caras por vista) y llevar el 3D a ~30 ms. Posible, pero exige tocar todas
  las etapas.
* **×20**: con la ventana de 480×96 no es alcanzable (volcado 29 + cielo 8 + teclado 17 ms ya llenan los 50 ms). Habría que
  reducir la ventana (p. ej. 480×64 o 320×96), volcar solo las filas que cambian y leer el teclado con menos frecuencia.

Orden propuesto: física → proyección por recíproco y recorte en 16 bits → cielo, teclado y lista de objetos reutilizada →
presupuesto de contenido → decidir si se acepta una ventana menor para el ×20.

## Ronda de ensamblador: proyección, recorte y relleno (noviembre 2026)

Objetivo: pasar a ensamblador lo que quedaba en C de las tres primeras etapas del reparto de arriba (transformar y proyectar
vértices, descartar y recortar polígonos, rellenar) y ver qué más se podía sacar de la física. Medido como antes (`make prof`,
SPEED 1, volcado estacionario de 2 cuadros = una vista) con la **imagen del QL idéntica byte a byte** a la de antes en las escenas
1, 3 y 4.

| Cambio | Dónde |
| --- | --- |
| **Proyección en un solo camino exacto** (`pj_xy`, usada por `proj_all` y `proj_pt`): `x*fx` con `MULS` y `DIVS`; si el cociente no cabe en 16 bits (vértice muy fuera de pantalla) se divide el valor absoluto con dos `DIVU` encadenadas (32/16 → 32 bits). Antes `pa_mdq` normalizaba el divisor bit a bit y, en cuanto el cociente desbordaba, llamaba a `muldiv` en C (~1.900 ciclos por vértice). Si x, y o z no caben en 16 bits se desplazan los tres a la vez. `proj_all` devuelve cuántos vértices están delante (adiós al bucle de descarte en C de `xform_obj`) | `ql68k.s`, `view3d.c` |
| **Luces** (`draw_lights`): proyección, recorte y punto (o dos) por luz en ensamblador; antes `project` + `muldiv` + `gfx_pixel` en C por cada luz | `ql68k.s`, `view3d.c` |
| **Recorte**: `lerp_div` exacto (MULU + 2 DIVU si cabe en 16 bits; si no, producto de 64 bits y división 64/32), `clip_edge` y `gfx_poly32` completo (caja, descarte, cuatro pasadas de recorte y relleno) en ensamblador; el C solo queda para los no convexos (`gfx_poly_gen`). `clip_project` y `draw_seg` usan `lerp_div` (se elimina `muldiv`) | `ql68k.s`, `gfx.c`, `view3d.c` |
| **Descarte**: `poly_fast` en una sola pasada (comprueba que los vértices están delante, copia y lleva la caja a la vez); el valor máximo para el área sale de la caja, no de cada vértice | `ql68k.s` |
| **Líneas**: `gfx_line` resuelve en ensamblador la aceptación y el rechazo triviales; solo las que cruzan el borde van al recorte en C (`gfx_line_c`) | `ql68k.s`, `gfx.c` |
| **Relleno**: `fc_hspan` con tablas de máscaras de los extremos, mezcla `v ^ ((v ^ p) & m)` y escritura de palabras largas (la 68008 admite accesos largos en cualquier dirección); `gfx_hspan_raw` la comparte y `gfx_hspan` (recorte a la superficie) pasa a ensamblador | `ql68k.s`, `gfx.c` |
| **Transformación**: `xform_obj3` con desplazamiento neto en palabras (sin `ext.l` ni doble desplazamiento) | `ql68k.s` |
| **Física**: `fxmul` con vía corta de dos `MULU` cuando un operando cabe en 16 bits (constantes < 1,0, razones...) | `ql68k.s` |

Para comprobar el ensamblador sin mirar imágenes: `make prof ESCENA=1 PROFFLAGS="-DASM_TEST" NDUMPS=0` (src/prof.c, `asm_test`) compara
`lerp_div`, `proj_pt`, `fxmul`, `gfx_hspan` y `gfx_line` con referencias en C sobre valores aleatorios y deja los errores en marcas
`100 + n` (0 = sin discrepancias).

### Resultado (ciclos del 68008 por vista, SPEED 1)

| Escena | Antes | Ahora | |
| --- | ---: | ---: | ---: |
| 1 final sobre Madrid | 7,50 M (1,00 s) | 6,35 M (0,85 s) | −15 % |
| 3 final nocturno | 11,52 M (1,54 s) | 6,95 M (0,93 s) | −40 % |
| 4 reactor, 12 km | 7,33 M (0,98 s) | 6,32 M (0,84 s) | −14 % |

La noche gana más porque las luces eran todo C. Escena 1 por etapas, ahora (s por vista): proyección (`pj_xy` + `pj_div` + `proj_all`)
0,063; `xform_obj3` 0,057; `poly_fast` 0,046; relleno (`fc_hspan` + `gfx_fill_convex` + `gfx_hspan`) 0,114 (incluye las filas del cielo);
recorte (`clip_edge` + `gfx_poly32`) 0,017 (antes 0,037).

**Lo que no se movió**: `xform_obj3` sigue en ~1.400 ciclos por vértice aunque se quitaron los desplazamientos largos. Cada vértice
son 9 `MULS` + 18 lecturas de la matriz + 9 sumas y 6 desplazamientos: ~50 instrucciones a 25-30 ciclos con el bus de 8 bits. Solo
bajaría con menos vértices (ver normales) o menos instrucciones por vértice. Tampoco bajó el relleno de polígonos pequeños: las
filas son cortas (unas decenas de píxeles) y mandan el preámbulo de cada fila y el trazado de aristas, no las palabras interiores.

### Normales por cara (el truco de Elite): medido, no aplicado

Se instrumentó `view3d.c` en el PC (5 aeropuertos, a 1,5 y a 6 km de la cabecera, 8 vistas cada uno) y se contó:

| | vértices de suelo | vértices de sólidos | caras de sólidos | traseras | vértices huérfanos |
| --- | ---: | ---: | ---: | ---: | ---: |
| Madrid 1,5 km | 1.952 | 1.520 | 1.064 | 65 % | 600 (39 % de los sólidos) |
| Barcelona 6 km | 2.200 | 2.112 | 1.320 | 58 % | 592 (28 %) |
| Palma 1,5 km | 1.552 | 2.256 | 1.448 | 78 % | 1.272 (56 %) |
| Málaga 1,5 km | 1.656 | 1.416 | 904 | 77 % | 768 (54 %) |
| Tenerife 1,5 km | 1.848 | 272 | 192 | 67 % | 136 (50 %) |

(*huérfano* = vértice de un sólido que no usa ninguna cara visible; las caras de suelo nunca se descartan por detrás.)

Conclusiones:

* Lo que se ahorraría: la proyección y la transformación de los huérfanos (del 2 % de **todos** los vértices a 6 km al 33 % en Palma a
  1,5 km; lo normal, 5-15 %) y el `poly_fast` de las traseras (60-75 % de las caras de sólidos), que se cambiaría por un producto
  escalar de 3 `MULS`. Con los costes de ahora (transformar + proyectar ≈ 14 % de la vista, `poly_fast` ≈ 5 %) sale **un 2-6 % del
  fotograma, y ~8 % en una ciudad densa y cercana**. Menos de lo que se pensaba (5-15 %) porque entre la mitad y dos tercios de los
  vértices son **suelo**, que no tiene caras traseras que descartar.
* Lo que cuesta: un plano por cara en la tesela (normal en 3 × s8 + distancia en s16 o s32, ~8 bytes por cara, 25-30 % más de memoria
  de caras), regenerar los escenarios (`mkscen.py`) o calcularlo al cargar la tesela (`tile_parse`), y rehacer la transformación para
  que trabaje sobre una lista de vértices marcados (dos pasadas: caras visibles → vértices usados → transformar y proyectar solo esos).
  Con las cámaras de la vista exterior y el balanceo hay que pasar el vector cámara→objeto al espacio del objeto (3 `MULS` por objeto).
* Decisión: **no se aplica todavía**. Se queda en la lista; antes rinde más recortar vértices de suelo (los bloques urbanos de 96
  vértices a 6-7 km son 192 de los ~306 vértices de la escena 1: LOD o menos manzanas lejos) y el cielo (~10 % del fotograma entre
  `view_sky`, `sky_row`, `gfx_hspan`: tiene dos divisiones de 32 bits y dos productos de 32 bits por fila que se pueden hacer
  incrementales).

### Física: qué queda

Medido con la escena 1 (por vista = 4 pasos): `flight` ~0,03 s (3 %), `fxmul` ~0,03 s (3,5 %, repartido entre la física y el
renderizado), `fxdiv` 0,004 s, `nav`/`atc`/`aircraft` por debajo del 0,5 %. Ya hecho en esta ronda: la vía corta de `fxmul`. Revisado y
descartado:

* `engines_step` (el caro del paso) ya corre cada 4 pasos; `world_ground` ya es una lectura de tabla; la colisión ya se espacia.
* `flight_step` cuesta ~0,003 s por paso (1,4 % de la vista): pasarlo entero a ensamblador no llega al 1 % del fotograma y duplicaría
  el modelo (hay que mantener la versión en C para el PC y las pruebas).
* `fxsin`/`fxcos` (8 por paso) hacen un `fxmul` y una lectura de tabla; y la vía corta de `fxmul` ya les ayuda.
* Lo que sí quedaría es bajar la frecuencia (2 pasos por vista con dt doble) o dejar `fxdiv` en 16 bits cuando cabe; ambos cambian
  la dinámica y no son optimizaciones puras.

## Física (paso de 1/25 s)

Medido en la escena 1 (módulo `flight.c`, SPEED 1, por 25 cuadros): **1,09 s → 0,35 s**; `nav.c` 0,37 s → 0,16 s; total 12,57 s → ~11,6 s.

* `fxdiv` en ensamblador (`ql68k.s`, `-DUSE_ASM_FXDIV`): mismo resultado que la versión C (comprobado con `-DFXDIV_TEST` en
  `prof.c`: 6000 pares aleatorios sin diferencias), con normalización binaria y `DIVU` en vez de la división de 32 bits de la biblioteca.
* Atmósfera (`sigma`, `delta`, `KL·sigma`, `√sigma`) solo al cambiar la altitud en metros; objetivo de flaps solo al cambiar la palanca.
* Motores, empuje, asimetría, combustible y velocidad sobre el suelo (`isqrt32`) cada `ENG_N` = 4 pasos con el paso de tiempo
  multiplicado (los motores tienen constantes de tiempo ≥ 0,5 s).
* Sin productos/divisiones de 32 bits de la biblioteca en el paso normal: `vi*vi` con `MULS16`, `1/masa` precalculada, `v/vref` una sola
  vez (antes 4 `fxdiv`), constantes como productos, índice de `world_ground` con `MULS16`, segunda lectura de suelo redundante eliminada,
  contador en vez de `nsteps % 12` (`sim.c`), punteros en vez de `nav[i]` en `nav.c`.
* La dinámica sale igual salvo redondeos (±0,5 % en las pruebas de `hosttest`).
