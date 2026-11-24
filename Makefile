# QFS - Makefile del PC: datos, pruebas y capturas (no genera el binario del QL)
#   make datos     -> tablas, escenario de demo y aviones
#   make prueba    -> compila y ejecuta las pruebas de fisica/navegacion/render
#   make capturas  -> pasa las capturas de pantalla QL a PNG
# El binario del QL se compila con Makefile.ql (qcc de XTC68).

PY      = python3
CC      = gcc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -Wno-unused-parameter -DHOST -O1 -g

COMUN   = src/fixmath.c src/tables.c src/gfx.c src/world.c src/view3d.c \
          src/aircraft.c src/flight.c src/nav.c src/atc.c src/util.c \
          src/panel.c src/sound.c src/sim.c

all: datos prueba capturas

src/tables.c: tools/gentables.py
	$(PY) tools/gentables.py src/tables.c

datos: src/tables.c build/demo.scn data/mono.acf

build/demo.scn: data/demo.txt tools/mkscen.py
	@mkdir -p build
	$(PY) tools/mkscen.py data/demo.txt build/demo.scn

data/mono.acf: tools/mkaviones.py
	$(PY) tools/mkaviones.py data

build/hosttest: $(COMUN) src/platform_host.c tests/hosttest.c src/*.h
	$(CC) $(CFLAGS) $(COMUN) src/platform_host.c tests/hosttest.c -lm -o $@

prueba: datos build/hosttest
	@mkdir -p build/shots
	./build/hosttest

capturas:
	for f in build/shots/*.scr; do $(PY) tools/qlscr2png.py $$f $${f%.scr}.png; done

# disco para el QL: nombres QDOS (punto -> subrayado)
disco: datos
	@mkdir -p build/disco
	cp build/demo.scn build/disco/demo_scn
	for f in data/*.acf; do b=$$(basename $$f .acf); cp $$f build/disco/$${b}_acf; done

clean:
	rm -rf build/hosttest build/shots build/disco

.PHONY: all datos prueba capturas disco clean
