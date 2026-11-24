# QFS - compilacion cruzada para el Sinclair QL con XTC68 (qcc + as68)
#   make -f Makefile.ql            -> build/qfs (ejecutable QDOS)
#   make -f Makefile.ql SOLO_C=1   -> sin ensamblador (para aislar fallos de ql68k.s)
#
# Requiere: qcc en el PATH y las cabeceras/librerias de C68 en
# /usr/local/share/qdos (como en el port de Elite).

CC      = qcc
ASMDEFS = -DUSE_ASM_FXMUL -DUSE_ASM_GFX -DUSE_ASM_XFORM
ifdef SOLO_C
ASMDEFS =
ASMOBJ  = src/qdos68k.o
else
ASMOBJ  = src/ql68k.o src/qdos68k.o
endif
CFLAGS  = -O $(ASMDEFS)

SRC     = src/main_ql.c src/sim.c src/fixmath.c src/tables.c src/gfx.c \
          src/world.c src/view3d.c src/aircraft.c src/flight.c src/nav.c \
          src/atc.c src/util.c src/panel.c src/sound.c src/platform_ql.c
OBJ     = $(SRC:.c=.o) $(ASMOBJ)

build/qfs: $(OBJ)
	@mkdir -p build
	$(CC) -o build/qfs $(OBJ)

src/%.o: src/%.c src/*.h
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.s
	$(CC) -c $< -o $@

clean:
	rm -f src/*.o build/qfs

.PHONY: clean
