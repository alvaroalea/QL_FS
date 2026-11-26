#!/usr/bin/env python3
"""teclado_ql.py - dibujo del teclado del Sinclair QL con la funcion de cada tecla en QFS (hoja A4 apaisada).

Uso: teclado_ql.py [salida.pdf]      (por omision doc/libro/img/teclado_ql.pdf; lo usa tools/libro.sh)
Cada tecla lleva su rotulo, lo que hace en vuelo, entre parentesis lo que hace con Mayus y, en azul, lo que hace en el menu inicial.
Las teclas se leen en src/sim.c (sim_keys, menu). Color = familia de la funcion.
"""
import sys
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, black, white

OUT = sys.argv[1] if len(sys.argv) > 1 else "doc/libro/img/teclado_ql.pdf"

FAM = {   # familia -> (color de fondo, nombre en la leyenda)
    "vuelo": (HexColor("#d9e8f7"), "Mandos de vuelo"),
    "motor": (HexColor("#f9dcd9"), "Motor y hélice"),
    "conf": (HexColor("#fbeccb"), "Flaps, tren y frenos"),
    "vista": (HexColor("#dcefd9"), "Vistas"),
    "radio": (HexColor("#e7dcf3"), "Radios y navegación"),
    "sim": (HexColor("#e4e4e4"), "Simulación"),
    None: (HexColor("#f7f7f7"), "Sin función en QFS"),
}
MENU = HexColor("#1d4f91")

# tecla: (rotulo, familia, [lineas de vuelo], [lineas con Mayus], [lineas del menu])
# Se escribe cada texto como lista corta de lineas ya partidas (la tecla es estrecha).
K = {}


def key(cap, fam=None, fly=(), sh=(), menu=(), w=1.0):
    return dict(cap=cap, fam=fam, fly=list(fly), sh=list(sh), menu=list(menu), w=w)


ROWS = [
    # fila superior
    (0.0, [key("ESC", "sim", ["Salir al", "menú"], menu=["Salir"]),
           key("1", None, menu=["Avión 1", "planeador"]), key("2", None, menu=["Avión 2", "monomotor"]),
           key("3", None, menu=["Avión 3", "bimotor"]),
           key("4", "vista", ["Mirar a la", "izquierda"], menu=["Avión 4", "reactor"]),
           key("5", "vista", ["Mirar al", "frente"]), key("6", "vista", ["Mirar a la", "derecha"]),
           key("7"), key("8", "vista", ["Mirar", "atrás"]), key("9"), key("0"),
           key("−", "vista", ["Zoom:", "aleja"]), key("=", "vista", ["Zoom:", "acerca"]),
           key("£"), key("\\", "sim", ["Indicador", "de cuadros", "por segundo"])]),
    (0.0, [key("TAB", "sim", ["Pausa"], w=1.4),
           key("Q"), key("W"),
           key("E", "motor", ["Arranque", "(mantener)"], menu=["Cambia de", "escenario"]),
           key("R", "sim", ["Reinicia en", "el siguiente", "aeropuerto"], ["anterior"]),
           key("T", "sim", ["Hora", "+1 h"], ["−1 h"], menu=["Hora de", "salida"]),
           key("Y", "vuelo", ["Timón", "automático", "sí / no"]),
           key("U"), key("I"),
           key("O", "radio", ["OBS del", "VOR: girar", "(mantener)"], ["al revés"]),
           key("P", "motor", ["Paso de", "la hélice:", "menos"], ["más"]),
           key("[", "vuelo", ["Compensador:", "morro abajo"]),
           key("]", "vuelo", ["Compensador:", "morro arriba"])]),
    (0.0, [key("CAPS", None, w=1.7),
           key("A", "radio", ["ADF:", "frecuencia", "arriba"], ["abajo"], menu=["Aeropuerto", "siguiente"]),
           key("S", None, menu=["Modo de", "salida"]),
           key("D", None, menu=["Pista o", "punto de", "la senda"]),
           key("F", "conf", ["Flaps:", "menos"], ["más"]),
           key("G", "conf", ["Tren de", "aterrizaje", "sube / baja"], menu=["Guardar", "partida"]),
           key("H", "radio", ["NAV2:", "frecuencia", "arriba"], ["abajo"]),
           key("J"),
           key("K", "radio", ["Llamar a", "la torre"]),
           key("L", None, menu=["Cargar", "partida"]),
           key(";"), key("'"), key("#"),
           key("ENTER", "sim", ["Reinicia tras", "un choque"], menu=["Empezar", "el vuelo"], w=1.4)]),
    (0.0, [key("MAYÚS", "sim", ["Invierte la", "función: la", "que va entre", "paréntesis"], w=1.9),
           key("Z", "vuelo", ["Timón a la", "izquierda"]),
           key("X", "vuelo", ["Timón a la", "derecha"]),
           key("C", "radio", ["COM1:", "frecuencia", "arriba"], ["abajo"]),
           key("V", "vista", ["Cambia de", "vista"]),
           key("B"),
           key("N", "radio", ["NAV1:", "frecuencia", "arriba"], ["abajo"]),
           key("M", "motor", ["Mezcla:", "empobrece"], ["enriquece"]),
           key(","),
           key(".", "conf", ["Frenos de", "las ruedas", "(mantener)"]),
           key("/"),
           key("MAYÚS", "sim", ["Invierte la", "función"], w=1.5)]),
]
BOTTOM = [key("CTRL", None, w=1.4), key("ALT", None, w=1.4),
          key("ESPACIO", "vuelo", ["Centra alerones,", "timón y profundidad"], w=6.3)]
