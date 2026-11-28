# AFS - compilacion cruzada para el Sinclair QL con XTC68 (qcc + as68)
#   make            -> build/afs.img y build/afs.win (por defecto)
#   make build/afs  -> solo el ejecutable QDOS
#   make SOLO_C=1   -> sin ensamblador (para aislar fallos de ql68k.s)
#   make SCHED=0    -> bucle sin planificador de interrupciones (por omision SCHED=1)
#
# Requiere: qcc en el PATH y las cabeceras/librerias de C68 en
# /usr/local/share/qdos (como en el port de Elite).

# Objetivo por defecto: las dos imagenes (build/afs.img y build/afs.win)
all: img win

CC      = qcc
ASMDEFS = -DUSE_ASM_FXMUL -DUSE_ASM_FXDIV -DUSE_ASM_GFX -DUSE_ASM_XFORM -DUSE_ASM_MDQ -DUSE_ASM_3D -DUSE_ASM_POLY -DUSE_ASM_XOBJ
ifdef SOLO_C
ASMDEFS =
ASMOBJ  = src/qdos68k.o
else
ASMOBJ  = src/ql68k.o src/qdos68k.o
endif
# SCHED=1 (por omision): fisica a 12,5 Hz dentro de la interrupcion de 50 Hz y el resto por turnos (doc/optimizacion.md, «Planificador por turnos»);
# make SCHED=0: el bucle de siempre (cuadros seguidos, fisica a 25 Hz por recuperacion de tiempo)
SCHED ?= 1
ifeq ($(SCHED),1)
ASMDEFS += -DUSE_SCHED
endif
# idioma de los textos del menu (src/lang.h): make QLANG=en  (por omision espanol). Si cambia, se recompila el codigo que usa los textos
QLANG    ?=
QLANGDEF := $(if $(filter-out es,$(QLANG)),-DQFS_LANG=_$(QLANG))
$(shell if [ "$$(cat build/.lang 2>/dev/null)" != "$(QLANG)" ]; then mkdir -p build; rm -f src/sim.o; echo "$(QLANG)" > build/.lang; fi)
CFLAGS  = -O -Isrc -DPLAT_QL $(ASMDEFS) $(QLANGDEF)

lang-check:
	@sh tools/lang_check.sh $(or $(QLANG),en)

SRC     = src/main_ql.c src/sim.c src/fixmath.c src/tables.c src/gfx.c \
          src/world.c src/view3d.c src/aircraft.c src/flight.c src/nav.c \
          src/atc.c src/util.c src/panel.c src/map.c src/sound.c src/platform_ql.c src/ql_envsetup.c
OBJ     = $(SRC:.c=.o) $(ASMOBJ)

build/afs: $(OBJ)
	@mkdir -p build
	$(CC) -o build/afs $(OBJ)

src/%.o: src/%.c src/*.h
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.s
	$(CC) -c $< -o $@

# ---------------------------------------------------------------- imagenes de disco ----
#   make img   -> build/afs.img (disquete DD 720 KB, arranca desde flp1_)
#   make win   -> build/afs.win (disco duro QXL.WIN, arranca desde win1_)
#   make disk  -> las dos
# Los datos salen de la raiz del proyecto (Makefile.pc: make -f Makefile.pc datos); en QDOS el punto es subrayado
# (demo.scn -> demo_scn) y qltools/qxltool lo traducen solo. No se pasa texto tras EXEC_W (cuelga sin Toolkit II):
# la unidad de datos va en la variable D$ de SuperBASIC, que lee main_ql.c.
QLTOOLS ?= qltools
QXLTOOL ?= qxltool
WINMB   ?= 4
DATOS    = build/demo.scn build/iberia.scn data/planeador.acf data/mono.acf data/bimotor.acf data/reactor.acf \
          data/planeador.src data/mono.src data/bimotor.src data/reactor.src

DISCOFILES = boot afs demo_scn iberia_scn planeador_acf mono_acf bimotor_acf reactor_acf planeador_src mono_src bimotor_src reactor_src

$(DATOS):
	$(MAKE) -f Makefile.pc datos

build/boot_flp1 build/boot_win1: Makefile
	@mkdir -p build
	printf '100 REMark AFS - simulador de vuelo\n110 D$$ = "$(patsubst build/boot_%,%,$@)_"\n120 MODE 4\n130 EXEC_W $(patsubst build/boot_%,%,$@)_afs\n' > $@

# qltools/qxltool usan el nombre del fichero sin directorio: se prepara una carpeta con los nombres de disco
build/disco/afs: build/afs $(DATOS)
	@mkdir -p build/disco
	cp build/afs build/disco/afs
	for f in $(DATOS); do b=$$(basename $$f); cp $$f build/disco/$${b%.*}_$${b##*.}; done

build/afs.img: build/disco/afs build/boot_flp1
	rm -f $@
	$(QLTOOLS) $@ -fdd AFS > /dev/null
	cp build/boot_flp1 build/disco/boot
	cd build/disco && $(QLTOOLS) ../afs.img -W $(DISCOFILES) > /dev/null
	@echo "$@:"; $(QLTOOLS) $@ -d

build/afs.win: build/disco/afs build/boot_win1
	rm -f $@
	cp build/boot_win1 build/disco/boot
	( for f in $(DISCOFILES); do echo "wr build/disco/$$f $$f"; done; \
	  echo ls; echo quit ) | $(QXLTOOL) -w $@ $(WINMB) AFS | tail -12

img: build/afs.img
win: build/afs.win
disk: img win

# perfil en sQLux (68008 a 7,5 MHz, con la contencion del ZX8301): make prof ESCENA=1  (ver tools/ql_prof.sh y doc/optimizacion.md)
ESCENA ?= 1
prof:
	./tools/ql_prof.sh $(ESCENA)

clean:
	rm -f src/*.o build/afs build/afs.img build/afs.win build/boot_flp1 build/boot_win1

.PHONY: all clean img win disk prof
