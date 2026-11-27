#!/bin/bash
# Perfil del simulador en el sQLux de pruebas (68008 a 7,5 MHz, con la contencion de la RAM interna del ZX8301):
#   tools/ql_prof.sh [escena]       (escena 0..4, ver src/prof.c; por omision 1)
# Compila una copia con -DQFS_PROF y las funciones static hechas globales (para que salgan en el mapa de enlace), crea
# build/prof/prof.win, la ejecuta en sQLux con el perfil por PC (SQLUX_PROF) y saca el informe por funcion y modulo.
# El ini lleva IPC_ROM = Hermes2_20_rom (el 8049 real: el teclado cuesta lo que en un QL).
# Se mide SIEMPRE a SPEED = 1.0 (68008 a 7,5 MHz con la contencion del ZX8301 y las interrupciones de QDOS): a toda marcha el emulador
# no cuenta esos ciclos y equivale a medir el C compilado para el host.
# Variables: ROMDIR (por omision ~/src/sQLux/build/roms), SQLUX (ejecutable de sQLux con el parche de perfil, la rama 'profile' de ~/src/sQLux por omision), TOP (funciones por volcado),
#            PROFFLAGS (banderas extra de qcc, p. ej. -DPROF_N=50), NOLIB=1 (no atribuir la biblioteca de C a quien la llama).
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
SCENE=${1:-1}
D=${PROFDIR:-build/prof}; mkdir -p $D/src
SQLUX=${SQLUX:-$R/../sQLux/build/sqlux}      # rama 'profile' de ~/src/sQLux
[ -x "$SQLUX" ] || { echo "falta sQLux con el parche de perfil: SQLUX=ruta"; exit 1; }
if [ -z "$RUN_ONLY" ]; then          # RUN_ONLY=1: no compila, usa lo que haya en $D (tools/bench_sched.sh compila en serie y ejecuta en paralelo)
rm -f $D/src/* $D/*.o $D/perfil.txt $D/qfs.MAP
[ -f build/iberia.scn ] || make -f Makefile.pc datos >/dev/null
for f in src/*.c src/*.h src/*.s; do sed -e '/^static .*(/ s/^static //' "$f" > $D/src/$(basename "$f"); done
OBJS=""
for s in main_ql sim fixmath tables gfx world view3d aircraft flight nav atc util panel map sound platform_ql ql_envsetup prof $([ -f src/bench.c ] && echo bench); do
  out=$(qcc -O -DPLAT_QL -DQFS_PROF -DPROF_SCENE=$SCENE -DUSE_ASM_FXMUL -DUSE_ASM_FXDIV -DUSE_ASM_GFX -DUSE_ASM_XFORM -DUSE_ASM_MDQ -DUSE_ASM_3D -DUSE_ASM_POLY -DUSE_ASM_XOBJ $PROFFLAGS -I$D/src -c $D/src/$s.c -o $D/$s.o 2>&1)
  [ -n "$out" ] && echo "$out"
  echo "$out" | grep -qi "error" && exit 1
  OBJS="$OBJS $s.o"
done
for s in ql68k qdos68k; do qcc -c $D/src/$s.s -o $D/$s.o || exit 1; OBJS="$OBJS $s.o"; done
(cd $D && qcc -ms -o qfs $OBJS) || exit 1
rm -f $D/prof.win
printf '100 REMark QFS perfil\n110 D$ = "win1_"\n120 MODE 4\n130 EXEC_W win1_qfs\n' > $D/boot
( echo "wr $D/boot boot"; echo "wr $D/qfs qfs"; echo "wr build/iberia.scn iberia_scn"; echo "wr data/mono.acf mono_acf"; echo "wr data/reactor.acf reactor_acf"; echo quit ) | qxltool -w $D/prof.win 4 QFS > /dev/null || exit 1
printf 'SYSROM = %s\nIPC_ROM = Hermes2_20_rom\nROMDIR = %s\nRAMTOP = %s\nSPEED = %s\nDEVICE = WIN1,%s/%s/prof.win,qdos-native\nBOOT_DEVICE = win1\nSKIP_BOOT = 1\nCPU_HOG = 1\nSOUND = 0\nHW_TRACE = %s\n' "${SYSROM:-MIN198.rom}" "${ROMDIR:-$HOME/src/sQLux/build/roms}" "${RAMTOP:-768}" 1.0 "$R" "$D" "${HW_TRACE:-0}" > $D/prof.ini
fi
[ -n "$BUILD_ONLY" ] && exit 0
# la biblioteca de C (todo lo enlazado detras de qdos68k.o) se apunta a quien la llama (productos, divisiones...)
export SQLUX_PROF_LIB=$(python3 - $D/qfs.MAP <<'PYEOF'
import sys
pos = 0; main = 0; mods = []
for l in open(sys.argv[1], errors='replace'):
    if 'TEXT=' in l:
        mods.append((l.split(':')[0].strip(), pos)); pos += int(l.split('TEXT=')[1].split()[0], 16)
    p = l.split()
    if len(p) >= 3 and p[0] == '_MAIN' and p[2] == 'TEXT': main = int(p[1].rstrip('*'), 16)
last = max(i for i, (n, _) in enumerate(mods) if 'qdos68k' in n or 'prof' in n)
lo = mods[last + 1][1] if last + 1 < len(mods) else pos
print("%d %d" % (lo - main, pos - main))
PYEOF
)
[ -n "$NOLIB" ] && unset SQLUX_PROF_LIB
export SQLUX_PROF=$R/$D/perfil.txt SQLUX_DUMP=$R/$D/dump.bin SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=offscreen
$SQLUX -f $D/prof.ini -w 1x > $D/sqlux.log 2>&1 &
PID=$!
# espera a que el programa haya volcado todos los perfiles (o 15 min)
for i in $(seq 1 900); do sleep 1; [ "$(grep -c "^END" $D/perfil.txt 2>/dev/null || echo 0)" -gt "${NDUMPS:-4}" ] && break; kill -0 $PID 2>/dev/null || break; done
# SHOT=1 (con PROFFLAGS="-DPROF_PAUSE=1 -DPROF_HOLD=1"): captura la pantalla del QL en build/prof/shot_<escena>.bin
if [ -n "$SHOT" ]; then sleep 1; rm -f $D/dump.bin; kill -USR1 $PID; sleep 1; cp $D/dump.bin $D/shot_$SCENE.bin 2>/dev/null; fi
kill $PID 2>/dev/null; wait $PID 2>/dev/null
python3 tools/ql_prof.py $D/perfil.txt $D/qfs.MAP ${TOP:-22}
