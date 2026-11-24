# QFS – Simulador de vuelo para Sinclair QL: informe técnico

Nov 24, 2026 · @Alvaro

## Resumen y disco de referencia

QFS es un simulador de vuelo para el Sinclair QL (68008, 640 KB, modo 4 a 512x256 en 4 colores, QSound con PSG) en C con las partes críticas en ensamblador. Ya vuela en un banco de pruebas en PC que usa el mismo código y el mismo formato de pantalla que el QL. Tiene cuatro aviones (planeador, monomotor, bimotor y reactor), un escenario de 3 aeropuertos con ILS, VOR, NDB y ATC básico, día/atardecer/noche y vistas de cabina y exteriores.

La imagen `disk1_1.img` (160 KB, disco de arranque directo de IBM PC sin sistema de ficheros) es **Microsoft Flight Simulator 1.0 (1982, Bruce Artwick)**. Lo que sale de sus textos y de la estructura de sectores (sin desensamblar el código):

- Cuatro zonas de escenario: Chicago (Champaign, Kankakee, Midway, Meigs, O'Hare), Los Ángeles (Catalina, Hughes, John Wayne, LAX, San Diego, Santa Monica, Van Nuys), Seattle (Boeing Field, Port Angeles, Olympia, SeaTac, Paine) y Nueva York/Boston (Block Island, JFK, Logan, Martha's Vineyard, Bridgeport).
- ATIS por aeropuerto con letra de información, pista en uso y frecuencia del localizador ("ILS RUNWAY 31 APPROACH IN USE").
- Editor de entorno: hora, estación, dos capas de nubes, tres capas de viento con altura de cizalladura, viento en superficie y factor de fiabilidad.
- Modo "Europe 1917" (combate), modo demostración, *slew* y "reality mode".
- Mensajes de choque: MOUNTAIN CRASH, BUILDING CRASH, SPLASH! y "CRASH! - LOWER YOUR GEAR NEXT TIME!". QFS los reproduce como homenaje.

QFS toma de él la estructura (mundo plano con objetos, radioayudas, ATIS, choques por tipo de obstáculo), pero añade lo que pedías y el original no tenía: varios aviones cargables con motor de mezcla y paso, flaps, tren retráctil, vista exterior, torre y escenarios cargados de disco por teselas.

## Requisitos y decisiones de diseño

Todas las decisiones salen de dos límites: el 68008 no tiene coma flotante ni multiplicación de 32 bits, y el bus es de 8 bits.

| Tema | Decisión |
| --- | --- |
| Pantalla | Modo 4, 512x256. Vista 3D de 480x96 (columnas 16–495, filas 2–97); el borde va en negro. Panel en las filas 100–255 |
| Píxel del QL | Es 1,5 veces más alto que ancho: focal vertical = 2/3 de la horizontal (fx 343, fy 228 a 70° de campo), círculos dibujados como elipses de 40x27 |
| Colores | 4 sólidos + 12 tramas de 2x2 (gris, agua, marrón, atardecer...) por palabra de 16 bits |
| Doble buffer | La vista 3D se dibuja en un buffer de 12 KB en la RAM de expansión (sin la contienda del vídeo) y se copia de una vez con MOVEM: sin parpadeo y sin tocar las variables de sistema de $28000 |
| Aritmética | Punto fijo: Q16 en física, Q14 en matrices y senos, ángulos BAM16 (65536 = 360°). Tablas de seno, arcotangente y atmósfera ISA generadas en el PC |
| Mundo | x = este, y = norte, z = arriba, en metros Q8 sobre 32 bits: ±8388 km. Suelo plano por tesela con objetos encima, como el original |
| Precisión 3D | Cada objeto elige su unidad (1/8 m, 1/4 m... según distancia) para que todo quepa en 16 bits y se pueda usar MULS |
| Física | Paso fijo a 25 Hz contado por una tarea de sondeo de 50 Hz; el dibujo va tan rápido como pueda |
| Motores | Ninguno (planeador), pistón con gas, mezcla y paso (fijo o de velocidad constante), reactor con N1 y retardo; 1 o 2 motores con empuje asimétrico |
| Datos | Escenarios (.SCN binario por teselas) y aviones (.ACF de texto editable) se cargan de disco |
| Lenguaje | C89 para c68/XTC68 (`qcc`) y ensamblador as68, como en tu port de Elite |
| Sonido | PSG en la QSound 2+: canal A motor, B ruido (viento/reactor), C avisos (pérdida, tren, balizas, ATC) |

