# Rendimiento en el QL: medidas y optimizaciones

## Cómo se mide

`make prof ESCENA=n` (o `tools/ql_prof.sh n`) compila una copia instrumentada con `-DQFS_PROF`, la ejecuta en sQLux y
saca los **ciclos del 68008 por función**: el emulador acumula, instrucción a instrucción, los ciclos de cada dirección
(con los estados de espera por la contención del ZX8301 a `SPEED = 1.0`) y `tools/ql_prof.py` los reparte por funciones
con el mapa de enlace de `qcc -ms`. El código de la biblioteca de C (`.Xlmul`, `.Xldiv`, `.Xlrem`...) se atribuye a la
función que lo llamó. Hace falta el sQLux con el parche de perfil (la rama `profile` de `~/src/sQLux`, ya compilada en `build/sqlux`; otra ruta con
`SQLUX=`) y las ROM en `~/src/sQLux/build/roms` (`ROMDIR=`).

* **Planificador (`PROF_SCHED_RUN`): ×1, ×2, ×8 y ×40** (`SPEEDV=<n>`: QL, Gold Card, Super Gold Card, ilimitado; ver la parte «Planificador por turnos», secciones 8 y 9). El perfil por función, en cambio:
* **Siempre SPEED = 1,0** (con la contención del ZX8301 y las interrupciones de QDOS). A toda marcha (SPEED 0) el emulador no
  cuenta esos ciclos y equivale a medir el C compilado para el host: sirve para iterar rápido pero no para decidir.
* `PROFFLAGS="-DPROF_N=1 -DPROF_DUMPS=3"`: cuadros por volcado y número de volcados. El primero tras la carga lleva la
  carga de teselas; el estacionario es el último.
* Escenas fijas (`src/prof.c`): 0 cabina en cabecera, 1 final a 5 km sobre Madrid, 2 igual en persecución, 3 final
  nocturno, 4 reactor en persecución a 12 km.
* `SHOT=1 PROFFLAGS="-DPROF_PAUSE=1 -DPROF_HOLD=1"` captura la pantalla del QL (`build/prof/shot_n.bin`) con la física
  parada, para compararla byte a byte con la del PC (`tmp/shot.c`): así se comprobó que el ensamblador nuevo pinta
  **exactamente** lo mismo que la versión en C.

> **Estado actual (27 nov 2026):** las cifras de las secciones intermedias son históricas, de cuando se midieron. Las vigentes están en
> la sección «Ronda 3» (panel, luces en filas, marcas de pista, ordenación y el resumen con el pendiente); «Reevaluación y perfil del
> simulador actual» es la ronda anterior. Al final se han integrado tres estudios que antes eran ficheros aparte: «Horizonte artificial y rosa del
> rumbo», «Planificador por turnos» (interrupciones de 50 Hz; sus secciones 1–9 son las de ese bloque) y «Memoria alta» (pila y montón en la RAM sin contención).

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

## Resultado (SPEED 1, ciclos del 68008 por fotograma estacionario) — histórico, de esa primera ronda

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

## Lo que queda (por orden de rentabilidad estimada) — histórico; la lista vigente está en «Ronda 3»

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
6. (superado) La física costaba ~35 ms por paso. Tras `world_hit` espaciado, `fxdiv`/`fxmul` en ensamblador y motores cada 4 pasos
   ya no es el cuello de botella: ~8 % en vuelo y solo ~2 % si se para (ver «Ronda 3»).

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

1. **(medida de la ronda anterior; hoy ~2-8 %) La física cuesta ~230 ms por vista (23 %)**: con `-DPROF_PAUSE=1` la vista baja de 1.006 a 774 ms. Son ~58 ms (~435 k ciclos)
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
`lerp_div`, `proj_pt`, `fxmul`, `gfx_hspan`, `gfx_line` y `sdiv32` con referencias en C sobre valores aleatorios y deja los errores en marcas
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

**Cielo** (después de la tabla): `view_sky` calcula `B` de forma incremental (una resta por fila en vez de dos productos de 32 bits) y `sky_row` divide con `sdiv32` (ensamblador: dos `DIVU` si |divisor| < 65536; antes la división de 32 bits de la biblioteca, dos por fila). Escena 1: 6,35 → 6,08 M (−4 %), escena 3: 6,95 → 6,68 M (−4 %); imagen idéntica byte a byte; `sdiv32` comprobada en `asm_test` (marca 105).

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

## Reevaluación y perfil del simulador actual (27 noviembre 2026)

Perfil con `make prof` (SPEED 1, volcado estacionario = 25 cuadros = 12,5 vistas de 2 cuadros). Estado: renderizado en
ensamblador, vista en dos cuadros, física a 2 pasos por cuadro, luces verdes de eje en las calles de rodaje (nuevas).

| Escena | s por volcado | ms por vista | FPS de vista | Antes (ronda de ensamblador) |
| --- | ---: | ---: | ---: | ---: |
| 0 cabina en cabecera (Madrid, día) | 18,17 | 1.454 | 0,69 | ~1.290 |
| 1 final a 5 km sobre Madrid | 7,80 | 624 | 1,60 | 850 |
| 3 final nocturno | 11,61 | 929 | 1,08 | 930 |
| 4 reactor, 12 km | 10,24 | 819 | 1,22 | 840 |
| 1 con la física parada (`PROF_PAUSE=1`) | 7,67 | 614 | 1,63 | — |

* Los dos primeros volcados de cada escena son más lentos (carga de teselas: escena 1, 11,4 s y 10,1 s frente a 7,8 s).
* **Luces de rodaje**: A/B con el mismo escenario sin ellas: escena 3 11,36 s → 11,61 s (**+2 %**), escena 0 sin cambio (de día no
  se dibujan; solo se reúnen los 127 objetos más).

### Reparto (% del volcado)

| Etapa | Esc. 0 (día, suelo) | Esc. 1 | Esc. 3 (noche) | Esc. 4 |
| --- | ---: | ---: | ---: | ---: |
| Transformar y proyectar (`xform_obj3`, `xform_pre`, `xform_obj`, `pj_xy`, `pj_div`, `proj_all`) | **27** | 16 | **38** | 11 |
| Luces (`draw_lights`, `dl_plot`) | — | 1 | **13** | 1 |
| Descarte/recorte de polígonos (`poly_fast`, `clip_*`, `gfx_poly32`, `draw_sorted`, `paint_face`) | 18 | 11 | 3 | 8 |
| Relleno (`fc_hspan`, `gfx_fill_convex`, `gfx_hspan`) | 13 | 8 | 5 | 18 |
| Reunir/ordenar (`gather_tile`, `view_world_begin`, `tile_get`, `world_tiles_near`, `dist_m`) | 9 | 11 | 9 | 7 |
| ROM/QDOS (teclado por IPC) | 7 | 7 | 7 | 7 |
| Cielo (`view_sky`, `sky_row`) | 2 | 5 | 3 | 4 |
| Volcado a pantalla (`gfx_memcpy`) | 2 | 5 | 3 | 3 |
| Líneas (`gfx_line_raw`, `draw_seg`) | 4 | 7 | 1 | 7 |
| Física (`flight_step`, `fxmul`, `isqrt32`, `nav`) | ~3 | ~8 | ~7 | ~8 |

### Conclusiones

1. **Lo que domina es el número de vértices**, no el relleno. En la escena 0 (cabina en el aeropuerto), `xform_obj3` es el 10,6 %
   y `poly_fast` el 8,9 %: ~800 vértices transformados por vista a ~1.400 ciclos cada uno. De noche son los vértices de las luces
   (unos 3.400 en alcance): transformar + proyectar + dibujar luces suman **~51 % del volcado**.
2. **Física: ya no es el objetivo.** Sumando `flight_step` + `fxmul` + `isqrt32` salen ~8 % en vuelo (el `fxmul` y el `isqrt32` también
   los usa el render); parando la física solo se gana el 1,7 % (7,80 → 7,67 s). Todo lo que se haga ahí rinde < 3 %.
3. **Teclado (ROM/QDOS), 7 %, y volcado, 2-5 %** son un suelo fijo por vista; el volcado es entero aunque solo cambie parte.
4. **Normales por cara**: sigue sin compensar (2-8 %, y el suelo no tiene caras traseras).
5. El escenario de cabecera es el peor (0,69 FPS de vista): marcas de pista (objetos de 80 vértices cada 600 m, `lod` 8), calles y
   plataformas son suelo plano que se transforma entero aunque cada trozo cubra pocos píxeles.

### Qué optimizar, por orden de rentabilidad

| # | Propuesta | Escenas | Ganancia estimada |
| --- | --- | --- | ---: |
| 1 | **Filas de luces como primitiva**: guardar extremos + paso y proyectar solo los extremos, interpolando en pantalla el recíproco (1/z) de cada luz; así las luces de borde de pista, eje de calle y aproximación pasan de 1 vértice a ~0 por luz | 3 (noche) | −25 a −35 % |
| 2 | Afinar el recorte de luces por distancia (`LIGHT_U0` > 0, tope de luces por vista, calles solo a < 2 km) | 3 | −5 a −15 % |
| 3 | Menos vértices de marcas y suelo cerca de aeropuertos: eje discontinuo en objetos de `lod` menor o como líneas, fusionar calles contiguas en un solo objeto | 0, 1 | −8 a −15 % |
| 4 | Reunir/ordenar solo cuando la cámara se ha movido (reutilizar `items[]` entre vistas) | todas | −5 a −8 % |
| 5 | Teclado: una fila IPC por cuadro cuando no hay pulsaciones | todas | −3 a −5 % |
| 6 | Volcado solo de las filas modificadas (ventana por bandas) | 1, 3, 4 | −2 a −3 % |
| 7 | Normales por cara | 0 | −2 a −8 % (más coste de datos) |
| 8 | Física a 2 pasos por vista | todas | −2 a −3 % (cambia la dinámica) |

Con 1 + 2 + 3 + 4 se esperan ~0,5 s por vista en la escena 3 y ~1,1 s en la 0: bastante lejos del objetivo de 10 FPS. Para
llegar ahí sigue haciendo falta un presupuesto duro de contenido (vértices y caras por vista) y, probablemente, una ventana menor.

### Cambios posteriores que afectan al rendimiento o a las medidas

* **Looping** (`flight.c`): al pasar de ±90° la trayectoria se refleja (`gam → π − gam`, rumbo y alabeo +180°, indicador `inv`) en vez de
  quedarse limitada a ±89°, que dejaba al avión parado en la vertical. Añade una comparación por paso; no se nota en el perfil.
* **Luces verdes de eje de calle** (`mkscen.py`, `taxi_lights`): una luz cada 40 m, `lod` 16 (~4 km), omitidas sobre las pistas.
  +17 KB y +127 objetos en el escenario de prueba; +2 % de coste de noche, 0 % de día.
* **Pendiente de medir tras la propuesta 1** (filas de luces): repetir `make prof ESCENA=3`; las luces de calle, de borde de pista y de
  aproximación son las que más se beneficiarían, porque van en línea recta con paso constante.

## Ronda 3: análisis del panel y de las luces, perfil y optimizaciones una a una (27 noviembre 2026)

Punto de partida (tras la pila y el montón en RAM alta, `fxmul` con vía corta y el panel/planificador de la ronda anterior). Para
comparar sin ruido todas las cifras de esta ronda se miden con la **física parada** (`PROFFLAGS="-DPROF_PAUSE=1 -DPROF_DUMPS=3"`): sin
ella la escena 1 cambia de un volcado a otro porque el avión avanza (9,9 → 6,1 s entre el primer y el cuarto volcado). Un volcado son
25 cuadros del bucle sin planificador (12,5 vistas); `ms por vista` = s / 12,5. El perfil se lanza de cuatro en cuatro:
`qcc` usa ficheros temporales comunes y no admite dos compilaciones a la vez, así que se compila en serie y se ejecuta en paralelo
(`BUILD_ONLY=1` y `RUN_ONLY=1` de `tools/ql_prof.sh`, igual que `tools/bench_sched.sh`; cada escena con su `PROFDIR=build/profN`, y el nombre debe
contener `prof` porque el reparto de la biblioteca de C lo busca en el mapa de enlace). Todo eso lo hace `tools/ql_prof_all.sh etiqueta 0 1 3 4`.

