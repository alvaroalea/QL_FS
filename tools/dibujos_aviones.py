#!/usr/bin/env python3
"""dibujos_aviones.py - tres vistas (planta, perfil, frente) de cada avion a partir de su .acf.

Dibuja el MISMO modelo exterior que usa el simulador (lineas V/F/L del .acf), asi que el libro y el
juego coinciden.  Salida: doc/libro/img/avion_<nombre>.png  (y las medidas, por la salida estandar, que
usa el libro: envergadura, longitud y altura sacadas del modelo).

Uso: python3 tools/dibujos_aviones.py [directorio_data] [directorio_salida]
"""
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon

# indices de color del modelo (ver tools/mkaviones.py) -> tinta de imprenta
COLORES = {0: "#222222", 1: "#c8372d", 3: "#ffffff", 4: "#9a9a9a", 7: "#e08a2e", 10: "#d4d4d4"}
AVIONES = ["planeador", "mono", "bimotor", "reactor"]


def leer(ruta):
    v, f, l = [], [], []
    for linea in open(ruta, encoding="latin-1"):
        linea = linea.split(";")[0].strip()
        if linea.startswith("V ="):
            v.append(tuple(float(x) for x in linea[3:].split(",")))
        elif linea.startswith("F ="):
            n = [int(x) for x in linea[3:].split(",")]
            f.append((n[0], n[1], n[2:]))
        elif linea.startswith("L ="):
            n = [int(x) for x in linea[3:].split(",")]
            l.append((n[0], n[1:]))
    return v, f, l


def medidas(v):
    xs, ys, zs = zip(*v)
    return max(xs) - min(xs), max(zs) - min(zs), max(ys)


def vista(ax, v, f, l, ejes, orden, titulo):
    """ejes = (indice del eje horizontal, indice del eje vertical, signo horizontal)."""
    h, w, s = ejes
    pr = lambda i: (s * v[i][h], v[i][w])
    caras = sorted(f, key=lambda c: sum(v[i][orden] for i in c[2]) / len(c[2]))
    for relleno, borde, idx in caras:
        ax.add_patch(Polygon([pr(i) for i in idx], closed=True, facecolor=COLORES.get(relleno, "#ffffff"),
                             edgecolor="#111111", linewidth=0.7, joinstyle="round"))
    for col, (a, b) in l:
        pa, pb = pr(a), pr(b)
        ax.plot([pa[0], pb[0]], [pa[1], pb[1]], color="#111111", linewidth=1.1, solid_capstyle="round")
    ax.set_aspect("equal")
    ax.autoscale_view()
    ax.axis("off")
    ax.set_title(titulo, fontsize=8, pad=3, color="#333333")


def dibujar(ruta, salida):
    v, f, l = leer(ruta)
    envergadura, longitud, altura = medidas(v)
    fig = plt.figure(figsize=(5.2, 3.3), dpi=220)
    gs = fig.add_gridspec(2, 2, width_ratios=[1, 1.35], height_ratios=[1.5, 1], hspace=0.08, wspace=0.04)
    # planta (morro a la derecha): horizontal = z, vertical = x
    a1 = fig.add_subplot(gs[:, 0])
    vista(a1, v, f, l, (2, 0, 1), 1, "PLANTA")
    a2 = fig.add_subplot(gs[0, 1])
    vista(a2, v, f, l, (2, 1, 1), 0, "PERFIL")
    a3 = fig.add_subplot(gs[1, 1])
    vista(a3, v, f, l, (0, 1, 1), 2, "FRENTE")
    fig.savefig(salida, bbox_inches="tight", pad_inches=0.04, facecolor="white")
    plt.close(fig)
    return envergadura, longitud, altura


def main():
    datos = sys.argv[1] if len(sys.argv) > 1 else "data"
    sal = sys.argv[2] if len(sys.argv) > 2 else "doc/libro/img"
    os.makedirs(sal, exist_ok=True)
    for n in AVIONES:
        e, lo, a = dibujar(os.path.join(datos, n + ".acf"), os.path.join(sal, "avion_%s.png" % n))
        print("%-10s envergadura %5.1f m  longitud %5.1f m  altura %4.1f m" % (n, e, lo, a))


if __name__ == "__main__":
    main()