La fraseología del ATC va en inglés, como en la aviación real y en el original; la interfaz del programa va en español.

## Presupuesto de memoria y tamaño de escenario

Con 640 KB caben unos 160 km x 160 km de escenario residente, y en streaming desde disco unos 240 km x 240 km por disquete de 720 KB. El límite real es el disco, no las coordenadas (±8388 km).

**Reparto de la RAM (estimado)**

| Zona | Tamaño | Qué hay |
| --- | --- | --- |
| RAM interna, $20000–$27FFF | 32 KB | Pantalla modo 4 |
| RAM interna, resto | \~96 KB | QDOS: variables de sistema, tablas, canales, búferes del disco |
| Programa (expansión) | \~70 KB | C compilado con c68 (\~50 KB) + libc + tablas |
| Datos fijos | \~70 KB | Buffer 3D 12 KB, copia limpia del panel 20 KB, pila 32 KB, estado |
| Caché de teselas | 100–250 KB | 25 teselas cargadas (LRU) |
| Libre | 90–240 KB | QDOS lo usa como caché de disco (slave blocks), lo que acelera las lecturas |

**Densidad medida con el compilador de escenarios** (teselas de 16,384 km = 268 km²):

| Tipo de tesela | Bytes | Contenido |
| --- | --- | --- |
| Rural (río, carretera) | 100–800 | polilíneas y campos |
| Aeropuerto con ILS | 3–4 KB | pista, marcas, luces, torre, hangares |
| Ciudad (70 edificios + luces) | 9–11 KB | manzanas agrupadas de 10 edificios |
| Media realista (20 % densas) | \~2,8 KB |  |

**Escenarios posibles**

- Todo en memoria (250 KB): 90 teselas medias = 24.000 km², unos **155 x 155 km**.
- Por teselas desde disquete DD de 720 KB (\~600 KB de escenario): \~215 teselas = 57.600 km², unos **240 x 240 km**. Solo se mantienen en RAM las 5x5 teselas alrededor del avión (80 x 80 km), más que la visibilidad de 24 km.
- Disco duro o ED de 3,2 MB: varias regiones de ese tamaño, cada una en su .SCN.

Como referencia, cada zona del MSFS original era del orden de 100 x 100 km. Un reactor a 900 km/h cruza una tesela en 65 s; leer 3 KB de disquete cuesta menos de medio segundo, así que el streaming llega de sobra si la lectura se adelanta (ver Puntos a vigilar).

## Rendimiento estimado en el 68008

La estimación es de 6–10 cuadros por segundo en escenas normales y 4–5 sobre la ciudad; el MSFS original iba a 3–5 en un PC de 4,77 MHz. Son cuentas de ciclos (68008 a 7,5 MHz, \~1,5x más lento que un 68000 por el bus de 8 bits), no mediciones: hay que medir en el QL real.

