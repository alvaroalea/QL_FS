#!/usr/bin/env python3
"""fijos.py - genera el listado de puntos de notificacion de cada aeropuerto: data/fijos.txt

Uso: fijos.py [salida]        (por omision data/fijos.txt; si ya existe NO se sobrescribe, se pide --forzar)
Cada aeropuerto tiene 4 fijos de salida (SID) a 30 DME de su VOR (N, E, S, W) y 4 IAF de aproximacion a 16 DME. El listado fija su
nombre y su posicion (radial y DME del VOR del aeropuerto, mas las coordenadas del simulador y las geograficas aproximadas) y
tools/procdesign.py los usa tal cual en todas las SID y aproximaciones del aeropuerto: un fijo es siempre el mismo punto.
Para cambiar uno, edita el listado (radial / DME) y regenera las cartas."""
import os, sys

args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "fijos.txt")
if os.path.exists(out) and "--forzar" not in sys.argv:
    sys.exit("%s ya existe: es el listado vigente (usa --forzar para regenerarlo con otros nombres)" % out)
sys.argv = [sys.argv[0]]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cartas as C
from procdesign import Designer, polar, NM

des = Designer(C.CONES, C.CITIES, C.APTS, C.NAVS, ground=C.ground_m)
lines = ["; Puntos de notificacion de QFS (generado por tools/fijos.py; editable a mano).",
         "; ICAO  FIJO   USO  CARD  RADIAL  DME    X(m)    Y(m)    LAT       LON   (radial y DME respecto al VOR del aeropuerto)"]
for L in C.APTS:
    vpos = L["vor"][2]
    for use, dme in (("SID", 30), ("IAF", 16)):
        for cd, b in (("N", 0), ("E", 90), ("S", 180), ("W", 270)):
            p = polar(vpos, b, dme * NM)
            la, lo = C.inv(p[0], p[1], C.is_canary(L["arp"]))
            lines.append("%-4s %-5s %-3s %-2s %6.1f %5.1f %9.0f %9.0f %8.3f %8.3f" % (L["icao"], des.name(), use, cd, b, dme, p[0], p[1], la, lo))
open(out, "w").write("\n".join(lines) + "\n")
print("%s: %d fijos" % (out, len(lines) - 2))
