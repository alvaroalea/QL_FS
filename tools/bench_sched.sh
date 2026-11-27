#!/bin/bash
# Lanza los experimentos del planificador por turnos (doc/interrupciones.md): compila uno a uno (qcc usa ficheros temporales
# comunes, no admite compilaciones en paralelo) y ejecuta de 3 en 3 en sQLux.
#   tools/bench_sched.sh <directorio de salida> id:patron:isr[:banderas] ...      (RUN = turnos de 20 ms que dura cada prueba)
OUTD=$1; shift; mkdir -p $OUTD
R=$(cd "$(dirname "$0")/.." && pwd); cd $R
for e in "$@"; do
  IFS=: read id pat isr extra <<< "$e"
  PD=build/prof_$id; mkdir -p $PD
  PROFDIR=$PD BUILD_ONLY=1 PROFFLAGS="-DBENCH_MODE=10 -DB_PATX=$pat -DB_ISR=$isr -DB_RUN=${RUN:-1000} $extra" ./tools/ql_prof.sh 1 > $OUTD/$id.build 2>&1 || echo "FALLO de compilacion en $id"
done
run() {
  PD=build/prof_$1
  PROFDIR=$PD RUN_ONLY=1 NDUMPS=1 ./tools/ql_prof.sh 1 > $OUTD/$1.txt 2>&1
  cp $PD/perfil.txt $OUTD/$1.perfil; python3 tools/bench_parse.py $OUTD/$1.perfil > $OUTD/$1.parsed; rm -rf $PD
}
export -f run; export OUTD
for e in "$@"; do echo "${e%%:*}"; done | xargs -P 3 -I{} bash -c 'run {}'