| Tarea por cuadro | Coste estimado | Nota |
| --- | --- | --- |
| Cielo/suelo 480x96 | \~17 ms | `gfx_hspan_raw` en ensamblador, una división por fila |
| Volcado del buffer 12 KB | \~15–20 ms | MOVEM de 48 bytes; la pantalla sufre contienda del ZX8301 |
| Transformación de vértices | \~110 µs/vértice | 9 MULS; 30–170 vértices de día tras el descarte por esfera |
| Líneas | \~20 ms | Bresenham en ensamblador, \~2000 píxeles |
| Polígonos rellenos | \~30 ms | recorte + barrido; el paso de aristas aún en C |
| Panel | \~25 ms | horizonte artificial cada cuadro + 1 de 4 grupos de instrumentos |
| Física (25 Hz) | \~8 % de CPU | \~60 `fxmul` + 10 `fxdiv` por paso |
| **Total** | **\~100–160 ms** |  |

El descarte por esfera envolvente que se añadió durante las pruebas bajó los vértices transformados de 2181 a 106 en la aproximación nocturna: es la optimización que más pesa.

## Arquitectura y estructura del proyecto

El código se reparte en 14 módulos C y 2 de ensamblador: todo el núcleo es portable y solo `platform_*` y `*.s` cambian entre el QL y el PC.

| Fichero | Función |
| --- | --- |
| `qfs.h`, `fixmath.c`, `tables.c` | Tipos, punto fijo, seno/atan/atmósfera, fuente 5x7 (tablas generadas) |
| `gfx.c` / `ql68k.s` | Primitivas modo 4: tramos, líneas, polígonos con recorte, elipses, texto, copias |
| `view3d.c` | Cámara, matriz de actitud, cielo/horizonte, descarte, recorte en el plano cercano, objetos, luces, avión exterior |
| `world.c` | Lector .SCN: cabecera, radioayudas, aeropuertos, caché LRU de teselas, colisiones |
| `aircraft.c` | Lector .ACF: aerodinámica, motores, marcas del ASI, modelo 3D |
| `flight.c` | Modelo de vuelo y motores, suelo, choques |
| `nav.c` | VOR, ILS (LOC + senda), DME, ADF, balizas |
| `atc.c` | ATIS, torre, fases del vuelo, teletipo |
| `panel.c` | Instrumentos con restauración desde copia limpia |
| `sound.c` | Motor, viento, avisos en el PSG |
| `sim.c` | Bucle principal, teclado, vistas, menú |
| `platform_ql.c`, `qdos68k.s`, `main_ql.c` | QL: KEYROW, modo 4, reloj de 50 Hz, PIA de la QSound, rutas `flp1_` |
| `platform_host.c`, `tests/hosttest.c` | PC: pantalla simulada, pruebas y capturas |
| `tools/*.py` | Tablas, compilador de escenarios, aviones, conversor de capturas a PNG |

Cada cuadro sigue este orden:

&#91;embedded content: bucle principal de QFS · 7 etapas\]

El reloj de 50 Hz (tarea de sondeo de QDOS) marca cuántos pasos de física tocan; si el dibujo se retrasa, se hacen hasta 4 pasos seguidos y no se acumula retraso. Las teselas se leen de disco dentro de la Vista 3D cuando el avión se acerca a una nueva.

## Formatos de escenario y avión

Los escenarios se escriben en texto y se compilan en el PC a un binario por teselas; los aviones son texto que el QL lee directamente.

**Escenario fuente (`data/demo.txt`)** — una orden por línea, metros y grados:

```
AEROPUERTO LEQN "QL NORTE" 470 -30 200 ATIS 127.85 TWR 118.30 "ILS RUNWAY 31 APPROACH IN USE."
PISTA LEQN 31 1000 -800 310 1800 30 ILS 110.30 LUCES
VOR QLV 113.20 -5000 -15000 DME
CIUDAD 8000 6000 3000 2400 70 7
MONTANA 34000 -14000 6000 2100 8
```

Otras órdenes: `NDB`, `TORRE`, `HANGAR`, `EDIFICIO`, `CARRETERA`, `RIO`, `LAGO`, `CAMPO`. Una `PISTA` genera sola el pavimento, las marcas, las luces de borde/umbral/fin, las de aproximación, el localizador en el extremo opuesto, la antena de senda a 300 m del umbral, las balizas (7,2 km, 1,05 km, 300 m) y la cabecera opuesta para el ATC.

