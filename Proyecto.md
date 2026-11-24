# QFS – Simulador de vuelo para Sinclair QL

Simulador de vuelo para el Sinclair QL (68008, 640 KB, modo 4 512x256, QSound con PSG), inspirado en
Microsoft Flight Simulator 1.0 (1982). C89 para c68/XTC68 (`qcc`) y ensamblador as68 en las partes críticas.

## Estado (24-nov-2026) – paso 1 y 2 terminados

- [x] Núcleo portable en C: punto fijo, tablas, gráficos modo 4, motor 3D, teselas, aviones, vuelo, motores,
      radios (VOR/ILS/DME/ADF/balizas), ATC, panel, sonido, menú.
- [x] Banco de pruebas en PC (`make`): física, ILS, ATC y 13 capturas en formato de pantalla QL.
- [ ] Compilar con `qcc` (`make -f Makefile.ql SOLO_C=1`, después con ensamblador).
- [ ] Validar en QL real / sQLux (lista de abajo).

## Estructura

```
src/        qfs.h fixmath.c tables.c(gen) gfx.c view3d.c world.c aircraft.c flight.c nav.c atc.c
            util.c panel.c sound.c sim.c
            ql68k.s (fxmul, hspan, line, memcpy, xform3)   qdos68k.s (KEYROW, modo 4, reloj 50 Hz)
            platform_ql.c main_ql.c (QL)   platform_host.c (PC)
tools/      gentables.py mkscen.py mkaviones.py qlscr2png.py
data/       demo.txt (escenario fuente)  planeador/mono/bimotor/reactor.acf
tests/      hosttest.c
Makefile    PC: datos, pruebas, capturas, disco (nombres QDOS en build/disco)
Makefile.ql QL con qcc
```

## Uso

```
make                         # PC: tablas, escenario, aviones, pruebas y capturas PNG
make disco                   # build/disco: demo_scn, mono_acf, ...
make -f Makefile.ql SOLO_C=1 # QL sin rutinas gráficas en ensamblador
make -f Makefile.ql          # QL completo -> build/qfs
EXEC flp1_qfs                # en el QL (argumento opcional: dispositivo de datos, "win1_")
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

## Resultados de las pruebas en PC

- Monomotor: despegue 685 m (69 kt), subida 800–1000 ft/min, pérdida limpia a 46 kt.
- Reactor: despegue 653 m en 22 s; bimotor: guiñada de 10° en 10 s con un motor parado.
- Planeador: fineza media 28,7. ILS a 3,2 km: LOC + senda + DME 2,8 NM. ATC: autorización de aterrizaje.

## Siguientes pasos

1. Compilar y validar en el QL (tabla de arriba).
2. Optimizar: proyección con DIVS, relleno de polígonos convexos en asm, precarga de teselas, panel.
3. Escenario: colinas, carreteras con anchura, varios .SCN, selector en el menú.
4. Entorno (viento por capas, nubes, estación), fallos, carburador, magnetos.
5. ATC ampliado; música y efectos con la API de sonido (psg_search).