### Perfil de partida (física parada, volcado estacionario)

| Escena | s por volcado | ms por vista | Funciones que más pesan |
| --- | ---: | ---: | --- |
| 0 cabina en cabecera | 14,56 | 1.165 | `xform_obj3` 13 %, `poly_fast` 9 %, `gfx_fill_convex`+`fc_hspan` 11 %, `pj_xy`+`pj_div` 11 %, `view_sort_step` 5 % |
| 1 final a 5 km | 6,43 | 514 | `fc_hspan`+`gfx_fill_convex` 16 %, `gfx_line_raw` 8 %, `xform_obj3` 5 %, `gfx_memcpy` 5 %, `gather_tile` 4 % |
| 3 final nocturno | 9,09 | 727 | `xform_obj3` 21 %, `pj_xy`+`pj_div` 17 %, `draw_lights`+`dl_plot` 14 %, `view_sort_step` 5 % |
| 4 reactor, 12 km | 7,10 | 568 | `gfx_fill_convex`+`fc_hspan` 21 %, `gfx_line_raw` 7 %, `gfx_memcpy` 4 % |

(`ROM/QDOS`, 9 % en todas, es el teclado del bucle sin planificador; con el planificador se lee a ~10 filas/s y pesa mucho menos.)

Dónde está el tiempo, por orden: (1) **vértices**: en la escena de noche `xform_obj3` + `pj_xy` + `pj_div` + `draw_lights` + `dl_plot` son el
**51 %**, casi todo luces de pista, de aproximación y de calle de rodaje; en cabecera de día `xform_obj3` + `poly_fast` + proyección son el 35 %
(marcas de pista y calles); (2) el relleno, dominante en las escenas con mucho suelo grande (1 y 4); (3) ordenar objetos (`view_sort_step`, 5 %) y reunirlos
(`gather_tile`, 3-4 %); (4) volcado, 3-5 %.

### Punto 1: altímetro (`draw_alt`)

**Análisis.** Se midió con `-DPROF_ALT=100` (`src/prof.c`: 100 repintados con la altitud subiendo 10 pies cada vez, en tres rondas: desde
500 pies, desde 3.800 —la aguja de 10.000 pies cruza el texto— y desde 25.000). Antes: **21,5 a 22,2 ms por repintado** (unas 160.000 ciclos), la
mayor unidad del panel y más de un turno de 20 ms. Por función: `gfx_line_xor` 30 %; recorte de las agujas (`line_clip`, `outcode`,
`gfx_line_xor_s`, `xline`) 17 %; cálculo de extremos (`dpt`, `isin`/`icos`, `xneedle`) 11 %; texto (`gfx_text_raw`, `gfx_text`, `u_num`)
14 %; borrar el cuadro de cifras (`gfx_rect`, `gfx_hspan`, `fc_hspan`) 11 %.

**Qué se hizo** (`src/panel.c`, `src/util.c`):

* Las agujas van **sin recorte** (`gfx_line_xor` directa): caen siempre dentro de la esfera.
* Los extremos de las dos agujas se **guardan**: no se recalculan para borrarlas, y una aguja **solo se rehace si su extremo cambia de píxel**
  (la de 10.000 pies se mueve 0,36° por cada 10 pies, 0,09 píxeles: cambia una vez de cada once; se rehace siempre solo cuando cruza el texto
  —entre ~3.500 y ~6.500 pies—, porque el rectángulo de un dígito se llevaría sus puntos y el XOR de borrarla después dejaría restos).
  Orden obligado: borrar agujas que se rehacen → cambiar texto → pintar agujas.
* **Solo se repintan los dígitos que cambian** (con 10 pies de paso, casi siempre uno).
* Ángulos con `sdiv32` (dos `DIVU`) en vez de `%` y `/` de 32 bits, y extremos con `MULS16` (mismas constantes: `RX*k>>8` = 21 y 35, `RY*k>>8` = 14 y 24).
* `u_num` (lo usan todos los textos del panel) divide en 16 bits (`DIVU`) cuando el número cabe (antes 2 divisiones de 32 bits de la biblioteca por dígito).

**Comprobación.** `tmp/alttest.c` (compilado con el `panel.c` anterior y el nuevo en el PC) repinta 3.000 altitudes (1.500 en escalones suaves de ±10 pies y 1.500
saltos aleatorios entre −66 y 33.000 pies) y compara la zona del altímetro: **idéntica pixel a pixel**.

**Resultado** (SPEED 1, ms por repintado):

| Ronda | Antes | Ahora |
| --- | ---: | ---: |
| desde 500 pies | 21,5 | **11,4** (−47 %) |
| desde 3.800 pies (aguja de 10.000 pies sobre el texto) | 21,7 | **13,9** (−36 %) |

Lo que queda es `gfx_line_xor` (~50 %, unos 220 ciclos por píxel: la 68008 con la contención de la pantalla). Se descarta especializar su bucle (color
y sentido): ahorraría ~25 % de un 1 % del fotograma.

**Descartado: copia limpia en vez de borrar con XOR** (27 nov 2026, sQLux a SPEED 1, `-DPROF_ALT=100`, escena 0). Variante probada: en cada repintado,
`restore_box` de la esfera entera (73×51 píxeles, 1.020 bytes) desde `clean`, rectángulo negro + 2 cifras y las dos agujas. Resultado: **13,9 ms por
repintado contra 8,1 de la versión con XOR** (el volcado cuesta 3,6 ms, unos 3,5 µs por byte de pantalla, más que las 70 píxeles de línea que se ahorran:
`gfx_line_xor` pasa de 4,9 a 3,4 ms). Restaurar solo el rectángulo de la aguja de 1.000 pies no sale a cuenta: ese rectángulo siempre contiene el centro,
así que habría que rehacer también la aguja de 10.000 pies (y a veces el texto); el balance estimado es −0,4 ms (5 %) por una lógica mucho más delicada.
Borrar con XOR cuesta lo mismo que pintar y no necesita leer la copia. El texto tampoco es problema: `gfx_text` no usa la ROM (fuente propia `font5x7` y
`gfx_text_raw` en ensamblador) y en el altímetro solo se repinta el dígito que cambia (no sale entre las diez primeras funciones del perfil).

### Punto 2: luces de pista, aproximación, calle de rodaje y ciudad como filas (`OF_ROWS`)

**Análisis.** Cada luz era un vértice del escenario: `xform_obj3` la transformaba (~1.400 ciclos), `pj_xy` la proyectaba (~900) y `dl_plot` la
pintaba (~560). Pero casi todas las luces van **en línea recta con paso constante**: los dos bordes de cada pista (una luz cada 60 m), el umbral y
el final (11 luces), el eje y las dos barras de aproximación (15 + 3 + 3), las luces verdes de eje de calle (cada 40 m) y las calles de la
ciudad (una cada 240 m). Del generador (`tools/iberia.py`): 18.067 luces de calles de ciudad, 2.216 de borde de pista, 2.098 de calle de rodaje, 231 + 231
de umbral y final y 399 de aproximación; solo quedan sueltas las 14 de torres, las 380 rojas de edificios altos y los PAPI. Como la perspectiva
conserva las rectas, una fila basta con sus **dos extremos**: se transforman 2 vértices en vez de n, y las luces intermedias se obtienen
**sumando** un paso en coordenadas de cámara (X, Y, Z con 12 bits de fracción) y proyectando cada una.

**Qué se hizo**

* **Formato** (`tools/mkscen.py`, `src/world.h`): bandera `OF_ROWS` (0x80) en un objeto `OBJ_LIGHTS`; cada fila son 2 vértices (extremos) y una cara
  `[3, color, a, b, n]` (n luces equiespaciadas de `a` a `b`, hasta 255). `Scen.lrows` agrupa hasta 48 filas por objeto. Pistas, calles de rodaje (en tramos
  entre las partes que caen sobre una pista) y calles de ciudad se generan como filas; los PAPI, torres y luces rojas siguen como luces sueltas (mismo
  código de siempre). El `.scn` de Iberia pasa de **531 KB a 391 KB**, de 4.017 a 3.755 objetos y la tesela mayor de 30,8 a 18,1 KB (menos disco, menos
  carga de teselas, menos objetos que reunir y ordenar).
* **Dibujo** (`src/view3d.c`, `src/ql68k.s`): `xform_obj` transforma los vértices de las filas (sin el diezmado por distancia) y `draw_obj` llama a `draw_lrow` por cara.
  `draw_lrow` (ensamblador; en el PC la referencia en C con la misma aritmética) calcula el paso `d = (p1 − p0)·4096 / (n − 1)` con `sdiv32`, lo multiplica por
  `2^shift` (el diezmado por distancia de siempre: 1 luz de cada 2^U), descarta la fila si sus dos extremos están detrás del plano cercano y, por luz, interpola,
  descarta las de detrás del plano cercano, proyecta con `pj_xy` (nueva entrada `pj_xy_r` con x, y, z ya en registros) y pinta con `dl_plot` (un punto; dos si el objeto es cercano).
* **Comprobación.** `-DASM_TEST` (marca 107): `draw_lrow` contra la referencia en C sobre 300 filas aleatorias (extremos grandes, filas que cruzan el plano cercano,
  distintos `shift`, `dbl`, colores) → **0 píxeles distintos**. En el PC, la escena 3 (noche) con el escenario antiguo y el nuevo difiere en 219 de 32.768 bytes de pantalla
  (el redondeo de interpolar frente a transformar cada luz); las escenas de día, 0 diferencias.

**Resultado** (SPEED 1, física parada, s por volcado de 25 cuadros):

| Escena | Antes | Ahora | |
| --- | ---: | ---: | ---: |
| 3 final nocturno | 9,09 | **7,27** | −20 % |
| 1 final a 5 km | 6,43 | 6,38 | −1 % (menos objetos que reunir) |
| 0 cabina en cabecera | 14,56 | 14,51 | 0 |
| 4 reactor, 12 km | 7,10 | 7,05 | −1 % |

Escena 3 por etapas, antes → ahora: `xform_obj3` 1,87 → 0,60 s; `draw_lights` 0,80 → `draw_lrow` 1,27 s (incluye el interpolado); `pj_xy`+`pj_div` 1,5 → 1,03 s;
`dl_plot` 0,51 → 0,40 s; nuevo `sdiv32` 0,49 s (tres divisiones por fila).

**Coste medido de `draw_lrow`** (`-DPROF_LROW`, 200 filas, SPEED 1): ~1.950 ciclos de la propia rutina para una fila de 1 luz y unos 730 por luz más (cada instrucción del
68008 con el bus de 8 bits cuesta ~25 ciclos); proyección ~900 y `dl_plot` ~560 por luz. Antes una luz costaba ~3.300 ciclos; ahora ~2.200.

**Lo que queda aquí** (no aplicado): las tres divisiones por fila (`sdiv32` 6,7 % de la noche; en línea con signo y valor absoluto compartidos ahorrarían ~2 %); `dl_plot` recorta con
cuatro comparaciones (~560 ciclos; con el recorte de la ventana de la vista como constantes sin signo, ~350); y bajar `LIGHT_U0` / limitar luces por vista cambia lo que se ve.

### Punto 3: marcas de pista cercanas (objetos pequeños y alcance corto)