**Binario `.SCN`** (big-endian, `tools/mkscen.py`):

| Bloque | Tamaño | Contenido |
| --- | --- | --- |
| Cabecera | 64 B | `QFS1`, versión, log2 de tesela, origen, rejilla, contadores, desplazamientos, elevación, nombre |
| Directorio | 8 B por tesela | desplazamiento, tamaño, elevación del suelo |
| Radioayudas | 24 B cada una | tipo (VOR, VOR/DME, NDB, ILS, GS, OM, MM, IM), frecuencia, posición, rumbo, alcance, senda, identificador |
| Aeropuertos | 120 B cada uno | OACI, nombre, posición, frecuencias ATIS/TWR, 4 cabeceras, texto ATIS |
| Teselas | variable | objetos de 20 B de cabecera + vértices s16 + caras `[n, atributo, índices]` |

Tipos de objeto: suelo (pistas, lagos, campos), polilíneas (carreteras, ríos), sólidos con ocultación y orden de caras (edificios, montañas) y luces. Banderas: colisión, agua (SPLASH!), visible de noche, montaña, ordenar caras. En el QL los vértices se usan en su sitio, sin copiarlos.

**Avión `.ACF`** — texto `CLAVE = valor`, decimales sin coma flotante:

```
NOMBRE = BIMOTOR 2X300HP
MOTOR = PISTON          ; NINGUNO, PISTON, REACTOR
MOTORES = 2
HELICE = VARIABLE       ; FIJA o VARIABLE (velocidad constante)
TREN = RETRACTIL
MASA = 2300
SUPERFICIE = 18.5
CL0 = 0.25
CLA = 5.0
ALFA_PERDIDA = 15
FLAPS = 0, 15, 30
POTENCIA = 224          ; kW por motor
BRAZO_MOTOR = 2.4       ; para el empuje asimetrico
VS0 = 70
VNE = 225
V = 0.00, 1.40, 4.50    ; vertices del modelo exterior (m)
F = 3, 0, 0, 1, 2       ; relleno, contorno, vertices
```

Los cuatro aviones de ejemplo salen de `tools/mkaviones.py`, pero una vez generados se editan a mano.

## Mockups de la vista

Las imágenes no son dibujos aparte: las ha pintado el propio motor de QFS en un buffer con el formato exacto de la pantalla del QL (modo 4) y se han pasado a PNG con el aspecto 4:3 del monitor. Lo que se ve es lo que debería verse en el QL.

**Distribución de la pantalla (512x256)**

| Zona | Filas | Contenido |
| --- | --- | --- |
| Vista 3D | 2–97 | 480x96 píxeles, columnas 16–495 |
| Instrumentos, fila 1 | 103–158 | Velocidad (marcas VS0/VFE/VS1/VNO/VNE), horizonte artificial, altímetro, RPM o N1 (dos agujas en bimotores) |
| Instrumentos, fila 2 | 164–219 | Coordinador de giro con bola, direccional, variómetro, CDI con senda de planeo, TO/FR y OBS |
| Bloque derecho | 103–220 | COM1, NAV1 (con identificador), NAV2, ADF con marcación, DME, transpondedor y hora; palancas de gas/paso/mezcla/flaps, luces del tren, MAP/EGT o EGT/FF, combustible, balizas O M I |
| Mensajes | 229 | Teletipo del ATC/ATIS, 82 caracteres |
| Estado | 241 | Vista, avión, PAUSA, PERDIDA!, TREN!, flaps, mensaje de choque |

**Vistas**: cabina (con miradas a izquierda, derecha y atrás, y zoom x0,5–x8), persecución (40 m detrás, 15° a un lado), torre (desde el aeropuerto más cercano, con zoom automático) y lateral. Las vistas exteriores pintan el modelo del avión del .ACF con su actitud real.

