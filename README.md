# QFS – Simulador de vuelo para Sinclair QL

Simulador de vuelo para el Sinclair QL (68008, 640 KB, modo 4 512x256, QSound con PSG), inspirado en
Microsoft Flight Simulator 1.0 (1982). C89 para c68/XTC68 (`qcc`) y ensamblador as68 en las partes críticas.

## Estado (24-nov-2026) – pasos 1, 2 y escenario ibérico terminados

- [x] Núcleo portable en C: punto fijo, tablas, gráficos modo 4, motor 3D, teselas, aviones, vuelo, motores,
      radios (VOR/ILS/DME/ADF/balizas), ATC, panel, sonido, menú.
- [x] Banco de pruebas en PC (`make`): física, ILS, ATC y 13 capturas en formato de pantalla QL.
- [x] Escenario Península Ibérica (`tools/iberia.py` -> `build/iberia.scn`, 451 KB a escala 1:2, 750 KB a 1:1):
      13 aeropuertos con ILS/VOR/ATIS (Madrid, Barcelona, Palma, Ibiza, Valencia, Málaga, Sevilla, Santiago,
      Asturias, Lisboa, Tenerife Sur, Fuerteventura, Gran Canaria), 20 VOR y 9 NDB en ruta, 33 ciudades,
      sistemas montañosos, ríos, autovías y mar con SPLASH. Canarias desplazada como en los mapas.
      Datos aproximados de memoria (no para navegación real).
- [x] Motor: visibilidad por escenario (40 km en Iberia), caché de 49 teselas, bruma en el horizonte,
      selector de escenario en el menú (E), tecla R para cambiar de aeropuerto, planeador sale en el aire.
- [x] Cartas aeronáuticas en PDF para el manual (`make cartas`, `tools/cartas.py`):
      carta en ruta A3 apaisado (1 pág.), plano de aeródromo + SID + STAR/ILS en A5 vertical (39 pág.),
      cartas visuales en A4 apaisado (13 pág.). Salen de los mismos datos que el escenario.
- [x] Escenario: calles de rodaje (A/B + A1..A5) y plataforma en cada aeropuerto; montañas recortadas para
      no invadir pistas ni 10 NM de aproximación final; designador de Madrid corregido (18R/36L).
- [x] A5 rehechas (`tools/procdesign.py` + `tools/cartas_a5.py`, 99 páginas): por aeropuerto plano de aeródromo,
      hoja de datos AD 2 (ARP, temperatura, pistas, distancias declaradas, perfil, luces, radioayudas,
      obstáculos, fijos, normas), una SID por cabecera con 4 rutas curvas a fijos N/E/S/W a 30 DME, e IAC:
      ILS (si lo hay) y VOR/DME para cada cabecera, con 4 IAF a 16 DME, espera, frustrada, MSA por sectores,
      perfil, mínimos A-D y tabla de régimen de descenso. Rutas elegidas para librar relieve y ciudades.
      Corredores libres de montañas ampliados (2,2 km, 12 NM) en las finales.
- [x] Compila con `qcc` y genera `build/qfs.img` y `build/qfs.win` (probado en sQLux con el `.win`).
- [ ] Validar en QL real / sQLux (lista de abajo).

## Estructura

```
src/        qfs.h fixmath.c tables.c(gen) gfx.c view3d.c world.c aircraft.c flight.c nav.c atc.c
            util.c panel.c sound.c sim.c
            ql68k.s (fxmul, hspan, line, memcpy, xform3)   qdos68k.s (KEYROW, modo 4, reloj 50 Hz)
            platform_ql.c main_ql.c (QL)   platform_host.c (PC)
tools/      libro.sh dibujos_aviones.py panel_anotado.py gentables.py mkscen.py mkaviones.py qlscr2png.py iberia.py mapa_scn.py cartas.py cartas_a5.py procdesign.py
data/       demo.txt, iberia.txt (escenarios fuente)  planeador/mono/bimotor/reactor.acf
tests/      hosttest.c iberiatest.c
doc/libro/  manual.md (fuente del libro), metadata.yaml, header.tex, cover.tex, aviso.lua, img/
Makefile    QL con qcc (por defecto: build/qfs.img y build/qfs.win)
Makefile.pc PC: datos, pruebas, capturas, disco (nombres QDOS en build/disco)
```

## Uso

```
make                         # QL completo -> build/qfs.img y build/qfs.win
make SOLO_C=1                # QL sin rutinas gráficas en ensamblador
make build/qfs               # solo el ejecutable QDOS
make -f Makefile.pc          # PC: tablas, escenario, aviones, pruebas y capturas PNG
make -f Makefile.pc ESCALA=1.0   # Iberia a tamaño real (por defecto 0.5)
make -f Makefile.pc cartas   # PDF en build/cartas (necesita reportlab y shapely:
                             #   python3 -m venv .venv && .venv/bin/pip install -r requirements.txt)
make -f Makefile.pc disco    # build/disco: demo_scn, iberia_scn, mono_acf, ...
make -f Makefile.pc libro    # manual de instrucciones A5: doc/QFS_Manual_A5.pdf (pandoc + XeLaTeX)
# en el QL: arrancar el .img/.win (boot fija D$ y lanza EXEC_W <unidad>_qfs)
```

## Pendiente de verificar en el QL (VERIFICACIÓN)

| Qué | Dónde | Cómo |
| --- | --- | --- |
| Sintaxis as68 de los .s | ql68k.s, qdos68k.s | ensamblar; nunca han pasado por as68 |
| Cada rutina asm = su versión C | USE_ASM_* | activar de una en una y comparar pantallas |
| Tabla KEYROW | platform.h | pulsar cada tecla de control |
| Pulsaciones que llegan a la consola/BASIC | platform_ql.c | vaciar la cola o ventana propia (como en Elite) |
| Reloj de 50 Hz y salida limpia (MT.RPOLL) | qdos68k.s | salir con ESC y con error |
| PIA QSound $C2000 y reloj del PSG | platform_ql.c, sound.h | oír motor/avisos; ajustar PSG_CLOCK_HZ; detectar base por SV.AYJMP |
| Lectura de flp1_demo_scn y *_acf | world.c, aircraft.c | arranque desde el menú |
| Cuadros por segundo | sim.c | añadir contador a la línea de estado |
| ROM Minerva | todo | probar como en Elite |
| Iberia: RAM con 49 teselas y 40 km de visibilidad | world.c, sim.c | memoria libre y cuadros/s sobre Madrid |
| Iberia: tirones al cargar teselas del disco duro | world.c | volar Madrid-Barcelona en reactor |

## Resultados de las pruebas en PC

- Monomotor: despegue 685 m (69 kt), subida 800–1000 ft/min, pérdida limpia a 46 kt.
- Reactor: despegue 653 m en 22 s; bimotor: guiñada de 10° en 10 s con un motor parado.
- Planeador: fineza media 28,7. ILS a 3,2 km: LOC + senda + DME 2,8 NM. ATC: autorización de aterrizaje.

## Siguientes pasos

1. Compilar y validar en el QL (tabla de arriba).
2. Optimizar: proyección con DIVS, relleno de polígonos convexos en asm, precarga de teselas, panel.
3. Escenario: cotas del terreno (Madrid a 610 m), colinas, carreteras con anchura, más aeropuertos.
4. Entorno (viento por capas, nubes, estación), fallos, carburador, magnetos.
5. ATC ampliado; música y efectos con la API de sonido (psg_search).