**Análisis.** En la escena 0 (cabina en cabecera, la peor) `xform_obj3`, `poly_fast`, `pj_xy` y `pj_div` suman el 35 %, y la lista de objetos dibujados (`-DOBJ_DUMP` en el PC) muestra
por qué: de ~800 vértices por vista, **400 son cinco objetos del eje discontinuo de la pista** (20 rayas = 80 vértices cada uno, `lod` 8 = 2 km) y 48 más las barras de umbral
(las dos cabeceras en un solo objeto de radio 2 km). Una raya mide 1,2 m de ancho: con `fx` = 343 ocupa menos de un píxel a partir de ~400 m, y `TINY_PX` ya
no la pinta (caja ≤ 1 píxel en los dos sentidos), pero **antes de descartarla** se transforma, se proyecta y pasa por `poly_fast`. Con objetos de 1,2 km de radio nunca se descartan enteros.

**Qué se hizo** (`tools/mkscen.py`): el eje se parte en objetos de `MARK_DASHES` = 5 rayas (20 vértices) y las barras de umbral en un objeto por cabecera, todos con `lod` =
`MARK_LOD` = 5 (1,3 km + su radio), de modo que `gather_tile` y `xform_pre` descartan los trozos lejanos enteros. Cambia solo qué se transforma; lo que se ve es lo mismo.
`build/iberia.scn`: 3.923 objetos, 394 KB.

**Comprobación (PC).** Escenas 0-4 con el escenario anterior y el nuevo: **0 bytes distintos**. Nueve posiciones sobre la pista (15 m a 300 m de altura, de 3 km antes de la
cabecera a 2 km después): 0-72 bytes de 32.768 distintos; no son rayas que desaparezcan, sino el redondeo de los vértices, que se cuantizan a 1/8 m respecto al origen de
cada objeto (los orígenes son otros), y mueven un borde un píxel.

**Resultado** (física parada, s por volcado): escena 0 **14,51 → ~11,7** (−19 %, descontada la ordenación del punto 4; por funciones, `xform_obj3` 1,90 → 1,09 s, `poly_fast` 1,31 → 0,97 s, `pj_xy` + `pj_div` 1,53 → 0,88 s,
`gfx_fill_convex` 0,94 → 0,85 s); escenas 1, 3 y 4 sin cambio apreciable (−0,01 a −0,06 s).

### Punto 4: ordenación de objetos (`view_sort_step`)

**Análisis.** La inserción cuesta entre 0,2 y 0,76 s por volcado (3-5 %); en la escena 0, ~1.000 desplazamientos por orden con ~75 objetos. El coste por desplazamiento era alto porque
**c68 resuelve la asignación de una estructura** (`items[j + 1] = items[j]`, 8 bytes) con una llamada a la biblioteca de ~400 ciclos.

**Qué se hizo** (`src/view3d.c`): se copian los dos campos a mano con punteros, sin cambiar el orden (inserción estable). `view_sort_step` conserva la interfaz por trozos de 4 elementos.
`view_sort_step` por volcado: escena 0 0,76 → 0,50 s (−34 %); escena 1 0,19 → 0,13 s; escena 4 0,14 → 0,09 s.

**Probado y descartado:** ordenación por cubetas (radix LSD de tres pasadas de 6 bits sobre una clave de 18 bits, *exactamente* el mismo orden: comprobado imagen a imagen en el PC
en 5 escenas y 6 posiciones). Con 70-130 objetos gasta **más** que la inserción: en c68 cada pasada por elemento cuesta ~200 ciclos y la lista llega casi ordenada
(por volcado, radix contra inserción con punteros: escena 0 0,37 contra 0,50 s; escena 1 0,24 contra 0,13; escena 4 0,22 contra 0,09). Se queda la inserción.

### Resumen de la ronda 3

Ciclos del 68008 por volcado de 25 cuadros del bucle sin planificador (SPEED 1, física parada); ms por vista = s / 12,5.

| Escena | Antes (s) | Ahora (s) | Antes (ms/vista) | Ahora (ms/vista) | |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 cabina en cabecera | 14,56 | 11,43 | 1.165 | 914 | **−21 %** |
| 1 final a 5 km | 6,43 | 6,32 | 514 | 506 | −2 % |
| 3 final nocturno | 9,09 | 7,26 | 727 | 581 | **−20 %** |
| 4 reactor, 12 km | 7,10 | 7,02 | 568 | 562 | −1 % |

Cambios: altímetro (22 → 11-14 ms por repintado, sin efecto en el volcado de escenas fijas pero sí en cada turno de panel), luces como filas (noche −20 %, escenario −26 %),
marcas de pista con alcance corto (cabecera −19 %), inserción sin copia de estructuras (−0,05 a −0,26 s).

**Simulador entero con el planificador** (`ND=1 PF="-DUSE_SCHED -DPROF_SCHED_RUN=1500" tools/ql_prof_all.sh sc 0 1 3 4`: 30 s de reloj con la física en marcha):

| Escena | Física (pasos/s) | Vistas completas por segundo | Teclado (filas/s) |
| --- | ---: | ---: | ---: |
| 0 cabina en cabecera | 12,50 | **0,83** (antes 0,63) | 10,3 |
| 1 final a 5 km | 12,50 | **1,20** (antes ~1,15) | 9,9 |
| 3 final nocturno | 12,52 | 1,27 | 6,4 |
| 4 reactor, 12 km | 12,52 | 1,33 | 9,1 |

(Las cifras «antes» son las de la ronda anterior; las escenas 3 y 4 no se midieron con planificador entonces.)

**Pendiente, por orden de rentabilidad**

1. Sigue mandando el número de vértices y de objetos: en la escena 0 todavía `xform_obj3` 9,5 %, `poly_fast` 9 %, proyección 8 %; calles de rodaje, plataformas y cuadros de suelo a 2-5 km (`lod` 20 = 5 km; un cuadro de 20 m a 3 km mide 2 píxeles).
2. De noche: `draw_lrow` 17,5 % + proyección 14 % + `sdiv32` 6,7 % + `dl_plot` 5,6 %. Margen conocido: divisiones de la fila en línea (~2 %), `dl_plot` con el recorte de la vista como
   constantes (~3 %), y limitar luces por vista / `LIGHT_U0` (cambia lo que se ve).
3. Relleno (`fc_hspan` + `gfx_fill_convex`) 11-21 % en las escenas 1 y 4: ya va por palabras largas; solo bajaría con menos solapes de suelo.
4. `gfx_line_raw` 7-8 % en las escenas 1 y 4 (~220 ciclos por píxel): especializar el bucle por color y sentido (~25-35 % de esa cifra).
5. Reunir y ordenar: `gather_tile` 3-4 % y `view_sort_step` 1-4 %: reutilizar la lista entre vistas (riesgo de «saltos» al girar).
6. Volcado (`gfx_memcpy`, 3-5 %): solo filas modificadas (hay que comparar con lo ya volcado, que cuesta casi tanto como copiar).

### Punto 5: `gfx_line_raw` especializada por color y sentido

**Qué se hizo** (`src/ql68k.s`): el bucle de Bresenham pasa de 21 instrucciones por píxel a ~13. Se despacha por tabla a uno de 8 bucles (4 colores × 2 sentidos en x) generados con
un guion: el píxel se pone con `OR` y se borra con `OR` + `EOR` (sin `btst`/`beq` por plano), y la máscara de un solo bit gira con `ROR.B`/`ROL.B`, cuyo acarreo avisa del cambio de byte (adiós a
`tst.b` + `bne` + comparar con cero). Los pasos de error son los mismos, así que los píxeles son los de siempre.

**Comprobación.** `-DASM_TEST` (marca 108): 600 líneas aleatorias (los 4 colores, verticales, horizontales, ambos sentidos) contra la definición en C sobre una pantalla con fondo no uniforme: **0 bytes distintos**.

**Resultado** (física parada, por volcado): `gfx_line_raw` escena 1 0,517 → **0,368 s** (−29 %), escena 4 0,490 → **0,351 s** (−28 %); fotograma completo escena 1 6,32 → 6,16 s (−2,5 %), escena 4 7,02 → 6,87 s (−2,1 %).
`gfx_line_xor` (agujas del panel) tiene el mismo bucle sin especializar; no se ha tocado (≈1 % del fotograma).

## Análisis: ¿interesa rehacer el escenario y añadir LOD a los objetos?

**Qué hay ahora.** El único LOD es *de alcance*: cada objeto tiene un `lod` (distancia máxima, en 256 m) y, según su escala `U`, se diezman las luces y se quitan los contornos. No hay
versiones simplificadas: un objeto cuesta lo mismo a 500 m que en el límite de su alcance.

**Coste por elemento (escena 0, 68008).** Fijo por objeto ≈ 7-8 mil ciclos (`xform_pre` ~2,6 k, `draw_obj` ~1,6 k, reunir y ordenar ~3 k) y por vértice ≈ 3 mil (`xform_obj3` 1,4 k + proyección 0,9 k +
`poly_fast`/recorte); un cuadro de 4 vértices cuesta ~20 k ciclos aunque ocupe tres píxeles. El relleno de la vista (`gfx_hspan`/`view_sky`) son ~35 k de los ~46 k píxeles por vista;
lo que pintan los objetos de suelo y montaña son 13-17 k píxeles en las escenas 1 y 4 y ~38 k en cabecera.

**Qué se dibuja** (PC, `-DOBJ_DUMP`, una vista; tras los puntos anteriores):

| Escena | Suelo de 4 vértices | Suelo > 4 vértices | Sólidos cóncavos (montañas, manzanas) | Líneas | Luces |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 cabecera | 38 obj / 152 v | 18 obj / 384 v | 20 obj / 334 v / 242 caras | 16 obj | 10 obj |
| 1 final a 5 km | 12 / 48 | — | 14 / 140 / 116 | 22 | 12 |
| 4 reactor | 6 / 24 | — | 12 / 108 / 96 | 18 | 12 |

**Conclusiones**

1. **Las escenas 1 y 4 no mejoran con LOD**: tienen ~280 vértices y 60 objetos. Lo que cuesta es relleno por filas (~18 ciclos por píxel, dominado por el preámbulo de cada fila de cada polígono grande) y
   líneas largas (carreteras y ríos de 10 km de radio). Menos vértices no lo tocan. Lo único útil ahí es reducir solapes de suelo (menos polígonos grandes superpuestos) o recortar líneas lejanas.
2. **La cabecera (escena 0) y la noche sí**: ahí mandan vértices y objetos. Hay tres fuentes de ahorro razonables, de mayor a menor rentabilidad estimada (sobre el fotograma de la escena 0):
   * **LOD por vértices en sólidos cóncavos (manzanas urbanas y montañas)** — ~104 vértices de manzanas a 3,6-5 km. Versión lejana = solo los techos (los vértices altos primero en la lista, una `nv_lejos` y
     un `nf_lejos` en la cabecera del objeto; se transforman solo esos vértices y se pintan solo esas caras), a partir de ~3 km. Cabe en el formato actual (un byte más por objeto o un bit
     en `flags` y caras ordenadas techo-primero). Ahorro estimado 3-4 % en cabecera.
   * **Fusionar suelo pequeño del mismo aeropuerto** (38 cuadros de 4 vértices: calles de rodaje, plataformas) en unos 8 objetos por zona: quita ~30 × 7,5 mil ciclos ≈ **3 %** de la cabecera, a costa de
     perder el descarte fino (el objeto fusionado se transforma entero) → conviene fusionar solo los que están a menos de 300 m entre sí.
   * **Descarte por tamaño proyectado** para suelo (hoy solo para sólidos: `MIN_PX`): un cuadro con radio · `fx` / z < ~2 píxeles no se dibuja. En el escenario actual ningún cuadro de suelo cae
     por debajo (radio ≥ 76 m a ≤ 5 km = ≥ 5 píxeles), así que no aporta hasta que haya más detalle.
3. **Rehacer el escenario entero no compensa**: los tres cambios son de generador (`mkscen.py`, como ya se hizo con luces y marcas) más una cabecera de LOD, no una reescritura. Lo que sí tendría
   sentido es añadir un **presupuesto por tesela** en `mkscen.py` (aviso si una tesela supera N vértices de suelo o N objetos), porque el límite real es `MAXITEMS` = 256 y la
   ordenación y reunión son O(objetos).