**Luz**: de día cielo blanco y suelo verde; al atardecer (19–21 h y 6–7 h) cielo de trama roja y suelo verde oscuro; de noche solo se ven las luces (pistas, aproximación, ciudad, torre).

**Cabina, de día, en viraje a la derecha.** El horizonte se inclina igual que en el horizonte artificial; panel completo con ILS sintonizado.

&#91;image: Vista de cabina en viraje\]

**Cabina en la cabecera de la pista 31**: pista con marcas, torre y hangares a la derecha.

&#91;image: Vista de cabina en la pista\]

**Exterior, vista de persecución**: el monomotor visto desde atrás y a un lado, con su alabeo.

&#91;image: Vista exterior de persecucion\]

**Exterior, vista lateral**: bimotor con el motor derecho parado.

&#91;image: Vista lateral del bimotor\]

**Atardecer** hacia la sierra del este, con el VOR QLV en NAV1.

&#91;image: Atardecer hacia la sierra\]

**Noche, final ILS de la pista 31** a 3,2 km, zoom x2: luces de pista, de umbral y de aproximación; la torre da autorización para aterrizar en el teletipo.

&#91;image: Aproximacion ILS de noche\]

## Controles

El teclado se lee con KEYROW, así que se pueden pulsar varias teclas a la vez. La tabla de la matriz está en `platform.h` y hay que comprobarla en el QL real.

| Teclas | Acción |
| --- | --- |
| Cursor arriba / abajo | Morro abajo / arriba (el profundidad se queda donde lo dejas) |
| Cursor izquierda / derecha | Alabeo (vuelve al centro al soltar) |
| Z / X | Timón (si la coordinación automática está apagada; Y la conmuta) |
| ESPACIO | Centrar mandos |
| \[ / \] | Compensador |
| F1 / F2 / F3 / F4 | Gas: cortar / menos / más / a fondo |
| M / Mayús+M | Mezcla: empobrecer / enriquecer (a cero para el motor) |
| P / Mayús+P | Paso de hélice (RPM del regulador) |
| E | Arranque |
| F / Mayús+F | Flaps: bajar / subir un punto |
| G | Tren |
| . (punto) | Frenos |
| V | Vista: cabina, persecución, torre, lateral |
| 5 / 4 / 6 / 8 | Mirar al frente / izquierda / derecha / atrás |
| = / - | Zoom |
| C, N, H, A (+Mayús) | Sintonizar COM1, NAV1, NAV2, ADF |
| O / Mayús+O | OBS |
| K | Llamar a la torre |
| T / Mayús+T | Hora +1 / -1 h |
| TAB / ESC / ENTER | Pausa / salir al menú / reiniciar tras un choque; R (Mayús+R): saltar al siguiente (anterior) aeropuerto; E en el menú: escenario |

## Pasos de desarrollo

Los pasos 1 y 2 están escritos y probados en PC; el siguiente es compilarlos con `qcc` y probarlos en el QL real o en sQLux.

- [x] **1. Núcleo portable**: punto fijo, tablas, primitivas modo 4, motor 3D, escenario por teselas, aviones, modelo de vuelo y motores, radios, ATC, panel, sonido, menú.
- [x] **2. Banco de pruebas en PC**: física, ILS, ATC y 13 capturas en formato QL.
- [ ] **3. Compilación en el QL**: `make`; primero con `SOLO_C=1` (todo en C salvo las llamadas a QDOS) y después con `ql68k.s`, rutina a rutina, comparando con la versión C.
- [ ] **4. Validación en el QL**: matriz de KEYROW, reloj de 50 Hz, modo 4, protocolo de la PIA y reloj real del PSG, lectura de `flp1_demo_scn`; medir los cuadros por segundo con un contador en la línea de estado.
- [ ] **5. Optimización en ensamblador** (orden por ganancia): proyección con DIVS 32/16, paso de aristas del relleno de polígonos, horizonte artificial, `fxdiv`, texto del panel.
- [ ] **6. Escenario completo**: órdenes para pendientes/colinas, carreteras con anchura, puentes; precarga de teselas por delante del avión; varios .SCN y selector en el menú.
- [ ] **7. Entorno y realismo**: editor de entorno como el original (viento por capas, nubes, hora, estación), calefacción del carburador, magnetos, fallos de motor, efecto suelo, viento cruzado en el aterrizaje.
- [ ] **8. ATC ampliado**: rodaje, tráfico simulado, avisos de altitud, segunda frecuencia, guardar/cargar situación.
- [ ] **9. Música y efectos** con tu API de sonido (psg\_search para detectar la tarjeta y su reloj).

