#!/usr/bin/env bash
# libro_en.sh - English edition of the instruction manual (A5): doc/en/AFS_Manual_A5_en.pdf
#   source:  doc/en/libro/manual_en.md  (+ metadata.yaml, header.tex, cover.tex, aviso.lua in the same folder)
#   cover:   doc/en/Portada_en.png (the Spanish cover with its sticker redone)
# Figures: aircraft drawings and the keyboard sheet are redrawn with QFS_LANG=en; the menu screenshot comes from a host build with
# -DQFS_LANG=_en (the same switch as `make QLANG=en`); the other pictures are the Spanish ones (they hold no text).
set -euo pipefail
cd "$(dirname "$0")/.."

PY=python3; [ -x .venv/bin/python ] && PY=.venv/bin/python
OUT=${1:-doc/en/AFS_Manual_A5_en.pdf}
IMG=doc/en/libro/img
mkdir -p "$IMG" build/libro_en build/shots

command -v pandoc  >/dev/null || { echo "pandoc missing" >&2; exit 2; }
command -v xelatex >/dev/null || { echo "xelatex missing (texlive-xetex)" >&2; exit 2; }

QFS_LANG=en $PY tools/dibujos_aviones.py data "$IMG" > build/libro_en/medidas.txt
QFS_LANG=en $PY tools/teclado_ql.py "$IMG/teclado_ql_en.pdf" > /dev/null
gcc -std=c89 -DHOST -O1 -Isrc -DQFS_LANG=_en src/fixmath.c src/tables.c src/gfx.c src/world.c src/view3d.c src/aircraft.c src/flight.c \
    src/nav.c src/atc.c src/util.c src/panel.c src/map.c src/sound.c src/sim.c src/platform_host.c tests/figuras.c -lm -o build/figuras_en
./build/figuras_en
$PY tools/qlscr2png.py build/shots/fig_menu.scr "$IMG/menu.png"
$PY tools/panel_anotado.py build/shots/fig_cabina_bimotor.png "$IMG/panel_anotado.png" 2>/dev/null || true

TEXINPUTS="$PWD/doc/en:$PWD/doc/en/libro:" pandoc doc/en/libro/manual_en.md \
  --metadata-file=doc/en/libro/metadata.yaml \
  --lua-filter=doc/en/libro/aviso.lua \
  --resource-path=.:doc/en:doc/en/libro \
  --pdf-engine=xelatex \
  --include-in-header=doc/en/libro/header.tex \
  --include-before-body=doc/en/libro/cover.tex \
  --columns=30 \
  -f markdown+smart -o "$OUT"
echo "$OUT"
