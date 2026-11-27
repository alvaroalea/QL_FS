#!/bin/bash
# Banco de pruebas de interrupciones en sQLux (doc/interrupciones.md):  tools/ql_bench.sh <modo> <salida> [escena] [banderas extra de qcc]
#   modo: BENCH_MODE de src/bench.c (1 costes, 2..5 perfiles, 6 unidades de la vista, 10 planificador con -DB_PAT=\"FNNNFNNN\" -DB_RUN=1000)
#   salida: prefijo; deja <salida>.perfil (marcas y perfil crudo) y <salida>.txt (informe por funcion)
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
MODE=$1; OUT=$2; SCENE=${3:-1}; shift 3
PD=build/prof_$$; mkdir -p $PD
PROFDIR=$PD PROFFLAGS="-DBENCH_MODE=$MODE $*" NDUMPS=1 ./tools/ql_prof.sh $SCENE > $OUT.txt 2>&1
cp $PD/perfil.txt $OUT.perfil
rm -rf $PD
python3 tools/bench_parse.py $OUT.perfil