## Puntos a vigilar y optimizar

Lo más arriesgado es lo que no he podido probar aquí: el ensamblador y las llamadas a QDOS.

**A vigilar en el QL**

- **Ensamblador sin ensamblar**: `ql68k.s` y `qdos68k.s` están escritos en sintaxis as68 revisada contra el código de XTC68, pero no han pasado por as68. Cada rutina tiene su gemela en C (`USE_ASM_FXMUL`, `USE_ASM_GFX`, `USE_ASM_XFORM`) para aislar fallos.
- **Convenio de c68**: las rutinas guardan todo salvo d0/d1/a0/a1 y esperan los argumentos de 4 bytes en la pila. Si alguna se declara con `short`, c68 podría pasar 2 bytes: mantener los prototipos con `s32`/`int`.
- **Tarea de sondeo de 50 Hz**: hay que llamar siempre a `plat_exit()` (MT.RPOLL) antes de salir, o QDOS saltará a código liberado. Revisar que no se sale por otro camino (CTRL+C, error de c68).
- **Teclado**: la tabla KEYROW de `platform.h` es la estándar, pero como viste en Elite las pulsaciones siguen llegando a la cola de la consola. Habrá que vaciar esa cola o tener una ventana propia con el cursor activo. Con ROM Minerva, revisar como en Elite.
- **QSound**: la PIA se toma en $C2000 con tu protocolo confirmado (registro, $0F, $0A; dato, $0E, $0A). La base debería salir de la detección por SV.AYJMP que estabas analizando, y `PSG_CLOCK_HZ` (2 MHz por defecto) del reloj real del SSG del YM2203 con su divisor.
- **Pila y memoria**: `_stack` = 32 KB. El recorte de polígonos y la ordenación de caras usan arrays locales de \~1 KB.
- **Lecturas de disco**: una tesela nueva se lee dentro del cuadro y puede dar un tirón de 0,2–0,5 s en disquete.

**A optimizar, por ganancia**

1. Proyección: `muldiv` en C llama a la división de 32 bits de la librería; en ensamblador con DIVS 32/16 (cuando Z cabe en 16 bits) baja de \~600 a \~170 ciclos por coordenada.
2. Relleno de polígonos: el paso de aristas y la ordenación por fila están en C; un relleno específico para convexos (la mayoría) en ensamblador.
3. Precarga de teselas: leer la siguiente tesela en la dirección de vuelo cuando el avión pasa la mitad de la actual.
4. Horizonte artificial: 55 filas por cuadro; a 2 de cada 3 cuadros basta con redibujar si la actitud cambió menos de 1°.
5. Texto del panel: pintar glifos por bytes en lugar de píxel a píxel.
6. `fxdiv` en ensamblador con DIVS y normalización.
7. Luces lejanas: ya se submuestrean según la distancia; se puede llevar a objetos de luz por zonas.

**Comportamiento de vuelo a ajustar**: el monomotor sube a \~800–1000 ft/min, algo por encima de un avión real de 160 HP (\~700); se corrige con la eficiencia de la hélice en el .ACF. El fugoide del planeador está poco amortiguado, como en la realidad.

## Escenario Península Ibérica