4. **Lo que no hay que hacer**: LOD discreto de las montañas (9 vértices, 8 caras: ya son lo mínimo; su coste es el relleno de triángulos grandes, que ninguna versión simplificada reduce) ni LOD de edificios
   pequeños (`SOLID_LOD` 5/8 y `MIN_PX` ya los descartan lejos).

**Recomendación:** hacer, por este orden, (a) LOD «solo techos» para manzanas y torres lejanas, (b) fusión local de suelo pequeño de aeropuerto, y dejar el presupuesto por tesela como comprobación del generador.
Ganancia conjunta esperada: ~6-7 % en cabecera, ~0 en las demás escenas; no cambia lo que se ve más allá de que las paredes lejanas no se dibujan.

### Punto 6: manzanas y torres lejanas «solo techos», y fusión de suelo pequeño

**LOD de solo techos** (`tools/mkscen.py`, `src/world.c`, `src/world.h`, `src/view3d.c`). La cabecera de cada objeto pasa de 20 a 22 bytes: `nvf` y `nff` (vértices y caras de la versión lejana; 0 = no hay). `Obj.pack`
reordena los sólidos que tienen techos (`box(..., roof=True)`: edificios, hangares, torres y las manzanas de ciudad): **los techos van los primeros en la lista de caras y sus vértices los primeros
en la de vértices**, de modo que la versión lejana es un prefijo. `xform_obj` decide por la distancia (`ROOF_DIST` = 3.000 m, máximo de |dx|, |dy| al origen del objeto): lejos transforma solo `nvf`
vértices y `draw_obj`/`draw_sorted` pintan solo `nff` caras (`obj_nf`). Una manzana de 10 edificios pasa de 80 vértices y 50 caras a 40 y 10; una torre, de 8 y 5 a 4 y 1. La colisión
(`world_hit`) usa todos los vértices y no cambia. `tools/mapa_scn.py` lee el formato nuevo. Hay que regenerar los escenarios.

**Fusión de suelo** (`Scen.merge_ground`, al escribir). Objetos de suelo pequeños (radio ≤ 400 m, ≤ 16 vértices, sin agua) de la misma tesela, mismo `lod` y misma altura, a ≤ 400 m del primero del grupo, se
unen en un solo objeto (hasta 80 vértices). **La forma no cambia**: las caras conservan el orden de pintado de origen (radio del objeto, de mayor a menor) y no se fusiona un grupo si otro
suelo cercano, de otro relleno, tiene un radio entre el de sus miembros y el del grupo (su orden relativo cambiaría). Resultado en Iberia: solo **21 objetos → 10**: de los 145 cuadros pequeños, la mayoría están aislados
a más de 400 m de otro del mismo `lod`, y separar más los grupos los hace chocar con esa regla (con 900 m salían menos fusiones, no más). Escenario: 3.912 objetos, 402 KB (+8 KB por la cabecera).

**Comprobación (PC).** Escenas 0-4 con el escenario anterior y el nuevo: 0 bytes distintos solo con la fusión; con la fusión **y** los techos, 9 y 4 bytes (escenas 0 y 1) y 0 en las demás.
En nueve posiciones de la pista y del entorno: la fusión cambia 0-2 bytes (redondeo de vértices); los techos 0-50 bytes de 32.768 (rendijas de pared de edificios a > 3 km, p. ej. a 1,5 km de la cabecera a 200 m de altura: 50 bytes en 8 filas).
`hosttest`, `iberiatest` y `figuras` pasan.

**Resultado** (física parada, s por volcado):

| Escena | Antes | Ahora | |
| --- | ---: | ---: | ---: |
| 0 cabina en cabecera | 11,43 | **10,40** | −9 % (≈ −0,12 s de ellos por el punto 5) |
| 1 final a 5 km | 6,16 | 6,07 | −1,5 % |
| 3 final nocturno | 7,26 | 7,27 | 0 |
| 4 reactor, 12 km | 6,87 | 6,88 | 0 |

Escena 0 por funciones: `poly_fast` 0,97 → 0,70 s, `xform_obj3` 1,09 → 0,99, `pj_xy` 0,52 → 0,47. Lo estimado era 6-7 % en cabecera: los techos aportan lo previsto (~7 %); la fusión, casi nada (−11 objetos de ~110), porque el suelo pequeño del escenario está disperso.

## Horizonte artificial y rosa del rumbo: dónde se iba el tiempo y qué se ha hecho

Medido en sQLux (rama `profile`, `IPC_ROM = Hermes2_20_rom`, 7,5 MHz con contención del ZX8301), escena 1, con el avión maniobrando
(`BENCH_MODE=7` y `8`, ver `tools/ql_bench.sh`). Todo lo nuevo da **los mismos píxeles** que lo anterior salvo lo indicado.

### Resultado

| | antes | ahora |
| --- | ---: | ---: |
| Horizonte, un repintado (alabeo de 3º por actualización) | 158 ms (110 con `AI_INCR`) | **~28 ms** (filas 24 + avión 1,4 + decidir 2) |
| Rosa del rumbo, un grado de cambio | 95 ms | **17 ms** |
| Vuelta completa del panel con todo cambiando | 260 ms | **120 ms** |
| FPS del panel en los turnos libres (física en la interrupción, `FKQQ…`) | 1,9 | **3,25** |
| Panel en un turno de cada 8 / en uno de cada 4 | 1,0 / 1,45 | **1,05 / 1,95** |

No llega a 6,25 FPS, pero la vuelta completa en el peor caso ya baja de 120 ms y la mayor unidad indivisible es la altitud (29 ms); el horizonte
y la rosa dejan de ser el problema. Quedan, por orden: altitud 29 ms, horizonte 28, radios 18, rosa 17, motor 11, mensajes 5.

### Horizonte artificial

Dónde estaban los 158 ms: filas 33 ms (una división de 32 bits y dos productos por fila), **aro 64 ms** (dos elipses de 32 segmentos cada
una, con seno y coseno) y avión de referencia 7 ms (seis líneas y un rectángulo), todo en cada cambio de un grado.

1. **Una división por cuadro** (`AI_INCR`): el borde cielo/tierra es una recta; su posición en cada fila sale por sumas en Q8. Cambia hasta
   un píxel en el borde frente a la división exacta.
2. **Solo se repintan los píxeles que cambian** (`AI_DIFF`, por omisión con `AI_INCR`): por cada fila se recuerda qué color hay a la izquierda y
   dónde empieza el de la derecha. Al cambiar la actitud solo se pinta el tramo entre la frontera vieja y la nueva (una `gfx_hspan`), o la fila
   entera si cambia el color de la izquierda; una fila que no cambia no cuesta nada. Si el avión sube o baja, solo se repintan las filas entre el horizonte viejo y el nuevo.
3. **El aro ya no se dibuja** (63,9 ms → 0): el tramo libre de cada fila se calcula de la copia limpia del panel (desde el centro hasta el primer
   punto no negro), así que el relleno no llega a pisar el aro. Antes se repintaba porque `hw_tab` lo pisaba en las filas de arriba y de abajo.
4. **El avión de referencia es una máscara** (5 filas × 7 grupos, calculada una vez dibujando las líneas): 7,4 → 1,4 ms, y solo se estampa
   si ha cambiado alguna de las filas 129–133.

Comprobado en PC con 3.000 actitudes aleatorias (saltos de ±30º, cabeceos, giro completo): píxel a píxel **idéntico** a repintar todo con
`AI_INCR` más aro y avión.

Lo que no se hizo: ensamblador para el horizonte. Con el diferencial ya solo se pintan las filas que cambian y el coste está en las llamadas a
`gfx_hspan` (que ya es ensamblador); el resto del bucle son sumas.

### Rosa del rumbo

95 ms = copia del fondo 20 + doce rayitas largas 35 + doce números 36 + rumbo 3. Cambios:

1. **`gfx_memcpy` para menos de 48 bytes** (`ql68k.s`): la versión anterior guardaba 11 registros con `MOVEM` en cada llamada y copiaba 24 bytes
   con un bucle de 6 vueltas; la copia de la rosa (57 filas de 24 bytes) tardaba 20 ms. Ahora la vía corta no salva registros.
2. **`gfx_blit`** (nuevo, ensamblador): copia `n` filas de `m` bytes con pasos distintos sin llamar a `gfx_memcpy` por fila. La copia de la rosa
   pasa de 20 a **5,5 ms** (el suelo es la pantalla: ~30 ciclos por byte). Lo usan también `gfx_copy_box` (todos los `restore_box`) y los instrumentos.
3. **30 copias del fondo en vez de 10** (`GYN`): el patrón de rayitas largas se repite cada 30º, así que van en la copia: 35 → 0,14 ms. Cuestan
   41 KB de RAM alta (13 KB antes) y algo más de tiempo al arrancar el panel (30 copias en vez de 10; sin medir).
4. **Posiciones de los números por tabla**: los 12 sitios se repiten cada 30º (solo cambia qué número hay en cada sitio); se calculan una vez
   (`lab_xy[30][12]`, 1,4 KB) y desaparecen `dpt`, los senos y los productos de 32 bits de cada número.
5. **Letras preparadas** (idea de los bitmaps por dígito): `glyph_make` deja cada letra en las 8 posiciones dentro del grupo de 8 píxeles, con
   «conservar» y «poner» ya calculados; `gfx_glyph` (ensamblador) las pinta con `and`/`or` sin desplazamientos ni máscaras. Son 11 letras de la rosa
   (cifras 0–6 blancas y N E S W verdes) más las cifras rojas del rumbo en sus 3 posiciones: 5,6 KB + 2,5 KB.
6. El rumbo se separa en dígitos con restas, no con `u_num` (dos divisiones de la biblioteca).

Resultado: copia 5,5 + rayitas 0,1 + números 9,4 + rumbo 1,7 = **17 ms**.

#### ¿Tiene sentido un bitmap por dígito en lugar de imprimir texto?

Parcialmente. Medido con `gfx_glyph` solo (sin código C alrededor): 18 letras de dos grupos tardan 6,2 ms y de un grupo 4,3 ms, es decir
0,24–0,35 ms por letra, frente a ~0,6–0,9 ms con `gfx_text_raw`. Gana una **2×–3×** en la letra, y otro tanto en C que ya no hay que calcular
(el cuello eran los productos de 32 bits de la biblioteca en el cálculo de índices y posiciones: cambiarlos por tabla y desplazamientos
bajó los números de 16 a 10 ms). Pero el suelo no es el cálculo: es el **acceso a la pantalla**, que cuesta unos 30–45 ciclos por byte
leído o escrito. Una letra de 7 filas toca 56 bytes de pantalla (lectura y escritura de palabras), y eso son ~2.000 ciclos que ningún
bitmap evita mientras la letra se mezcle con lo que hay detrás (fondo transparente).

Dos intentos de evitarlo no funcionaron:

* **Componer la rosa en RAM alta y volcarla** (los accesos del texto pasan a memoria sin contención y la pantalla solo recibe una copia):
  20,7 ms contra 17 ms. La copia de entrada (hi → hi) y la de salida (hi → pantalla) cuestan lo que se ahorra.
* Un mapa de bits opaco por cada rumbo (360 imágenes de la rosa entera = 480 KB): no cabe.

Lo que sí sería general: `gfx_glyph` podría usarse para el texto de **todos** los instrumentos (cada cadena fija, cada cifra), con tablas de
letras por tinta (blanco, verde, rojo, ~14 KB por tinta para el alfabeto de 64 caracteres). Quita un 2×–3× a los 18 ms de radios, los 11 del motor
y los mensajes; no se ha hecho por no estar pedido.

### Qué quedó descartado o pendiente

