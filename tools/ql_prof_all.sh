#!/bin/bash
# Perfil de varias escenas a la vez: qcc usa ficheros temporales comunes y no admite dos compilaciones en paralelo, asi que se
# compila en serie (BUILD_ONLY=1) y se ejecuta en paralelo (RUN_ONLY=1), cada escena en su directorio build/prof<n> (el nombre debe
# contener "prof": el reparto de la biblioteca de C lo busca en el mapa de enlace).
#   tools/ql_prof_all.sh etiqueta escena...      resultados en build/prof_all/<etiqueta>_<escena>.txt (y .build)
#   PF="-DPROF_PAUSE=1 -DPROF_DUMPS=3"   banderas de qcc (fisica parada: A/B sin ruido);  ND=3  volcados que espera cada ejecucion
#   Para medir el simulador entero con el planificador:  ND=1 PF="-DUSE_SCHED -DPROF_SCHED_RUN=1500" y despues
#   python3 tools/sched_parse.py build/prof<n>/perfil.txt   (hay que esperar a que aparezca la marca 241 en el perfil)
cd "$(dirname "$0")/.."
L=$1; shift
mkdir -p build/prof_all
for s in "$@"; do PROFDIR=build/prof$s BUILD_ONLY=1 PROFFLAGS="$PF" ./tools/ql_prof.sh $s > build/prof_all/${L}_$s.build 2>&1 || echo "FALLO la compilacion de la escena $s"; done
for s in "$@"; do (PROFDIR=build/prof$s RUN_ONLY=1 NDUMPS=${ND:-3} PROFFLAGS="$PF" TOP=${TOP:-45} ./tools/ql_prof.sh $s > build/prof_all/${L}_$s.txt 2>&1 &); done
echo "ejecutandose: build/prof_all/${L}_<escena>.txt (se llenan al terminar)"