El escenario ibérico ocupa 451 KB a escala 1:2 y 750 KB a escala real, así que en un disco duro de 16 MB cabe con mucho margen. He dejado la escala 1:2 por defecto porque reduce a la mitad los tiempos de vuelo y la tabla de teselas en RAM (27 KB frente a 81 KB). `make ESCALA=1.0` lo genera a tamaño real.

**Contenido**

| Elemento | Cantidad |
| --- | --- |
| Aeropuertos con ILS, torre, ATIS y VOR/DME propio | 13: Madrid, Barcelona, Palma, Ibiza, Valencia, Málaga, Sevilla, Santiago, Asturias, Lisboa, Tenerife Sur, Fuerteventura y Gran Canaria |
| VOR/DME en ruta | 20 (Zaragoza, Valladolid, Toledo, Badajoz, Albacete, Pamplona, Salamanca, Oporto, Faro, León, Murcia, Granada, Bilbao, Reus, Menorca, Lanzarote, Córdoba, Cáceres, Teruel, Burgos) |
| NDB | 9, uno en mitad del mar entre Cádiz y Canarias |
| Ciudades | 33, con luces de noche; rascacielos en Madrid y torre en Barcelona |
| Sistemas montañosos | 118 conos: Pirineos, Cantábrica, Montes de León, Central, Ibérico, Sierra Morena, Montes de Toledo, Béticas (Mulhacén 3479 m), Tramuntana, Catalanes y picos canarios (Teide 3715 m) |
| Ríos y autovías | Ebro, Duero, Tajo, Guadiana, Guadalquivir, Miño; A-1 a A-8 y A1 portuguesa |
| Mar | 2480 polígonos de agua recortados por tesela; tocar el agua da SPLASH! |

Mapa del fichero `iberia.scn` dibujado desde los datos compilados (amarillo: aeropuertos; cuadrados: VOR; círculos morados: NDB; marrón: montañas):

&#91;image: Mapa del escenario Peninsula Iberica\]

En la cabecera 32L de Madrid, con la sierra de Guadarrama al fondo:

&#91;image: Madrid cabecera 32L\]

Final a Tenerife Sur visto desde el lateral, con el Teide a la derecha y la bruma en el horizonte:

&#91;image: Tenerife Sur\]

**Escala y geografía**: las distancias van a 1:2 y las pistas, edificios y alturas a tamaño real. Canarias va desplazada como el recuadro de los mapas: queda a \~280 km de Sevilla en vez de \~1100. El mundo sigue siendo plano a cota 0, como en el original: Madrid está a 0 m en vez de 610 m.

| Ruta | Distancia en el simulador | Monomotor (110 kt) | Reactor (400 kt) |
| --- | --- | --- | --- |
| Madrid – Barcelona | 245 km | 72 min | 20 min |
| Madrid – Lisboa | 255 km | 75 min | 21 min |
| Madrid – Málaga | 214 km | 63 min | 17 min |
| Sevilla – Fuerteventura | 283 km | 83 min | 23 min |
| Barcelona – Palma | 101 km | 30 min | 8 min |
| Fuerteventura – Tenerife Sur | 118 km | 35 min | 10 min |

**Comprobado en PC**: en los 13 aeropuertos la pista queda en tierra (rodando 4 s sin SPLASH) y, a 6 km en final, el localizador, la senda y el DME funcionan. Se despega de Madrid con la sierra de Guadarrama al fondo.

**Cambios en el motor para este escenario**: visibilidad que fija el escenario (40 km), caché de 49 teselas, banda de bruma en el horizonte (el suelo más allá de la visibilidad se pinta como neblina, lo que evita ver "tierra" falsa al fondo sobre el mar), menú con selector de escenario (tecla E) y tecla R en vuelo para saltar al siguiente aeropuerto (Mayús+R, al anterior). El planeador empieza en el aire, a 600 m y 4 km del umbral.