* Rayitas cortas en las copias: ya lo estaban (10 copias antes; ahora 30 con las largas).
* La **altitud** (29 ms, ahora la mayor unidad del panel) son cuatro agujas de OR exclusivo (borrar y pintar, 8 líneas con recorte) y una cifra de 5 dígitos:
  mismo tratamiento posible (máscaras de aguja por ángulo, 16 ángulos × 2 agujas).
* El texto de radios (18 ms) y motor (11 ms) se rehace entero para compararlo: comparar los valores antes de formar las cadenas lo deja casi a cero cuando no hay cambios.

### Qué cambia en el código

* `src/ql68k.s`: `gfx_memcpy` con vía corta, `gfx_blit`, `gfx_glyph`. `src/gfx.c/.h`: sus versiones en C para el PC.
* `src/panel.c`: `ai_begin/ai_rows/ai_plane/ai_end` (con `AI_INCR`, `AI_DIFF`), `ai_limits`, rosa con tablas y letras preparadas, `GYN = 30`.
  `AI_INCR` y `AI_DIFF` van activos con `USE_SCHED` (`qfs.h`); `-DAI_NODIFF` los separa. Sin `AI_INCR` el horizonte es el de siempre.
* Diferencias de píxeles frente al código anterior (solo con `AI_INCR`): hasta un píxel en el borde cielo/tierra; y en la rosa, donde un número
  verde cae sobre una rayita blanca el número gana (antes ganaba la rayita porque se dibujaba después).
* `src/bench.c` modo 8 (partes sueltas del horizonte y de la rosa) y modo 7 (unidades del panel).

## Planificador por turnos (estudio de interrupciones): física, panel y vista 3D repartidos por turnos de la interrupción de 50 Hz

Pregunta: ¿se puede enganchar la física a una de cada 4 interrupciones (12,5 Hz estables, `F N N N F N N N…`), el panel a una de cada 8
(6,25 FPS, desplazado `F+2`, quizá partido en dos) y una vista 3D solo de alambre en 4 trozos (3,125 FPS), saltando el turno cuando la
tarea anterior no ha acabado?

**Respuesta corta** (todo medido en sQLux a 7,5 MHz con la contención del ZX8301, escena 1 = final a 5 km sobre Madrid, monomotor):

| Objetivo | Resultado |
| --- | --- |
| Física estable a 12,5 Hz, independiente del resto | **Sí, pero solo si corre dentro de la interrupción** (20 líneas de ensamblador). En todas las pruebas con la física en la interrupción salió **12,50 Hz** aunque el panel o la vista acapararan la CPU. Como turno cooperativo (sin interrupción) cae a 9,3–11,2 Hz en cuanto hay panel o 3D. Hay que sacar de ella los motores y la navegación. |
| Física sola, sin panel ni 3D, con teclado, a toda marcha | **41 pasos/s** (55 sin motores ni navegación): a 12,5 Hz usa el 25–30 % de la CPU. |
| Panel a 6,25 FPS estables | **No.** Una vuelta completa con todo cambiando cuesta **260 ms** (6,25 FPS pedirían 1,6 s de CPU por segundo). Con los turnos libres: 1,0 FPS (un turno cada 8), 1,5 (cada 4) y 2,0–2,3 FPS como máximo. |
| Vista 3D de alambre a 3,125 FPS | **No.** El alambre **no es más barato** que el sólido (487 ms contra 499 ms por vista) y un cuadro necesita ~24 turnos. Salen **0,6 FPS** (un trozo por periodo) y 0,85 FPS con todos los turnos libres. |
| Las dos cosas a la vez | Panel 1,4 FPS + vista 0,2 FPS, o panel 0,65 + vista 0,55; la física intacta a 12,5 Hz con la interrupción. |

El mecanismo funciona; lo que no cabe es el contenido. Los números que faltan y lo que habría que recortar están al final.

### 1. Peso del modelo físico y del 3D

Tamaños del enlace actual (`qfs.MAP`: texto, datos, BSS, en bytes). El ejecutable entero ocupa 80.888 bytes (79.162 antes de este estudio;
los 1,7 KB de más son el panel y la vista reanudables y la partición de la física).

| Parte | Código | Datos / tablas | RAM de trabajo |
| --- | ---: | ---: | --- |
| **Modelo físico** (`flight` 5.520, `aircraft` 3.186, `nav` 1.560, `atc` 2.666, `fixmath` 528) | 13.460 | 3.146 (tablas de seno, atan y atmósfera) | `Flight` 328 + `AircraftDef` 240 + `Radios` 80 + `Atc` 240 + BSS de `aircraft` 1.728: **< 2,7 KB** |
| `fxmul`/`fxdiv`/`isqrt` en ensamblador | ~0,7 KB de los 4.454 de `ql68k.s` | | |
| **3D** (`view3d` 6.814, `gfx` 2.258, `world` 4.830) | 13.902 + ~3,7 KB de `ql68k.s` = **~17,6 KB** | | BSS de `view3d` 9.004 + `gfx` 1.280 = 10,3 KB; buffer de vista 12.288 (96 × 128); **caché de teselas: 56 KB** en Madrid (30 teselas; la mayor mide 30 KB; tope 49) |
| Panel (`panel` 10.488) | 10.488 | | 742 de BSS + fondo de 20 KB (`.src`) + copia limpia de 20 KB en RAM alta |

El código de la física y el del 3D pesan casi lo mismo (~17 KB), pero la física casi no usa RAM (< 3 KB) y el 3D arrastra ~80 KB (buffer, BSS y caché de
teselas). La caché de teselas es además el único dato que comparten la física (`world_ground`, `world_hit`) y la vista (ver 5). Los ~0,7 KB de ensamblador
de la física son una estimación.

### 2. Cuánto cuesta cada tarea (`BENCH_MODE=1,5,6,7`, ciclos reales del 68008)

| Tarea | ms |
| --- | ---: |
| `controls_step` + `flight_step` (sin motores) | 14,3 (12,9 – 20,5) |
| `flight_engines` (cada 4 pasos en el código actual) | 7,8 |
| `nav_update` + `atc_update` (cada 12 pasos de 1/25 s) | **27** (eran **78** antes de rechazar balizas por caja, ver 6) |
| Teclado: una fila de `KEYROW` / tres filas / ocho | 0,5 / 2,0 / 4,9 con el IPC de alto nivel; **1,45 / 5,2 / 12,9 con `IPC_ROM = Hermes2_20_rom`** (ver 7) |
| `controls_frame` / `snd_update` / `build_status` | 1,3 / 2,8 / 6,3 |
| Panel, unidades con todo cambiando (avión alabeando, altitud, rumbo…) | horizonte 110 (filas 37 + avión y aro **70** + decidir 3), **rumbo 84**, altitud 28, radios 18, motor 9, mensajes 5, resto ~4 → **260 vuelta completa** |
| El mismo panel sin que cambie nada | 43 (se rehacen las cadenas de radios y motor para compararlas) |
| Vista sólida completa / de alambre completa | 499 / 487 |
| Vista, por unidades de trabajo (alambre) | preparar cámara y teselas **73**, reunir una tesela 1,3 (máx. 13), ordenar 4 elementos 2,2, **dibujar un objeto 13 (máx. 41)**, volcar 16 filas 4,9 |

Un turno dura 20 ms, de los que QDOS se queda ~1,3 ms en su propia interrupción (se ve en el mínimo y el máximo de cada medida). Ninguna unidad del panel (84, 70, 28 ms) ni de la vista (73, 41 ms) cabe en un
turno; la física sin motores (14 ms) sí.

### 3. La frecuencia de 12,5 Hz y la dinámica

`PHYS_DT` ahora se puede cambiar (`-DPHYS_DT=5243L -DPHYS_HZ=12`). `tests/dtstudy.c` repite las mismas maniobras con 1/25 s y con 1/12,5 s:

| Prueba | 1/25 s | 1/12,5 s |
| --- | ---: | ---: |
| Despegue (s / m de carrera) | 17,24 / 320 | 17,12 / 321 |
| Subida 40 s: IAS (kt), altitud (m), VS (fpm) | 83,2 / 791 / 1287 | 83,0 / 791 / 1284 |
| Pulso de profundidad: cabeceo máximo (º) | 8,13 | 7,92 |
| Alabeo tras 1,5 s de alerones (º) | 31,64 | 31,77 |
| Pérdida (s hasta entrar, IAS en kt) | 69,5 / 47,2 | 68,9 / 47,4 |
| Aterrizaje desde 3 km | 116,26 s, sin accidente | 116,32 s, sin accidente |
| Reactor a 130 m/s (181 kt IAS), mandos a ±60 % cada 0,4 s: velocidad de cabeceo máx. (º/s) | 5,88 | 6,29 |

Con la dinámica actual el paso de 0,08 s es estable (el peor caso para un Euler, el reactor con presión dinámica máxima, queda ~7 % por
encima y amortigua igual) y cuesta la mitad de CPU. Hay que cambiar el reloj de la simulación (`PHYS_HZ` entero se usa para contar segundos) y
`nav` (12 pasos pasan a 6), cosas que el estudio no toca.

### 4. Experimentos de planificación (`BENCH_MODE=10`, 1000 turnos = 20 s cada uno)

Patrón de 32 turnos con `E` (motores) en los turnos 3 y 19 y `Y` (navegación + ATC) en el 6, de las letras: `F` física, `K` una fila de teclado
y controles, `Q` panel reanudable (trabaja hasta el final del turno y sigue en el siguiente), `V` vista de alambre reanudable, `N` nada.
«Coop» = la física es un turno más (no interrumpe a nadie); «ISR» = la física se llama desde el sondeo de 50 Hz (`tm_poll`) en los turnos
0 de cada 4. Un turno cuyo hueco pasó mientras la tarea anterior seguía se **salta** (se cuenta). Banderas: `-DAI_INCR -DENG_EXTERNAL -DB_NAVX=1`.

| Patrón base | | CPU | F real (Hz) | K (filas/s) | **Panel FPS** | **Vista FPS** | Turnos saltados (K / Q / V) |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `FKNN FKNN` (solo física y teclado) | coop | 28 % | 12,47 | 12,5 | – | – | 0 / – / – |
| | **ISR** | 25 % | **12,53** | 12,5 | – | – | 0 |
| `FKQN FKNN` (panel en F+2 cada 8, el planteado) | coop | 53 % | **10,82** | 10,9 | 1,00 | – | 33 / 0 / – |
| | ISR | 52 % | **12,53** | 10,9 | 1,00 | – | 33 |
| `FKQN FKQN` (panel en F+2 cada 4, «dos mitades») | coop | 66 % | **10,12** | 10,2 | 1,55 | – | 47 / 1 / – |
| | ISR | 65 % | **12,49** | 10,1 | 1,45 | – | 48 / 24 |
| `FKQQ FKQQ` (panel en todo lo libre) | coop | 78 % | **9,29** | 9,8 | 2,30 | – | 54 / 57 / – |
| | ISR | 77 % | **12,51** | 9,4 | 1,95 | – | 62 / 88 |
| `FVNN FVNN` (alambre, un trozo por periodo, el planteado) | coop | 60 % | 12,47 | – | – | **0,60** | – / – / 0 |
| | ISR | 56 % | 12,53 | – | – | **0,60** | |
| `FKVV FKVV` (alambre en todo lo libre) | coop | 73 % | **10,87** | 12,3 | – | 0,85 | 4 / – / 18 |
| | ISR | 70 % | 12,52 | 11,3 | – | 0,80 | 24 / – / 21 |
| `FKQV FKQV` (panel y alambre a medias) | coop | 78 % | **9,36** | 10,1 | 1,55 | 0,25 | 49 / 5 / 33 |
| | ISR | 75 % | 12,52 | 9,8 | 1,40 | 0,20 | 54 / 25 / 46 |
| `FVQN FVNN` (F N P N con trozos de 3D) | coop | 75 % | **11,22** | – | 0,65 | 0,55 | |
| | ISR | 73 % | 12,53 | – | 0,65 | 0,55 | |

