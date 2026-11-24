#!/usr/bin/env bash
# libro.sh - genera el manual de instrucciones de QFS (A5) en PDF: doc/QFS_Manual_A5.pdf
#   fuente:  doc/libro/manual.md  (+ metadata.yaml, header.tex, cover.tex, aviso.lua)
#   portada: doc/Portada.png (a pagina completa)
# Las ilustraciones salen de los datos del juego: dibujos de los aviones (data/*.acf) y panel numerado
# (capturas de build/shots, que crea `make -f Makefile.pc prueba`).
#
# Debian/Ubuntu:  sudo apt install pandoc texlive-xetex texlive-latex-recommended texlive-latex-extra \
#                                  texlive-fonts-recommended texlive-lang-spanish fonts-dejavu
#                 pip install matplotlib pillow
set -euo pipefail
cd "$(dirname "$0")/.."

PY=python3; [ -x .venv/bin/python ] && PY=.venv/bin/python
OUT=${1:-doc/QFS_Manual_A5.pdf}
IMG=doc/libro/img
mkdir -p "$IMG" build/libro

faltan=""
command -v pandoc  >/dev/null || faltan="$faltan pandoc"
command -v xelatex >/dev/null || faltan="$faltan texlive-xetex texlive-latex-extra texlive-lang-spanish"
if [ -n "$faltan" ]; then echo "Faltan programas: sudo apt install$faltan fonts-dejavu" >&2; exit 2; fi

# 1. ilustraciones
$PY tools/dibujos_aviones.py data "$IMG" > build/libro/medidas.txt
for f in 04_exterior_persecucion; do
  [ -f build/shots/$f.png ] && cp build/shots/$f.png "$IMG/$f.png"
done
[ -f build/shots/ib01_madrid_cabecera.png ] && $PY tools/panel_anotado.py build/shots/ib01_madrid_cabecera.png "$IMG/panel_anotado.png"

# 2. PDF (la portada Portada.png se busca en doc/)
TEXINPUTS="$PWD/doc:$PWD/doc/libro:" pandoc doc/libro/manual.md \
  --metadata-file=doc/libro/metadata.yaml \
  --lua-filter=doc/libro/aviso.lua \
  --resource-path=.:doc:doc/libro \
  --pdf-engine=xelatex \
  --include-in-header=doc/libro/header.tex \
  --include-before-body=doc/libro/cover.tex \
  --columns=30 \
  -f markdown+smart -o "$OUT"
echo "$OUT"
