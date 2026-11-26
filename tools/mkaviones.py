#!/usr/bin/env python3
"""mkaviones.py - genera los ficheros de avion de ejemplo (data/*.acf).
Los .acf son texto: se pueden editar a mano. Este script solo evita escribir
a mano los vertices de los modelos exteriores (ejes: x dcha, y arriba, z adelante,
metros, y = 0 en las ruedas)."""
import sys, os

BLANCO, ROJO, GRIS, NEGRO, GRISCLARO, NARANJA = 3, 1, 4, 0, 10, 7
C_NEGRO, C_ROJO, C_BLANCO = 0, 1, 3


class M:
    def __init__(s):
        s.v, s.f = [], []

    def vert(s, x, y, z):
        s.v.append((x, y, z)); return len(s.v) - 1

    def face(s, fill, out, *idx):
        s.f.append(("F", fill, out, idx))

    def line(s, col, a, b):
        s.f.append(("L", col, None, (a, b)))

    def fus(s, y0, nose, zf, zr, tail, w, h, fill=BLANCO):
        n = s.vert(0, y0 + h * 0.45, nose)
        a = [s.vert(-w, y0 + h, zf), s.vert(w, y0 + h, zf), s.vert(w, y0, zf), s.vert(-w, y0, zf)]
        b = [s.vert(-w * .8, y0 + h * .95, zr), s.vert(w * .8, y0 + h * .95, zr),
             s.vert(w * .8, y0 + h * .15, zr), s.vert(-w * .8, y0 + h * .15, zr)]
        t = s.vert(0, y0 + h * 0.7, tail)
        for i in range(4):
            j = (i + 1) % 4
            s.face(fill, C_NEGRO, n, a[i], a[j])
            s.face(fill, C_NEGRO, a[i], a[j], b[j], b[i])
            s.face(fill, C_NEGRO, b[i], b[j], t)
        return t

    def wing(s, y, span, zf, zr, tip_off=0.0, fill=ROJO, root=0.0):
        p = [s.vert(-span, y, zf - tip_off), s.vert(span, y, zf - tip_off),
             s.vert(span, y, zr - tip_off * 0.5), s.vert(-span, y, zr - tip_off * 0.5)]
        s.face(fill, C_NEGRO, *p)

    def fin(s, y0, y1, zf, zr, ztop, fill=ROJO):
        s.face(fill, C_NEGRO, s.vert(0, y0, zf), s.vert(0, y1, ztop), s.vert(0, y1, ztop - 0.8), s.vert(0, y0, zr))

    def text(s):
        out = []
        for v in s.v:
            out.append("V = %.2f, %.2f, %.2f" % v)
        for kind, a, b, idx in s.f:
            if kind == "F":
                out.append("F = %d, %d, %s" % (a, b, ", ".join(map(str, idx))))
            else:
                out.append("L = %d, %s" % (a, ", ".join(map(str, idx))))
        return "\n".join(out)


def gear(m, y, zmain, znose, half):
    for x in (-half, half):
        m.line(C_NEGRO, m.vert(x * .6, y, zmain), m.vert(x, 0, zmain))
    m.line(C_NEGRO, m.vert(0, y, znose), m.vert(0, 0, znose))


def mono():
    m = M()
    m.fus(0.7, 4.0, 1.8, -1.0, -5.2, 0.6, 1.3)
    m.wing(2.05, 5.5, 1.5, 0.0)
    m.wing(1.05, 1.8, -4.3, -5.1)
    m.fin(1.2, 2.6, -3.9, -5.2, -4.8)
    m.line(C_NEGRO, m.vert(-0.9, 1.3, 4.1), m.vert(0.9, 1.3, 4.1))
    gear(m, 0.7, 0.9, 3.0, 1.2)
    return m


def bimotor():
    m = M()
    m.fus(0.8, 4.5, 2.2, -1.2, -5.8, 0.65, 1.4)
    m.wing(0.95, 5.8, 1.6, -0.1)
    for x in (-2.4, 2.4):           # gondolas
        a = m.vert(x, 1.0, 2.6); b = m.vert(x - .45, 1.3, 1.0); c = m.vert(x + .45, 1.3, 1.0)
        d = m.vert(x, 0.7, 1.0); e = m.vert(x, 1.0, -1.0)
        m.face(GRISCLARO, C_NEGRO, a, b, c); m.face(GRISCLARO, C_NEGRO, a, c, d); m.face(GRISCLARO, C_NEGRO, a, d, b)
        m.face(GRISCLARO, C_NEGRO, b, c, e); m.face(GRISCLARO, C_NEGRO, c, d, e); m.face(GRISCLARO, C_NEGRO, d, b, e)
        m.line(C_NEGRO, m.vert(x - .9, 1.1, 2.7), m.vert(x + .9, 1.1, 2.7))
    m.wing(1.3, 2.2, -4.7, -5.6)
    m.fin(1.3, 3.0, -4.2, -5.8, -5.3)
    gear(m, 0.8, 0.8, 3.4, 2.4)
    return m


