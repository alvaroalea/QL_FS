# Horizonte artificial y rosa del rumbo: dónde se iba el tiempo y qué se ha hecho

Medido en sQLux (rama `profile`, `IPC_ROM = Hermes2_20_rom`, 7,5 MHz con contención del ZX8301), escena 1, con el avión maniobrando
(`BENCH_MODE=7` y `8`, ver `tools/ql_bench.sh`). Todo lo nuevo da **los mismos píxeles** que lo anterior salvo lo indicado.

## Resultado

| | antes | ahora |
| --- | ---: | ---: |
| Horizonte, un repintado (alabeo de 3º por actualización) | 158 ms (110 con `AI_INCR`) | **~28 ms** (filas 24 + avión 1,4 + decidir 2) |
| Rosa del rumbo, un grado de cambio | 95 ms | **17 ms** |
| Vuelta completa del panel con todo cambiando | 260 ms | **120 ms** |
| FPS del panel en los turnos libres (física en la interrupción, `FKQQ…`) | 1,9 | **3,25** |
| Panel en un turno de cada 8 / en uno de cada 4 | 1,0 / 1,45 | **1,05 / 1,95** |

No llega a 6,25 FPS, pero la vuelta completa en el peor caso ya baja de 120 ms y la mayor unidad indivisible es la altitud (29 ms); el horizonte
y la rosa dejan de ser el problema. Quedan, por orden: altitud 29 ms, horizonte 28, radios 18, rosa 17, motor 11, mensajes 5.

## Horizonte artificial

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

## Rosa del rumbo

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

### ¿Tiene sentido un bitmap por dígito en lugar de imprimir texto?

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

## Qué quedó descartado o pendiente

* Rayitas cortas en las copias: ya lo estaban (10 copias antes; ahora 30 con las largas).
* La **altitud** (29 ms, ahora la mayor unidad del panel) son cuatro agujas de OR exclusivo (borrar y pintar, 8 líneas con recorte) y una cifra de 5 dígitos:
  mismo tratamiento posible (máscaras de aguja por ángulo, 16 ángulos × 2 agujas).
* El texto de radios (18 ms) y motor (11 ms) se rehace entero para compararlo: comparar los valores antes de formar las cadenas lo deja casi a cero cuando no hay cambios.

## Qué cambia en el código

* `src/ql68k.s`: `gfx_memcpy` con vía corta, `gfx_blit`, `gfx_glyph`. `src/gfx.c/.h`: sus versiones en C para el PC.
* `src/panel.c`: `ai_begin/ai_rows/ai_plane/ai_end` (con `AI_INCR`, `AI_DIFF`), `ai_limits`, rosa con tablas y letras preparadas, `GYN = 30`.
  `AI_INCR` y `AI_DIFF` van activos con `USE_SCHED` (`qfs.h`); `-DAI_NODIFF` los separa. Sin `AI_INCR` el horizonte es el de siempre.
* Diferencias de píxeles frente al código anterior (solo con `AI_INCR`): hasta un píxel en el borde cielo/tierra; y en la rosa, donde un número
  verde cae sobre una rayita blanca el número gana (antes ganaba la rayita porque se dibujaba después).
* `src/bench.c` modo 8 (partes sueltas del horizonte y de la rosa) y modo 7 (unidades del panel).