* **La física en la interrupción no se entera de lo que haga el resto** (12,49–12,53 Hz en los ocho patrones, con la relación de reloj en
  1,002). Como turno cooperativo la pierden los patrones con panel o 3D, porque ninguna unidad cabe en 20 ms y el turno `F` que llega mientras
  tanto se salta.
* **El salto de turno funciona** (`K` pierde del 13 al 25 % de sus turnos cuando el panel se pasa de su hueco) pero lo que garantiza es que no se
  acumule retraso, no que las tareas largas salgan: el panel y la vista tardan 12–25 turnos en acabar un cuadro por mucho que se parta.
* La CPU total queda en 25 % (física, motores, navegación y teclado) + el reparto del resto. No sobra para 6,25 + 3,125 FPS.
* Física dentro de la interrupción, motores y navegación **dentro** de ella (variante ingenua, medida antes): la relación de reloj baja a 0,962
  (se pierde el 3,8 % de las interrupciones porque el paso dura más de 20 ms). Con motores y navegación fuera, el paso de la interrupción dura
  **10,2 ms de media y 15,8 de máximo** (`F en la interrupción`, 250 pasos): cabe, con 4 ms de margen.

### 5. Qué hay que hacer para que sea real

1. **Física en la interrupción** (hecho en `qdos68k.s`: `tm_poll` llama a `plat_isr_fn` con todos los registros salvados y pila propia
   `plat_isr_sp`; coste con el enganche vacío, un `TST.L` y un `BEQ` por interrupción). Dentro solo `controls_step` + `flight_step` con
   `-DENG_EXTERNAL`; motores (7,8 ms) y navegación + ATC (27 ms) pasan a tareas normales (`flight_engines`, letras `E` e `Y`).
2. **Compartir datos**: la interrupción escribe `sim.f` mientras el panel y la vista lo leen (solo se ve un instrumento con el dato un paso
   atrasado). Lo peligroso es la **caché de teselas**: `world_ground` y `world_hit` leen teselas y la lista de colisiones que
   `world_tiles_near` reorganiza al cambiar de tesela (~13 ms por tesela cargada). Hay que enmascarar interrupciones (`SR |= $0700`) durante
   esa carga; el retraso (< 1 turno de la física) es aceptable. En las pruebas de 20 s no hubo cambios de tesela, así que **esta carrera no
   está probada**.
3. **Trabajo reanudable en vez de turnos fijos**: `sim_vjob_step` (vista) y `panel_job_step` (panel) trabajan hasta que cambia el contador de
   50 Hz y siguen en el turno siguiente. Es más fino que «cuatro trozos»; aun así hay unidades que no caben en 20 ms.
4. **Para llegar de verdad a 6,25 FPS de panel** habría que bajar la vuelta completa de 260 ms a unos 40 ms (el 25 % de la CPU): sacar el aro
   y el avión de referencia del horizonte de cada repintado (70 ms), pintar la rosa del rumbo (84 ms) por diferencias o solo al cambiar un
   grado, no rehacer las cadenas de radios y motor (43 ms constantes) y refrescar cada instrumento a su ritmo (rumbo y horizonte rápidos;
   radios y motor a 1 Hz). Con eso el panel ocuparía ~25 % de la CPU y a la vista le quedaría otro ~50 %: 160 ms por cuadro a 3,125 FPS.
5. **Para llegar a 3,125 FPS de alambre** un cuadro tiene que costar ≲ 150 ms (hoy ~490): 3–4 veces menos trabajo (alcance de 5–8 km,
   sin suelo, sin ciudades), preparar la lista de objetos solo al moverse (73 ms), líneas más rápidas en ensamblador (20 % del cuadro)
   y partir `draw_obj` por caras para que ningún objeto pase de 15 ms. El alambre solo no ahorra nada: el 55 % del cuadro (transformar,
   proyectar, reunir, ordenar) es el mismo.

### 6. Cambios en el código (todo con el comportamiento por defecto intacto)

* `src/qdos68k.s`: enganche `plat_isr_fn` / `plat_isr_sp` en `tm_poll`.
* `src/nav.c`: rechazo por caja de las balizas antes de `dist_m` (mismo resultado, `nav_update` de 76 a 26 ms).
* `src/flight.c/.h`: `flight_engines()` separada; `-DENG_EXTERNAL` hace que `flight_step` no la llame. Salida de las pruebas idéntica.
* `src/qfs.h`: `PHYS_DT` / `PHYS_HZ` se pueden cambiar por línea de órdenes.
* `src/view3d.c/.h`: reunir y ordenar en pasos (`view_begin_init`, `view_gather_step`, `view_sort_step`; `view_world_begin` los encadena y da lo
  mismo); `view_wire` pinta caras como contorno, sin relleno ni ordenar.
* `src/sim.c/.h`: `sim_vjob_step` (vista reanudable, alambre o sólida).
* `src/panel.c/.h`: el horizonte en tres tiempos (`ai_begin/ai_rows/ai_end`, salida **idéntica**); `-DAI_INCR` calcula el horizonte con una
  sola división por cuadro (160 → 110 ms; difiere hasta 52 bytes por cuadro, un píxel en el borde cielo/tierra; **desactivado**);
  `panel_piece` y `panel_job_step`.
* `src/bench.c`, `src/prof.c`, `tools/ql_prof.sh` (`PROFDIR`, `BUILD_ONLY`, `RUN_ONLY`), `tools/ql_bench.sh`, `tools/bench_sched.sh`,
  `tools/bench_parse.py`, `tools/bench_table.py`, `tests/dtstudy.c`: el banco de pruebas.

Reproducir (necesita el sQLux con el parche de perfil, como `make prof`):

```
tools/ql_bench.sh 1  /tmp/costes 1 -DAI_INCR -DENG_EXTERNAL          # costes sueltos
tools/ql_bench.sh 7  /tmp/panel  1 -DAI_INCR                         # unidades del panel
tools/ql_bench.sh 6  /tmp/vista  1 -DBENCH_UNITS                     # unidades de la vista
tools/ql_bench.sh 11 /tmp/fisica 1 -DB_RUN=500                       # física sola a toda marcha
tools/bench_sched.sh /tmp/e id:patron:isr:banderas ...               # planificador por turnos; tools/bench_table.py /tmp/e
gcc -DHOST ... tests/dtstudy.c ; gcc ... -DPHYS_DT=5243L -DPHYS_HZ=12 ... tests/dtstudy.c     # dinámica a 25 y 12,5 Hz
```

### 7. Cautelas

* **Todo es emulador** (sQLux con ciclos del 68008 y contención del ZX8301, relación ciclos/reloj 1,002). No se ha probado en un QL real.
  Las secciones 2 a 5 se midieron **sin** `IPC_ROM`; la sección 8 las repite con `IPC_ROM = Hermes2_20_rom` (el 8049 real, que ahora lleva `tools/ql_prof.sh` con la rama `profile` de `~/src/sQLux`): no cambia ninguna conclusión.
* **El teclado**: aquí una fila de `KEYROW` cuesta 0,5 ms, pero las rondas anteriores de este documento cuentan ~125.000 ciclos (17 ms) por fila medidos en otra ronda.
  Una de las dos cifras no es la del hardware: si fueran 17 ms, tres filas serían 50 ms y la lectura del teclado también tendría que ir
  fuera de los 20 ms de un turno. Hay que medirlo en el QL.
* **La interrupción de sondeo** llama a código en C con una pila propia de 4 KB y no hace llamadas a QDOS dentro (el teclado queda fuera). No se
  ha probado en Minerva real ni con otras ROM; sQLux sí ejecuta la ROM `MIN198`.
* La escena 1 es un caso denso (aeropuerto y ciudad a 5 km). En crucero la vista cuesta menos y el panel, sin cambios, ~40 ms.
* El panel «todo cambiando» es el peor caso (el avión alabea a 3º por refresco y sube 15 m y gira 3º entre vueltas); en vuelo recto con
  los instrumentos quietos solo cuestan las cadenas de radios y motor.

### 8. Repetición con `IPC_ROM = Hermes2_20_rom` y el simulador entero con el planificador

**Planificador por turnos, mismos 16 experimentos** (`/e4`; entre paréntesis lo que salió sin `IPC_ROM`). Solo cambia el teclado: una tarea `K` (3 filas + mandos
+ ...) pasa de 2 ms a 3 ms de reloj en su turno, y una fila de `KEYROW` de 0,5 a 1,45 ms. Todo lo demás queda en una décima:

| Patrón | | F real (Hz) | Panel FPS | Vista FPS |
| --- | --- | ---: | ---: | ---: |
| solo física y teclado | coop / ISR | 12,53 / 12,52 (12,47 / 12,53) | – | – |
| panel en F+2 cada 8 | coop / ISR | 10,92 / 12,49 (10,82 / 12,53) | 1,00 / 1,00 | – |
| panel en F+2 cada 4 | coop / ISR | 10,12 / 12,52 (10,12 / 12,49) | 1,55 / 1,45 | – |
| panel en todo lo libre | coop / ISR | 9,16 / 12,52 (9,29 / 12,51) | 2,30 / 1,90 (2,30 / 1,95) | – |
| alambre, un trozo por periodo | coop / ISR | 12,53 / 12,52 | – | 0,60 / 0,60 |
| alambre en todo lo libre | coop / ISR | 11,02 / 12,52 (10,87 / 12,52) | – | 0,80 / 0,80 (0,85 / 0,80) |
| panel y alambre a medias | coop / ISR | 9,67 / 12,52 (9,36 / 12,52) | 1,55 / 1,40 | 0,20 / 0,20 |
| `FVQN FVNN` | coop / ISR | 11,33 / 12,51 | 0,60 / 0,60 | 0,50 / 0,55 |

La física en la interrupción sigue en 12,5 Hz exactos (paso: 10,2 ms de media, 15,8 de máximo; relación de reloj 1,002) y la CPU total, igual que antes.
Las conclusiones de la sección 4 no cambian.