def reactor():
    m = M()
    m.fus(0.9, 7.5, 4.0, -3.0, -8.5, 0.9, 1.9, fill=BLANCO)
    p = [m.vert(-7.5, 1.0, -2.6), m.vert(7.5, 1.0, -2.6), m.vert(1.0, 1.0, 1.4), m.vert(-1.0, 1.0, 1.4)]
    m.face(GRIS, C_NEGRO, p[0], p[3], p[2], p[1])
    for x in (-1.6, 1.6):           # motores en cola
        a = m.vert(x, 2.3, -3.2); b = m.vert(x, 2.3, -5.6); c = m.vert(x * 1.25, 1.8, -5.6); d = m.vert(x * 1.25, 1.8, -3.2)
        m.face(GRISCLARO, C_NEGRO, a, b, c, d)
    m.fin(2.4, 5.0, -6.0, -8.6, -8.6)
    t = [m.vert(-3.0, 5.0, -7.6), m.vert(3.0, 5.0, -7.6), m.vert(3.0, 5.0, -8.6), m.vert(-3.0, 5.0, -8.6)]
    m.face(ROJO, C_NEGRO, *t)
    gear(m, 0.9, -1.5, 5.0, 1.9)
    return m


def planeador():
    m = M()
    m.fus(0.4, 3.2, 1.6, -0.5, -6.5, 0.4, 0.9)
    m.wing(1.25, 8.5, 1.1, 0.1, fill=BLANCO)
    m.wing(1.75, 1.4, -6.0, -6.6, fill=ROJO)
    m.fin(0.8, 1.8, -5.6, -6.6, -6.3)
    m.line(C_NEGRO, m.vert(0, 0.4, 0.5), m.vert(0, 0, 0.5))
    return m


AVIONES = {
"planeador.acf": ("""NOMBRE = PLANEADOR
MOTOR = NINGUNO
MOTORES = 0
TREN = FIJO
MASA = 420
SUPERFICIE = 13.0
CL0 = 0.40
CLA = 5.5
ALFA_PERDIDA = 13
CD0 = 0.012
K = 0.020
FLAPS = 0, 1          ; aerofrenos
DCL_FLAP = -0.15
DCD_FLAP = 0.080
DALFA_FLAP = 0
ALABEO = 35
PROFUNDIDAD = 11
VREF = 30
VS0 = 36
VS1 = 38
VFE = 110
VNO = 100
VNE = 135
TECHO = 6000
ALTURA_OJO = 1.0
ACTITUD_SUELO = 2
""", planeador),
"mono.acf": ("""NOMBRE = MONOMOTOR 160HP
MOTOR = PISTON
MOTORES = 1
HELICE = FIJA
TREN = FIJO
MASA = 1000
COMBUSTIBLE = 200
SUPERFICIE = 16.2
CL0 = 0.30
CLA = 4.8
ALFA_PERDIDA = 15
CD0 = 0.026
K = 0.057
FLAPS = 0, 10, 20, 30
DCL_FLAP = 0.60
DCD_FLAP = 0.060
DALFA_FLAP = 2
ALABEO = 55
PROFUNDIDAD = 12
VREF = 50
POTENCIA = 119        ; kW
RPM_MAX = 2700
RPM_RALENTI = 650
RPM_ESTATICA = 2300
CONSUMO = 38          ; l/h
VS0 = 41
VS1 = 48
VFE = 85
VNO = 128
VNE = 160
TECHO = 4200
ALTURA_OJO = 1.5
ACTITUD_SUELO = 0
""", mono),
"bimotor.acf": ("""NOMBRE = BIMOTOR 2X300HP
MOTOR = PISTON
MOTORES = 2
HELICE = VARIABLE
TREN = RETRACTIL
MASA = 2300
COMBUSTIBLE = 520
SUPERFICIE = 18.5
CL0 = 0.25
CLA = 5.0
ALFA_PERDIDA = 15
CD0 = 0.026
K = 0.050
DCD_TREN = 0.020
FLAPS = 0, 15, 30
DCL_FLAP = 0.55
DCD_FLAP = 0.055
DALFA_FLAP = 2
ALABEO = 60
PROFUNDIDAD = 12
VREF = 70
POTENCIA = 224
RPM_MAX = 2700
RPM_RALENTI = 650
RPM_ESTATICA = 2500
CONSUMO = 75
BRAZO_MOTOR = 2.4
VS0 = 70
VS1 = 80
VFE = 120
VNO = 195
VNE = 225
TECHO = 6000
ALTURA_OJO = 1.6
ACTITUD_SUELO = 0
""", bimotor),
"reactor.acf": ("""NOMBRE = REACTOR EJECUTIVO
MOTOR = REACTOR
MOTORES = 2
TREN = RETRACTIL
MASA = 7000
COMBUSTIBLE = 3000
SUPERFICIE = 30
CL0 = 0.20
CLA = 5.2
ALFA_PERDIDA = 13
CD0 = 0.022
K = 0.045
DCD_TREN = 0.018
FLAPS = 0, 15, 40
DCL_FLAP = 0.60
DCD_FLAP = 0.070
DALFA_FLAP = 2
ALABEO = 60
PROFUNDIDAD = 11
VREF = 110
EMPUJE = 13           ; kN por motor
ACELERACION_N1 = 0.25 ; fraccion de N1 por segundo
CONSUMO = 700
BRAZO_MOTOR = 1.6
VS0 = 88
VS1 = 102
VFE = 200
VNO = 300
VNE = 330
TECHO = 13000
ALTURA_OJO = 2.6
ACTITUD_SUELO = 0
""", reactor),
}

dst = sys.argv[1] if len(sys.argv) > 1 else "data"
for fn, (txt, mk) in AVIONES.items():
    m = mk()
    open(os.path.join(dst, fn), "w").write(
        "; avion para QFS - texto CLAVE = valor (';' comenta)\n" + txt +
        "; ---- modelo exterior: V = x,y,z (m)  F = relleno, contorno, vertices  L = color, a, b\n" +
        m.text() + "\n")
    print(fn, len(m.v), "vertices", len(m.f), "caras")
