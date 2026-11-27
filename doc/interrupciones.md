# Estudio: física, panel y vista 3D repartidos por turnos de la interrupción de 50 Hz

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

## 1. Peso del modelo físico y del 3D

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

## 2. Cuánto cuesta cada tarea (`BENCH_MODE=1,5,6,7`, ciclos reales del 68008)

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

## 3. La frecuencia de 12,5 Hz y la dinámica

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

## 4. Experimentos de planificación (`BENCH_MODE=10`, 1000 turnos = 20 s cada uno)

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

## 5. Qué hay que hacer para que sea real

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

## 6. Cambios en el código (todo con el comportamiento por defecto intacto)

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

## 7. Cautelas

* **Todo es emulador** (sQLux con ciclos del 68008 y contención del ZX8301, relación ciclos/reloj 1,002). No se ha probado en un QL real.
  Las secciones 2 a 5 se midieron **sin** `IPC_ROM`; la sección 8 las repite con `IPC_ROM = Hermes2_20_rom` (el 8049 real, que ahora lleva `tools/ql_prof.sh` con la rama `profile` de `~/src/sQLux`): no cambia ninguna conclusión.
* **El teclado**: aquí una fila de `KEYROW` cuesta 0,5 ms, pero `optimizacion.md` cuenta ~125.000 ciclos (17 ms) por fila medidos en otra ronda.
  Una de las dos cifras no es la del hardware: si fueran 17 ms, tres filas serían 50 ms y la lectura del teclado también tendría que ir
  fuera de los 20 ms de un turno. Hay que medirlo en el QL.
* **La interrupción de sondeo** llama a código en C con una pila propia de 4 KB y no hace llamadas a QDOS dentro (el teclado queda fuera). No se
  ha probado en Minerva real ni con otras ROM; sQLux sí ejecuta la ROM `MIN198`.
* La escena 1 es un caso denso (aeropuerto y ciudad a 5 km). En crucero la vista cuesta menos y el panel, sin cambios, ~40 ms.
* El panel «todo cambiando» es el peor caso (el avión alabea a 3º por refresco y sube 15 m y gira 3º entre vueltas); en vuelo recto con
  los instrumentos quietos solo cuestan las cadenas de radios y motor.

## 8. Repetición con `IPC_ROM = Hermes2_20_rom` y el simulador entero con el planificador

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
`make clean`). Es el programa real, no el banco: `sim_frame` hace la tarea del turno según `SCHED_PAT` (`FKQVFKVVFKEVFKVVFKYNFKVVFKEVFKVV`: teclado y
mandos en el turno 1 de cada 4, motores `E` y navegación `Y` en turnos fijos, panel `Q` y vista `V` por los libres), la física va en la interrupción de
50 Hz, el reloj del simulador se cuenta con los 50 Hz y la tecla **W** alterna vista sólida y de alambre. Medido con `PROF_SCHED_RUN` (30 s de reloj, tras
calentar la caché), escena 1 (final a 5 km) y escena 0 (cabecera):

| Escena | Física | Vistas completas | Teclado |
| --- | ---: | ---: | ---: |
| 1, vista sólida | **12,50 pasos/s** (12,52 reales) | 0,67 /s | 11,0 /s |
| 1, alambre | 12,49 | 0,60 | 11,1 |
| 0 (cabecera, sólida) | 12,50 | 0,33 | 10,4 |

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