**Todo el simulador con el planificador** (`make` lo activa por omisión; `make SCHED=0` vuelve al bucle de siempre; tras cambiar de modo hay que
`make clean`). Es el programa real, no el banco: `sim_frame` hace la tarea del turno según `SCHED_PAT` (`VFKYVFKQVFKEVFKQVFKYVFKQVFKEVFKQ`: teclado y
mandos en el turno 1 de cada 4, motores `E` y navegación `Y` en turnos fijos, panel `Q` —uno de cada 8 turnos, 4 de 32— y vista `V` por los libres; el indicador de la tecla `\` muestra `3D`, `SIM` y `PAN`, las vueltas del panel por segundo), la física va en la interrupción de
50 Hz, el reloj del simulador se cuenta con los 50 Hz y la tecla **W** alterna vista sólida y de alambre. Medido con `PROF_SCHED_RUN` (30 s de reloj, tras
calentar la caché), escena 1 (final a 5 km) y escena 0 (cabecera):

| Escena | Física | Vistas completas | Teclado |
| --- | ---: | ---: | ---: |
| 1, vista sólida | **12,50 pasos/s** (12,52 reales) | 0,67 /s | 11,0 /s |
| 1, alambre | 12,49 | 0,60 | 11,1 |
| 0 (cabecera, sólida) | 12,50 | 0,33 | 10,4 |

**Navegación y ATC por partes (letra `Y`).** `nav_update` cuesta ~23 ms y `atc_update` ~1,2 (medido en sQLux, `tools/ql_bench.sh 1`), más que un turno. `nav_update`
se parte en `NAV_PARTS` = 5 (`nav_part`: NAV1 3,6 ms, NAV2 4,1, ADF 3,7, balizas en dos mitades 7,3 y 4,7; `nav_update` las llama todas y da el mismo resultado) y el turno `Y` solo
arranca el trabajo (`sch_ystage`): `sch_y_step` hace una parte tras otra en lo que sobra del turno y de los siguientes (antes que la vista) y se para en cuanto cambia el contador de
50 Hz, así que como mucho un turno se pasa por una parte (≤ 7 ms) en vez de 27 ms. `Y` está dos veces por patrón (cada 16 turnos); la pulsación de contacto (`K`) llama solo a `atc_update` (no necesita `nav_update`: solo lee `com` y `squawk`).

**Recuperación de turnos saltados y tres patrones** (`sim_frame`: si el contador avanza más de uno, las letras `K`, `E` e `Y` que cayeron en los turnos saltados se
ejecutan una vez cada una en cuanto el hilo principal vuelve; `Q` y `V` reanudan solos y `F` la hace la interrupción). Medido con `PROF_SCHED_RUN=1500` (30 s de reloj, SPEED 1,
sQLux, volcado tras calentar; `tools/sched_parse.py`), todos con el código nuevo (`Y` por partes, `K` sin `nav_update`). Física 12,48-12,50 pasos/s en las doce medidas.

| Patrón | Escena | Vistas/s | Panel (vueltas/s) | `Y` /s | Teclado /s | Turnos saltados |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| A `FKQVFKVVFKEVFKVVFKYNFKVVFKEVFKVV` (el anterior) | 0 cabecera | 0,83 | 1,20 | 1,56 | 12,45 | 15,5 % |
| | 1 final 5 km | 1,33 | 0,33 | 1,57 | 12,46 | 16,9 % |
| | 3 noche | 1,17 | 0,13 | 1,57 | 10,49 | 47,8 % |
| | 4 reactor 12 km | 1,27 | 0,30 | 1,53 | 11,60 | 29,7 % |
| B `FKQVFKEVFKQVFKYVFKQVFKEVFKQVFKYV` | 0 | 0,70 | 4,70 | 3,13 | 12,50 | 13,8 % |
| | 1 | 1,13 | 1,57 | 3,13 | 12,47 | 14,8 % |
| | 3 | 1,06 | 0,93 | 2,36 | 10,48 | 43,5 % |
| | 4 | 1,10 | 1,40 | 3,10 | 11,63 | 25,3 % |
| **C `VFKYVFKQVFKEVFKQVFKYVFKQVFKEVFKQ`** (por omisión) | 0 | 0,70 | 5,00 | 3,13 | 12,50 | 13,1 % |
| | 1 | 1,10 | 1,60 | 3,13 | 12,50 | 13,1 % |
| | 3 | 1,07 | 0,83 | 2,83 | 10,60 | 41,3 % |
| | 4 | 1,13 | 1,37 | 3,13 | 11,73 | 24,3 % |

Lo que se lee: el panel pasa de 0,1-1,2 a 0,8-5 vueltas/s a cambio de un 15-20 % de vistas; `Y` completa todas sus vueltas (3,13/s) salvo de noche (90 %); el teclado
no llega a 12,5/s en las escenas 3 y 4 porque una unidad larga de la vista cubre más de un periodo de `K` (4 turnos) y las `K` saltadas se recuperan como una sola.

**Panel por partes en los huecos de turno** (`sch_q_run`). El turno `Q` solo *empieza* la vuelta (guarda la línea de estado, `build_status` ~4,8 ms) y la vuelta sigue en lo que sobra
de todos los turnos que no son de la vista, después de la parte pendiente de `Y` (`panel_job_step` ya era reanudable). Una vuelta nueva no empieza antes de `SCHED_PANEL_TICKS`
turnos desde que empezó la anterior (16 por omisión: 3,1 vueltas/s como máximo) y siempre en un turno `Q`. Unidades de `panel_job_step`, medidas en sQLux con todo cambiando
(`tools/ql_bench.sh 7 salida 1 -DAI_INCR`): altitud 9,8 ms (**la primera de la vuelta**, para que quepa en el turno `Q` con `build_status`), horizonte (decidir 2,7, nueve bandas de 1,5-3,3, avión y aro 1,8),
velocidad 0,5, variómetro 0,6, **rumbo en tres unidades** (fondo y rayitas 5,7, números 7,7, rumbo central 1,5; antes una de 15 ms), viraje 1,1, rpm 1,5 (máx 11,6), CDI 0,5, radios 0,7, motor 2,5,
mensajes y FPS 4,3. **Vuelta completa 61 ms en 21 unidades** (los 260 ms de la sección 2 son anteriores al horizonte incremental). Comprobado en el PC: tras 80 vueltas con el avión
maniobrando la pantalla del panel es idéntica, píxel a píxel, a la del orden anterior.

| `SCHED_PANEL_TICKS` | Escena | Vistas/s | Panel (vueltas/s) | Teclado /s | Turnos saltados |
| --- | --- | ---: | ---: | ---: | ---: |
| (antes: panel solo en `Q`, hueco 8) | 0 / 1 / 3 / 4 | 0,70 / 1,10 / 1,07 / 1,13 | 5,00 / 1,60 / 0,83 / 1,37 | 12,50 / 12,50 / 10,60 / 11,73 | 13 / 13 / 41 / 24 % |
| 8 | 0 | 0,70 | 5,87 | 12,50 | 11,1 % |
| | 1 | 0,97 | 5,03 | 12,43 | 11,8 % |
| | 3 | 0,90 | 2,93 | 10,67 | 36,9 % |
| | 4 | 0,97 | 4,16 | 11,89 | 20,9 % |
| **16** (por omisión) | 0 | 0,73 | 3,00 | 12,50 | 12,9 % |
| | 1 | 1,00 | 3,00 | 12,50 | 12,9 % |
| | 3 | 0,93 | 2,23 | 10,50 | 37,3 % |
| | 4 | 1,07 | 2,77 | 11,77 | 23,2 % |
| 32 | 0 | 0,80 | 1,57 | 12,50 | 14,9 % |
| | 1 | 1,13 | 1,53 | 12,50 | 13,9 % |
| | 3 | 1,00 | 1,37 | 10,47 | 40,4 % |
| | 4 | 1,10 | 1,50 | 11,93 | 24,2 % |

Física 12,47-12,50 pasos/s, `Y` 2,9-3,1/s y motores 3,1/s en todas. Con 16 el panel sube de 0,8-1,6 a 2,2-3,0 vueltas/s (escenas 1, 3 y 4) por un 5-13 % de vistas; con 8 llega a 4-6 vueltas/s en las escenas
0, 1 y 4 por un 12-16 %; con 32 el panel se queda como estaba. De noche (escena 3) el límite lo pone el coste de la vuelta, no el valor.

**Máquinas rápidas** (`sch_pass`). Si la vista acaba un cuadro antes de que acabe el turno (la máquina da más de 12,5 cuadros/s), lo que queda de ese turno y de las llamadas
siguientes dentro del mismo turno (`sim_frame` ya no vuelve sin hacer nada si el contador no ha cambiado) se reparte así: primero lo pendiente (la parte de `Y`, la vuelta del panel a medias,
también en un turno `V` si la vista está entre dos cuadros) y, si la vista acaba de terminar un cuadro dentro del turno, una vuelta **nueva** del panel cada `SCHED_PANEL_FAST` = 8 turnos
(6,25 por segundo) en cualquier turno, no solo en `Q`; la vista empieza el cuadro siguiente después. El ritmo se ancla al instante previsto de la vuelta, no al real, para que no se atrase por lo
que tarda la vista en acabar su cuadro. En una máquina lenta nada cambia (la holgura no existe). Medido con `SPEEDV=<n> PROF_SCHED_RUN=1500` (`tools/ql_prof.sh`: la CPU emulada es n veces la del QL; el
contador de 50 Hz sigue el vídeo real). Velocidades de referencia: **×1 QL, ×2 QL con Gold Card, ×8 QL con Super Gold Card, ×40 ilimitado**. Con `SPEEDV` distinto de 1 la columna «real» de `sched_parse.py` no significa nada:

| CPU | Escena | Vistas/s | Panel (vueltas/s) | Teclado /s | Turnos saltados |
| --- | --- | ---: | ---: | ---: | ---: |
| ×1 (QL) | 0 cabecera | 0,76 | 3,29 | 12,47 | 13,0 % |
| | 1 final 5 km | 1,00 | 3,10 | 12,37 | 13,5 % |
| | 3 noche | 0,93 | 2,47 | 10,27 | 37,5 % |
| | 4 reactor 12 km | 1,03 | 2,83 | 11,53 | 23,5 % |
| ×2 (Gold Card) | 0 | 2,03 | 3,53 | 12,50 | 4,4 % |
| | 1 | 2,93 | 3,33 | 12,50 | 5,9 % |
| | 3 | 2,57 | 2,60 | 12,47 | 33,7 % |
| | 4 | 2,93 | 3,00 | 12,50 | 16,2 % |
| ×8 (Super Gold Card) | 0 | 9,60 | 5,10 | 12,50 | 0 |
| | 1 | 15,03 | 6,10 | 12,50 | 0 |
| | 3 | 12,77 | 6,10 | 12,50 | 0 |
| | 4 | 14,33 | 6,17 | 12,50 | 0 |
| ×40 (ilimitado) | 0 | 49,97 | 6,27 | 12,50 | 0 |
| | 1 | 80,83 | 6,27 | 12,50 | 0 |
| | 3 | 67,83 | 6,27 | 12,50 | 0 |
| | 4 | 74,83 | 6,23 | 12,50 | 0 |

Con el ritmo anclado, 30 s de reloj por celda. Física 12,50 pasos/s, `Y` 3,1/s y motores 3,1/s en todas. A ×1 y ×2 (vista por debajo de 12,5 cuadros/s) el panel ya pasa de los 3,1/s del ritmo normal en las
escenas 0 y 1 porque a veces la vista acaba un cuadro dentro del turno (0,2-1,4 vueltas/s empiezan con holgura); a ×8 las escenas 1, 3 y 4 ya dan más de 12,5 vistas/s y el panel va a ~6,1 (el tope es 6,25); la
escena 0, la más cargada, da 9,6 vistas/s y 5,1 vueltas/s; a ×40 el panel está en el tope. Sin saltos de turno desde ×8. La vista sube del ritmo de la física porque tras acabar un cuadro empieza otro con lo que sobra del turno.

Cambios que ello trae al código normal:

* `USE_SCHED` (Makefile) fija `PHYS_DT` = 1/12,5 s, `ENG_EXTERNAL` y `AI_INCR` (el horizonte cambia hasta un píxel en el borde cielo/tierra).
* La física de la interrupción no integra si el hilo principal reorganiza las teselas (`view_begin_init`) o reinicia el vuelo (`sim_reset`): cuenta
  el paso como perdido y el hilo principal lo recupera después (`phys_hold`, `phys_missed`). Es una protección sencilla; sigue sin probarse un cambio de tesela con
  la física corriendo en el QL real.
* `plat_isr_set()` (`platform_ql.c`, pila de 8 KB; en PC no hace nada) y `sim_stop()` (la llama `main_ql.c` al acabar el vuelo).
* El mensaje de choque se pinta también en la vista reanudable; `flight.c` calcula el salto de colisiones con `PHYS_HZ`.
* Pruebas: `tests/schedtest.c` (PC, con `-DUSE_SCHED`; simula la interrupción con `host_tick()`) despega y da 12,50 pasos/s y el reloj exacto;
  `hosttest` sale idéntico.

Qué mirar en un QL real: que el reloj (tecla `\`: ahora el segundo número son pasos de física por segundo, ~12,5) no se atrase —si lo hiciera, la
interrupción tarda más de 20 ms—, que el teclado responda (cada fila se lee cada ~0,4 s en las filas poco usadas) y que no se cuelgue al cambiar de tesela.

### 9. Resumen de los cambios del planificador (27 noviembre 2026)

Todo medido en sQLux con `PROF_SCHED_RUN=1500` (30 s de reloj), escenas 0, 1, 3 y 4, y comprobado en el PC (`hosttest` idéntico a la versión anterior, `schedtest` a 12,50 pasos/s).

| Cambio | Dónde | Efecto |
| --- | --- | --- |
| Patrón `VFKYVFKQVFKEVFKQVFKYVFKQVFKEVFKQ` (V antes de F; Y 2, Q 4, E 2 por 32 turnos) | `src/sim.c` `SCHED_PAT` | panel con 4 turnos de 32 en vez de 1; `K` e `Y` fijos |
| Indicador `3D … SIM … PAN …` (vueltas de panel por segundo) | `panel.c`, `sim.c` (`fps_p`, `pps10`) | tecla `\` |
| `nav_update` en 5 partes (`nav_part`: NAV1 3,6 ms, NAV2 4,1, ADF 3,7, balizas 7,3 + 4,7) y `atc_update` 1,2 ms | `src/nav.c` | ninguna unidad pasa de ~7 ms; antes ~23 ms de golpe |
| Turno `Y` solo arranca el trabajo; sigue en los restos de turno antes que el panel y la vista | `sim.c` (`sch_ystage`, `sch_y_step`) | `Y` acaba en 1-2 turnos y completa 3,1 ciclos/s |
| Contacto ATC (`K`) ya no llama a `nav_update` (`atc_update` solo lee `com` y `squawk`) | `sim.c` | −23 ms en cada pulsación |
| `K`, `E` e `Y` saltados por una unidad larga se recuperan una vez | `sim_frame` | `K` a 12,5/s salvo cuando una unidad de la vista cubre más de 4 turnos (de noche y reactor: 10,3-11,7) |
| Panel: altitud la primera, rumbo en 3 unidades, vuelta en 21 unidades (61 ms con todo cambiando) | `panel.c` (`panel_job_step`) | pantalla idéntica píxel a píxel |
| El panel sigue en los restos de los turnos que no son V; `SCHED_PANEL_TICKS` = 16 entre inicios de vuelta | `sim.c` (`sch_q_run`) | de 0,1-1,2 a 2,2-3,3 vueltas/s a ×1 por un 5-13 % de vistas |
| Máquinas rápidas: pasada `sch_pass` (pendiente → vuelta nueva del panel con holgura cada `SCHED_PANEL_FAST` = 8 → vista) y `sim_frame` trabaja también en llamadas repetidas del mismo turno | `sim.c` | panel a 6,1-6,3 vueltas/s desde ×8; vista de 10 a 80 cuadros/s |
| Medida: `SPEEDV` en `tools/ql_prof.sh`, estadísticas del planificador en `prof.c`/`sched_parse.py`, partes de `nav` en el banco (`bench.c`) | `tools/`, `src/prof.c`, `src/bench.c` | |

Velocidades de medida a usar siempre: ×1, ×2, ×8 y ×40 (QL, Gold Card, Super Gold Card, ilimitado).

**Pendiente / no hecho:** meter el teclado en la interrupción (el IPC es una `trap #1` que no se ha probado dentro de la interrupción ni en un QL real; en el hardware una fila de IPC podría costar ~17 ms,
ver sección 7); un `K` por cada `K` saltado (dos lecturas seguidas dan la misma pulsación); partir la altitud (9,8 ms) si se quiere que quepa en un hueco menor que el turno `Q`;
medir el planificador completo en un QL real.

