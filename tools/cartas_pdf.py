#!/usr/bin/env python3
"""cartas_pdf.py - paso 2: agrupa las cartas SVG en los PDF de doc/ y monta doc/AFS.pdf.

Uso: cartas_pdf.py [doc]            (por omision doc)
Para cada pagina de doc/svg/paginas.json se coge doc/svg-manual/<nombre>.svg si existe (retoques a mano) y si no doc/svg/<nombre>.svg.
Cada SVG se pasa a PDF con Inkscape (en build/cartas_pdf, solo si el SVG es mas nuevo que su PDF) y se unen con pdfunite:
  doc/AFS_Carta_Ruta_A3.pdf  doc/AFS_Aeropuertos_A5.pdf  doc/AFS_Cartas_Visuales_A4.pdf
  doc/AFS.pdf = manual (AFS_Manual_A5.pdf) + cartas visuales A4 + aeropuertos A5 + carta de ruta A3
"""
import json, os, subprocess, sys

doc = sys.argv[1] if len(sys.argv) > 1 else "doc"
svg, man = os.path.join(doc, "svg"), os.path.join(doc, "svg-manual")
os.makedirs(man, exist_ok=True)
tmp = "build/cartas_pdf"
os.makedirs(tmp, exist_ok=True)
pages = json.load(open(os.path.join(svg, "paginas.json")))


def source(name):
    s = os.path.join(man, name + ".svg")
    if os.path.exists(s):
        return s, "svg-manual"
    return os.path.join(svg, name + ".svg"), "svg"


res = []
todo = []
for group, name in pages:
    s, origin = source(name)
    out = os.path.join(tmp, name + ".pdf")
    res.append((group, name, out, origin))
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(s):
        todo.append((s, out))
if todo:
    # un solo Inkscape en modo shell para todos (varios a la vez se caen, y arrancar uno por pagina es lento)
    cmds = "".join("file-open:%s; export-type:pdf; export-filename:%s; export-do; file-close\n" % (s, o) for s, o in todo) + "quit\n"
    subprocess.run(["inkscape", "--shell"], input=cmds.encode(), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    missing = [o for s, o in todo if not os.path.exists(o)]
    if missing:
        sys.exit("Inkscape no genero: " + ", ".join(missing))
OUT = {"A3": "AFS_Carta_Ruta_A3.pdf", "A5": "AFS_Aeropuertos_A5.pdf", "A4": "AFS_Cartas_Visuales_A4.pdf"}
for group, fn in OUT.items():
    files = [r[2] for r in res if r[0] == group]
    if len(files) == 1:
        subprocess.run(["cp", files[0], os.path.join(doc, fn)], check=True)
    else:
        subprocess.run(["pdfunite"] + files + [os.path.join(doc, fn)], check=True)
    print("%s: %d paginas" % (fn, len(files)))
used = [r[1] for r in res if r[3] == "svg-manual"]
if used:
    print("retocadas a mano (doc/svg-manual):", ", ".join(used))
manual = os.path.join(doc, "AFS_Manual_A5.pdf")
parts = [manual] if os.path.exists(manual) else []
parts += [os.path.join(doc, OUT[g]) for g in ("A4", "A5", "A3")]
subprocess.run(["pdfunite"] + parts + [os.path.join(doc, "AFS.pdf")], check=True)
print("doc/AFS.pdf: " + " + ".join(os.path.basename(p) for p in parts))