**Aviso**: coordenadas, rumbos de pista, frecuencias e identificadores son aproximados, de memoria. Sirven para jugar, no para navegar de verdad. Están todos en `tools/iberia.py` para corregirlos.

**Para ampliarlo**: los datos están en `tools/iberia.py` como listas de lat/lon. Añadir un aeropuerto, una ciudad o una sierra es añadir una línea. `data/iberia.txt` es la versión de texto (todo salvo el mar) y `tools/mapa_scn.py` dibuja cualquier .SCN visto desde arriba para revisarlo.

## Cartas aeronáuticas para el manual

Las cartas salen de los mismos datos que el escenario (`make cartas`, `tools/cartas.py`), así que pistas, calles, radioayudas, relieve y costa coinciden con lo que se ve en el simulador.

| PDF | Formato | Páginas | Contenido |
| --- | --- | --- | --- |
| `QFS_Carta_Ruta_A3.pdf` | A3 apaisado | 1 | Carta en ruta: costa, relieve, retícula, altitudes mínimas por cuadrícula de 1°, 20 aerovías con rumbos y distancias, VOR con rosa y Morse, NDB, aeropuertos, recuadro de Canarias y leyenda |
| `QFS_Aeropuertos_A5.pdf` | A5 vertical | 39 | Por aeropuerto: plano de aeródromo (pistas, calles A/B con carteles, plataforma, torre, LOC/GP, tabla de pistas), SID y STAR con perfil ILS, MSA y frustrada |
| `QFS_Cartas_Visuales_A4.pdf` | A4 apaisado | 13 | Por aeropuerto, a 1:300.000: tintas hipsométricas, ciudades, autovías, ríos, CTR clase D, puntos N/E/S/W, pluma del ILS, frecuencias y circuito |

Para que cartas y escenario sean coherentes, el escenario ganó calles de rodaje y plataforma en cada aeropuerto, y las montañas se recortan para no invadir las pistas ni las 10 NM de la aproximación final; el generador comprueba que todas las sendas ILS libran el relieve. Las SID, STAR, aerovías, CTR y puntos de notificación son inventados para el juego, construidos sobre las radioayudas del escenario.

## Estado del código y pruebas

El código C compila sin avisos en C89 estricto (`-std=c89 -pedantic -Wall -Wextra`) y pasa las pruebas en PC con AddressSanitizer y UBSan sin errores. No se ha compilado con `qcc` ni ensamblado con as68.

| Prueba (en PC) | Resultado |
| --- | --- |
| Monomotor: despegue a fondo, rotación a 55 kt | 685 m de carrera, despega a 69 kt |
| Monomotor: subida | 790–1030 ft/min a 78–82 kt |
| Monomotor: pérdida limpio, sin motor | 46 kt IAS (VS1 del .ACF: 48) |
| Reactor: despegue con flaps 15 | 653 m, 22 s, 12,3 kN por motor |
| Bimotor: corte del motor derecho en subida | guiña 10° hacia el motor parado en 10 s |
| Planeador: planeo de 4 min | fineza media 28,7 |
| ILS a 3,2 km del umbral | localizador y senda activos, DME 2,8 NM |
| ATC con la torre | "QL NORTE TOWER, RUNWAY 31, WIND 360 AT 5, CLEARED TO LAND." |

Cómo seguir con el código:

1. `make` en el PC: genera tablas, escenario y aviones, ejecuta las pruebas y deja las capturas en `build/shots`.
2. `make disco`: deja en `build/disco` los ficheros con nombre QDOS (`demo_scn`, `mono_acf`...).
3. `make SOLO_C=1` y luego sin `SOLO_C`: ejecutable `build/qfs`.
4. En el QL: `EXEC flp1_qfs` (o `EX flp1_qfs;"win1_"` si los datos están en otro dispositivo).

El estado de cada paso y lo que falta por verificar en el QL está en `Proyecto.md`.