## Memoria alta: todo el programa en la RAM alta (sin la contención del ZX8301)

Los primeros 128 KB de RAM ($20000–$3FFFF) los comparte la CPU con el ZX8301: cada acceso espera al vídeo (en sQLux: `zx8301_insn` y
`zx8301_ram`, tanto en la captura de instrucciones como en los datos). A partir de $40000 no hay contención. Lo que sigue es qué estaba
abajo sin que lo supiéramos, cuánto costaba y cómo se ha arreglado.

### Dónde estaba cada cosa (`tools/addr_parse.py`, `-DPROF_ADDR`)

Medido con Minerva 1.98, JS y RAMTOP de 512, 640 y 768 KB (siempre lo mismo):

| | antes | ahora |
| --- | --- | --- |
| Código del programa | $09–$0B…, **arriba** | arriba |
| Datos globales (`sim`, tablas, BSS) | **arriba** (a continuación del código) | arriba |
| **Pila de la tarea** (`_stack`, 32 KB) | **$3D8FE: en la RAM contenida** | $46962: arriba |
| **`malloc`** (teselas del escenario, objetos, tablas del mundo, modelo del avión, listas de colisión, mapas de letras) | **$2BB1C–$33B6C: en la contenida** | arriba |
| `plat_alloc_hi` (vista 3D, panel, rosa) | arriba (ya estaba) | arriba |
| Pila de la interrupción (8 KB) | arriba (es un array global) | arriba |

Es decir: el código y las variables globales ya iban arriba (QDOS carga la tarea, de ~110 KB, en el primer hueco donde cabe, y por debajo de
$40000 solo quedan, se estima, unos 70 KB libres), pero **la pila y todo lo que daba `malloc` salían del montón común por abajo, que es la RAM interna**. El
`plat_alloc_hi` de la vista solo cubría el buffer de vídeo; lo demás que se lee sin parar —las teselas con los vértices y caras que
transforman `gather_tile`, `xform_obj` y `poly_fast` en cada cuadro, y las llamadas, argumentos y variables locales de todas las funciones en la
pila— corría con los estados de espera del vídeo.

### Qué se ha hecho (`src/platform_ql.c`, `src/main_ql.c`, `src/qdos68k.s`)

* **Tapón** (`plat_himem_init`): al arrancar se llena el hueco bajo del montón con bloques que se quedan reservados (tallas de 32 KB hasta 64 bytes,
  hasta que `malloc` devuelve una dirección ≥ $40000); a partir de ahí **cualquier `malloc` del programa, de la biblioteca de C o de `fopen` cae
  arriba**. Son ~77 KB de RAM interna (`$12E80` en la prueba). Se devuelven al salir (`plat_exit`). Si no hay RAM de expansión (el primer
  `malloc` tras el tapón sale bajo o falla) se suelta el tapón y el programa funciona como antes: en un QL de 128 KB todo es lento y no hay remedio.
* **Pila alta**: `_stack` baja de 32 KB a 6 KB (solo hasta `main`), se reserva un bloque de 24 KB arriba y `plat_run` (`plat_run_on_stack`,
  ensamblador) llama al programa real con la pila allí: guarda la pila vieja en la nueva, hace `jsr` y la recupera al volver. No se toca ningún registro salvo A0, A1 y A7.
* `-DNO_HIMEM` (compilar) deja el comportamiento anterior, para medir.
* **Aviso en el menú**: `plat_low_warn` pinta en rojo «AVISO: PROGRAMA EN RAM LENTA (CONTENIDA)» si hay RAM de expansión y el código o los
  datos globales caen por debajo de $40000.

La parte baja queda **vacía para el programa** (el tapón la ocupa): el menú no usa memoria apreciable (dibuja en la pantalla) y la lista de
teselas del escenario (`tdir_off`, `tdir_size`, 20 KB, solo se leen al cargar una tesela) también está arriba, porque con el tapón no hay forma
de pedir memoria baja. Una reserva de datos «fríos» abajo sería posible dejando un bloque del tapón como reserva y repartiéndolo a mano; ahora
mismo sobraría complejidad por 20 KB que no se leen en el bucle.

### Resultado (escena 1 y 0 de `src/prof.c`, 10 cuadros, SPEED 1,0)

| | todo abajo (`NO_HIMEM`) | pila y monton arriba |
| --- | ---: | ---: |
| Escena 1 (final a 5 km), s por 10 cuadros | 4,30 | **3,38** (−21 %) |
| Escena 0 (cabecera), s por 10 cuadros | 7,52 | **6,18** (−18 %) |
| Planificador, vistas por segundo (escena 1, física en la interrupción) | 0,67 | **0,77** (+15 %) |
| Planificador, pasos de física por segundo | 12,50 | 12,50 |

Todo el programa corría un quinto más despacio por esos accesos. Lo medido antes de este cambio (las secciones anteriores de este documento, el planificador y el horizonte y la rosa) se hizo con la pila y el montón abajo; todas esas cifras son por tanto pesimistas en esa
proporción para el programa final.

### Sobre el código

Hoy el ejecutable (~80 KB de código y ~26 KB de datos globales en un mismo bloque) no cabe en los ~70 KB que quedan libres bajo $40000, así que
QDOS lo carga arriba; es lo que sale en las pruebas con ambas ROM. Si el ejecutable se hiciera pequeño (p. ej. quitando el panel o el escenario
de Iberia) podría caer abajo; el aviso del menú lo detecta. Para **forzarlo** arriba haría falta que el hueco bajo estuviera lleno **antes** del
`EXEC`: un primer programa residente que llame a `plat_himem_init`, se quede suspendido (`mt_susjb`) y lance el segundo, o equivalente desde
SuperBASIC. No se ha implementado: no hay forma de probar en el emulador un QDOS que cargue por abajo (los dos ROM probados cargan arriba) y
en esa situación el aviso ya lo dice. La alternativa sería hacer el código independiente de la posición (`bsr` y datos relativos a A4), que c68 no genera.

### Qué mirar en un QL real

* `plat_himem_ok` y el aviso del menú: con 512 KB de RAM de expansión no debe salir el aviso.
* Que abrir ficheros y canales (disco o red) siga funcionando con el tapón puesto: QDOS reserva sus propias estructuras en el montón común
  y, con el hueco bajo lleno, las pone arriba. Si faltara memoria arriba, el programa se queda sin ella antes: el tapón se puede achicar
  quitando tallas de la lista (`plat_himem_init`).
* La pila alta es de 24 KB (antes 32 KB bajos): si hubiera desbordamiento, subir el 24576 de `plat_himem_init` y `plat_run`.

## Ventana 3D más pequeña: medido y descartado (28 noviembre 2026)

Pregunta: ¿cuánto sube el FPS de la vista si la ventana pasa de 480x96 a 240 o 384 de ancho y 64 o 48 de alto? Se hizo una prueba con el centro, las mitades y las focales de la ventana como variables (campo horizontal fijo de 70°, así que `fx` baja con el ancho; pixel 1:1,5) y se midió con `PROF_SCHED_RUN=1500`, SPEED 1,0, `IPC_ROM = Hermes2_20_rom`, escenas 0, 1, 3 y 4. Vistas completas por segundo:

| Ventana | Esc. 0 | Esc. 1 | Esc. 3 | Esc. 4 | Media | Ganancia |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 480x96 | 0,73 | 0,97 | 0,90 | 1,03 | 0,91 | — |
| 480x64 | 0,77 | 1,13 | 0,93 | 1,07 | 0,98 | +7 % |
| 384x96 | 0,77 | 0,97 | 0,90 | 1,07 | 0,93 | +2 % |
| 384x64 | 0,80 | 1,10 | 0,93 | 1,13 | 0,99 | +9 % |
| 240x96 | 0,83 | 1,03 | 0,93 | 1,20 | 1,00 | +10 % |
| 240x64 | 0,87 | 1,10 | 0,90 | 1,20 | 1,02 | +12 % |
| 240x48 | 0,90 | 1,17 | 0,96 | 1,27 | 1,08 | +18 % |
| 192x48 | 0,90 | 1,17 | 0,97 | 1,27 | 1,08 | +19 % |

La mejor escena gana un 23 % (cabecera, 192x48) y la peor un 8 % (noche). Muy por debajo del 50 %: el coste lo ponen los vértices (transformar y proyectar, luces), que no dependen del tamaño de la ventana; solo bajan el relleno, el cielo y el volcado. Además la versión con variables cuesta un ~3 % en 480x96 (los descartes de `xform_pre` pasan de desplazamientos a `xp_mul`). No se aplicó: ni menú de opciones ni cambio de tamaño. Para ganar más habría que reducir objetos (alcance, LOD), no píxeles.
