#!/usr/bin/env python3
"""cartas_svg.py - paso 1: trocea los PDF de cartas (build/cartas) en una pagina por fichero SVG en doc/svg.

Uso: cartas_svg.py [carpeta_con_los_pdf] [carpeta_de_salida]     (por omision build/cartas  doc/svg)
Cada pagina se llama como dice build/cartas/paginas.json (LEMD_a5_03_sid_32L.svg, LEMD_vac_a4.svg, ruta_a3.svg...). La lista, con el
grupo (A3/A4/A5) y el orden, se copia a doc/svg/paginas.json para el paso 2 (tools/cartas_pdf.py). Si quieres retocar una carta,
copia su SVG a doc/svg-manual con el mismo nombre y edita la copia: el paso 2 usa la de doc/svg-manual si existe.
"""
import json, os, subprocess, sys, tempfile

src = sys.argv[1] if len(sys.argv) > 1 else "build/cartas"
out = sys.argv[2] if len(sys.argv) > 2 else "doc/svg"
FILES = {"A3": "AFS_Carta_Ruta_A3.pdf", "A5": "AFS_Aeropuertos_A5.pdf", "A4": "AFS_Cartas_Visuales_A4.pdf"}
pages = json.load(open(os.path.join(src, "paginas.json")))
os.makedirs(out, exist_ok=True)
idx = {}
keep = set()
tmp = tempfile.mkdtemp()
for group, name in pages:
    idx[group] = idx.get(group, 0) + 1
    one = os.path.join(tmp, name + ".pdf")
    subprocess.run(["pdfseparate", "-f", str(idx[group]), "-l", str(idx[group]), os.path.join(src, FILES[group]), one], check=True)
    subprocess.run(["pdftocairo", "-svg", one, os.path.join(out, name + ".svg")], check=True)
    os.remove(one)
    keep.add(name + ".svg")
for f in os.listdir(out):                      # fuera las de cartas que ya no existen
    if f.endswith(".svg") and f not in keep:
        os.remove(os.path.join(out, f))
json.dump(pages, open(os.path.join(out, "paginas.json"), "w"), indent=0)
print("%d SVG en %s" % (len(pages), out))