CURS = [key("←", "vuelo", ["Alabeo a", "la izquierda"]), key("↑", "vuelo", ["Morro", "abajo"]),
        key("↓", "vuelo", ["Morro", "arriba"]), key("→", "vuelo", ["Alabeo a", "la derecha"])]
FKEYS = [key("F1", "motor", ["Gas al", "ralentí"]), key("F2", "motor", ["Menos", "gas"]),
         key("F3", "motor", ["Más", "gas"]), key("F4", "motor", ["Gas a", "fondo"]), key("F5")]


def draw_key(c, x, y, w, h, k):
    """x, y: esquina inferior izquierda"""
    bg = FAM[k["fam"]][0]
    c.setFillColor(HexColor("#b9b9b9"))
    c.roundRect(x + 0.5, y - 0.6, w, h, 2.2, stroke=0, fill=1)          # sombra
    c.setFillColor(bg)
    c.setStrokeColor(HexColor("#555555"))
    c.setLineWidth(0.7)
    c.roundRect(x, y, w, h, 2.2, stroke=1, fill=1)
    # rotulo de la tecla (arriba a la izquierda, grande)
    c.setFillColor(black)
    cap = k["cap"]
    size = 11 if len(cap) <= 2 else (7 if len(cap) <= 5 else 6)
    c.setFont("Helvetica-Bold", size)
    cb = y + h - 1.8 * mm - size * 0.8
    c.drawString(x + 1.8 * mm, cb, cap)
    # texto: vuelo (negro), Mayus (rojo, entre parentesis), menu (azul)
    fs = 5.3
    lh = fs + 1.3
    ty = cb - lh - 2.0
    for ln in k["fly"]:
        c.setFont("Helvetica", fs)
        c.setFillColor(black)
        c.drawString(x + 1.8 * mm, ty, ln)
        ty -= lh
    if k["sh"]:
        c.setFillColor(HexColor("#b3261e"))
        c.setFont("Helvetica-Bold", fs)
        c.drawString(x + 1.8 * mm, ty, "(" + " ".join(k["sh"]) + ")")
        ty -= lh
    if k["menu"]:
        c.setFillColor(MENU)
        c.setFont("Helvetica-Oblique", fs)
        yy = y + 1.6 * mm + (len(k["menu"]) - 1) * lh
        for ln in k["menu"]:
            c.drawString(x + 1.8 * mm, yy, ln)
            yy -= lh


def main():
    W, H = landscape(A4)
    c = canvas.Canvas(OUT, pagesize=(W, H))
    c.setTitle("QFS - Teclado del Sinclair QL")
    c.setAuthor("QFS")
    c.setFillColor(black)
    c.setFont("Helvetica-Bold", 17)
    c.drawString(14 * mm, H - 16 * mm, "El teclado del QL en QFS")
    lx0 = 14 * mm
    for txt, col, fn in (("Texto negro: lo que hace la tecla en vuelo.", black, "Helvetica"),
                         ("(Rojo entre paréntesis: con MAYÚS pulsada.)", HexColor("#b3261e"), "Helvetica-Bold"),
                         ("Azul cursiva: en el menú inicial.", MENU, "Helvetica-Oblique")):
        c.setFillColor(col)
        c.setFont(fn, 8.2)
        c.drawString(lx0, H - 22 * mm, txt)
        lx0 += c.stringWidth(txt, fn, 8.2) + 9 * mm
    gap = 1.6 * mm
    u = 15.7 * mm          # ancho de una tecla normal
    kh = 23.5 * mm
    fx = 8 * mm           # columna de las teclas de funcion
    mx = fx + u + 4 * mm   # inicio del bloque principal
    top = H - 30 * mm
    # F1-F5, en columna a la izquierda
    c.setFont("Helvetica-Oblique", 6.5)
    c.setFillColor(HexColor("#555555"))
    for i, k in enumerate(FKEYS):
        draw_key(c, fx, top - kh - i * (kh * 0.84 + gap), u, kh * 0.84, k)
    for r, (off, row) in enumerate(ROWS):
        x = mx + off
        y = top - kh - r * (kh + gap)
        for k in row:
            w = u * k["w"] + gap * (k["w"] - 1)
            draw_key(c, x, y, w, kh, k)
            x += w + gap
    # fila inferior: CTRL, ALT, espacio y cursores
    y = top - kh - 4 * (kh + gap)
    x = mx
    for k in BOTTOM:
        w = u * k["w"] + gap * (k["w"] - 1)
        draw_key(c, x, y, w, kh, k)
        x += w + gap
    x += 6 * mm
    for k in CURS:
        draw_key(c, x, y, u, kh, k)
        x += u + gap
    # leyenda de colores
    ly = y - 9 * mm
    lx = 14 * mm
    c.setFont("Helvetica", 7.5)
    for fam, (col, name) in FAM.items():
        if fam is None:
            continue
        c.setFillColor(col)
        c.setStrokeColor(HexColor("#555555"))
        c.setLineWidth(0.5)
        c.rect(lx, ly - 1, 6 * mm, 3.6 * mm, stroke=1, fill=1)
        c.setFillColor(black)
        c.drawString(lx + 8 * mm, ly, name)
        lx += 8 * mm + c.stringWidth(name, "Helvetica", 7.5) + 7 * mm
    c.setFillColor(HexColor("#555555"))
    c.setFont("Helvetica", 7)
    c.drawString(14 * mm, ly - 8 * mm,
                 "Las teclas de F1 a F5 están en el lateral izquierdo del QL; los cursores, a la derecha de la barra espaciadora. "
                 "El teclado se lee a la vez: puedes pulsar varias.")
    c.drawString(14 * mm, ly - 12.5 * mm,
                 "En el menú inicial T cambia la hora y, con MAYÚS, A, S y D recorren las opciones hacia atrás.")
    c.save()
    print(OUT)


main()
