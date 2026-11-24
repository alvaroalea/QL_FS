#!/usr/bin/env python3
"""iberia.py - genera el escenario "Peninsula Iberica" para QFS.

Uso:  iberia.py [escala] [salida.scn] [salida.txt]
      escala 0.5 (por defecto) = distancias a la mitad; 1.0 = tamano real.

- Las DISTANCIAS geograficas se multiplican por la escala; pistas, edificios y
  alturas de montanas van a tamano real (las pendientes salen mas empinadas).
- Canarias va desplazada como en los mapas (recuadro), mas cerca de la peninsula.
- Mundo plano a cota 0 (como el MSFS original): el suelo por defecto es tierra
  y el mar son poligonos de agua recortados por tesela (SPLASH! si se toca).
- Coordenadas, frecuencias e identificadores son APROXIMADOS (de memoria):
  sirven para jugar, no para navegar de verdad.

Escribe tambien un .txt con todo salvo el mar, editable con el formato de mkscen.
"""
import math, os, sys, random
from shapely.geometry import Polygon, LineString, box, Point
from shapely.ops import unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkscen
from mkscen import Scen, Obj, attr, FILLS, OBJ_GROUND, OF_WATER

S = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
OUT = sys.argv[2] if len(sys.argv) > 2 else "build/iberia.scn"
TXT = sys.argv[3] if len(sys.argv) > 3 else "data/iberia.txt"

LAT0, LON0 = 40.0, -3.7                    # origen: Madrid
KX = 111320.0 * math.cos(math.radians(LAT0))
KY = 110570.0
CAN_DX, CAN_DY = 400000.0, 500000.0        # desplazamiento real de Canarias (recuadro)


def P(lat, lon):
    x = (lon - LON0) * KX
    y = (lat - LAT0) * KY
    if lat < 30.5 and lon < -12.5:         # Canarias
        x += CAN_DX
        y += CAN_DY
    return (x * S, y * S)


def poly_ll(pts):
    return Polygon([P(a, b) for a, b in pts])


def circle_ll(lat, lon, rlat, rlon, n=12):
    return poly_ll([(lat + rlat * math.cos(2 * math.pi * i / n), lon + rlon * math.sin(2 * math.pi * i / n)) for i in range(n)])


# ---------------------------------------------------------------- tierra
PENINSULA = [  # peninsula + sur de Francia, sentido antihorario desde Hendaya
    (43.37, -1.79), (43.33, -2.25), (43.40, -2.70), (43.35, -3.03), (43.43, -3.55), (43.47, -3.80),
    (43.45, -4.20), (43.42, -4.75), (43.48, -5.30), (43.56, -5.68), (43.66, -5.85), (43.57, -6.30),
    (43.56, -6.95), (43.65, -7.40), (43.79, -7.69), (43.70, -8.05), (43.48, -8.25), (43.37, -8.40),
    (43.30, -8.80), (43.20, -9.10), (42.88, -9.27), (42.60, -9.05), (42.40, -8.85), (42.24, -8.80),
    (42.10, -8.85), (41.87, -8.87), (41.50, -8.78), (41.15, -8.67), (40.64, -8.75), (40.15, -8.87),
    (39.60, -9.07), (39.35, -9.38), (39.00, -9.42), (38.78, -9.50), (38.65, -9.25), (38.48, -9.18),
    (38.40, -8.85), (37.95, -8.87), (37.50, -8.80), (37.02, -8.99), (37.08, -8.65), (37.10, -8.20),
    (37.00, -7.93), (37.15, -7.45), (37.20, -6.95), (36.95, -6.45), (36.75, -6.43), (36.53, -6.30),
    (36.30, -6.15), (36.00, -5.61), (36.13, -5.43), (36.30, -5.25), (36.50, -4.88), (36.72, -4.42),
    (36.75, -3.90), (36.72, -3.52), (36.75, -2.95), (36.83, -2.45), (36.72, -2.19), (37.00, -1.90),
    (37.35, -1.65), (37.60, -0.98), (37.63, -0.69), (37.95, -0.73), (38.34, -0.48), (38.55, -0.05),
    (38.73, 0.23), (38.95, 0.05), (39.25, -0.25), (39.45, -0.32), (39.97, 0.02), (40.35, 0.40),
    (40.72, 0.87), (41.05, 1.20), (41.20, 1.65), (41.38, 2.20), (41.67, 2.79), (41.90, 3.20),
    (42.10, 3.20), (42.32, 3.32), (42.43, 3.17), (42.70, 3.04), (43.20, 3.20), (43.45, 3.90),
    (43.45, 4.70), (43.30, 5.40), (45.50, 5.40), (45.50, -1.20), (44.60, -1.25), (43.80, -1.40),
    (43.50, -1.55)]
MARRUECOS = [(35.78, -5.93), (35.90, -5.30), (35.40, -4.70), (35.20, -3.90), (35.30, -2.95), (35.10, -2.30),
             (35.10, -1.20), (34.00, -1.20), (34.00, -6.80), (34.80, -6.20), (35.50, -6.05)]
ISLAS = [
    [(39.95, 3.17), (39.87, 3.10), (39.85, 3.20), (39.73, 3.45), (39.56, 3.37), (39.40, 3.27), (39.28, 3.05),
     (39.35, 2.85), (39.50, 2.75), (39.55, 2.62), (39.50, 2.40), (39.58, 2.34), (39.70, 2.55), (39.80, 2.75),
     (39.90, 2.95)],                                                       # Mallorca
    [(40.05, 3.82), (40.05, 4.10), (39.88, 4.32), (39.82, 4.15), (39.93, 3.85)],   # Menorca
    [(39.10, 1.35), (39.07, 1.60), (38.88, 1.50), (38.84, 1.25), (38.98, 1.20)],   # Ibiza
    [(38.72, 1.38), (38.66, 1.58), (38.65, 1.40)],                         # Formentera
    [(28.58, -16.13), (28.47, -16.25), (28.30, -16.38), (28.10, -16.50), (28.00, -16.67), (28.10, -16.80),
     (28.32, -16.90), (28.38, -16.70), (28.42, -16.45), (28.55, -16.30)],  # Tenerife
    [(28.75, -13.83), (28.50, -13.84), (28.30, -13.90), (28.10, -14.10), (28.05, -14.35), (28.15, -14.50),
     (28.30, -14.10), (28.55, -14.00), (28.72, -13.95)],                   # Fuerteventura
    [(29.22, -13.45), (29.00, -13.42), (28.86, -13.80), (28.98, -13.88), (29.15, -13.62)],  # Lanzarote
]
CIRCULOS = [(27.95, -15.60, 0.21, 0.24), (28.11, -17.22, 0.10, 0.11), (27.74, -18.02, 0.10, 0.12),
            (28.67, -17.86, 0.20, 0.12)]   # Gran Canaria, La Gomera, El Hierro, La Palma

# ---------------------------------------------------------------- aeropuertos
# icao, nombre, lat, lon, ATIS, TWR, VOR (ident, MHz), pistas [(num, rumbo, largo, ancho, ILS, dx, dy)]
AEROPUERTOS = [
    ("LEMD", "MADRID BARAJAS", 40.472, -3.561, 118.67, 118.15, ("MAD", 116.40),
     [("32L", 324, 4100, 60, 109.90, 0, 0), ("36L", 181, 3500, 60, None, -2600, 1400)]),
    ("LEBL", "BARCELONA EL PRAT", 41.297, 2.078, 118.65, 118.10, ("BCN", 116.70),
     [("25R", 247, 3350, 60, 110.30, 0, 0), ("20", 200, 2500, 45, None, 1200, 1300)]),
    ("LEPA", "PALMA DE MALLORCA", 39.552, 2.739, 119.42, 118.30, ("PMI", 112.20),
     [("24L", 238, 3270, 45, 109.70, 0, 0)]),
    ("LEIB", "IBIZA", 38.873, 1.373, 127.05, 118.80, ("IBZ", 112.40),
     [("24", 239, 2800, 45, 110.90, 0, 0)]),
    ("LEVC", "VALENCIA", 39.489, -0.482, 125.40, 118.50, ("VLC", 114.60),
     [("12", 124, 3200, 45, 110.50, 0, 0)]),
    ("LEMG", "MALAGA", 36.675, -4.499, 118.37, 118.45, ("MGA", 112.10),
     [("13", 128, 3200, 45, 109.50, 0, 0)]),
    ("LEZL", "SEVILLA", 37.418, -5.893, 127.27, 118.10, ("SVL", 112.90),
     [("27", 267, 3360, 45, 109.10, 0, 0)]),
    ("LEST", "SANTIAGO", 42.896, -8.415, 120.62, 118.75, ("STG", 116.10),
     [("17", 172, 3200, 45, 110.70, 0, 0)]),
    ("LEAS", "ASTURIAS", 43.564, -6.035, 118.97, 118.15, ("AST", 112.60),
     [("29", 285, 2200, 45, 109.30, 0, 0)]),
    ("LPPT", "LISBOA", 38.781, -9.136, 124.15, 118.10, ("LIS", 114.80),
     [("03", 28, 3800, 45, 110.10, 0, 0)]),
    ("GCTS", "TENERIFE SUR", 28.044, -16.572, 118.37, 119.00, ("TFS", 116.60),
     [("07", 75, 3200, 45, 108.90, 0, 0)]),
    ("GCFV", "FUERTEVENTURA", 28.453, -13.864, 120.05, 118.25, ("FTV", 114.10),
     [("01", 8, 3400, 45, 108.70, 0, 0)]),
    ("GCLP", "GRAN CANARIA", 27.932, -15.387, 118.05, 118.30, ("GDV", 112.50),
     [("03L", 30, 3100, 45, 111.10, 0, 0)]),
]

VORS = [("ZZA", 114.30, 41.66, -1.04), ("VLL", 115.50, 41.71, -4.85), ("TLD", 113.20, 39.97, -4.34),
        ("BJZ", 114.00, 38.89, -6.82), ("ABC", 117.90, 38.95, -1.86), ("PPN", 113.90, 42.77, -1.65),
        ("SLM", 116.80, 40.95, -5.50), ("PRT", 114.50, 41.24, -8.68), ("FAR", 112.80, 37.01, -7.97),
        ("LEN", 117.10, 42.59, -5.65), ("MUR", 116.20, 37.80, -1.13), ("GDX", 113.50, 37.19, -3.78),
        ("BLB", 112.30, 43.30, -2.91), ("RES", 114.90, 41.15, 1.17), ("MHN", 113.70, 39.86, 4.22),
        ("LZR", 115.70, 28.95, -13.60), ("CDB", 117.40, 37.84, -4.85), ("CCS", 115.90, 39.47, -6.37),
        ("TER", 113.00, 40.41, -1.20), ("BRG", 116.90, 42.36, -3.62)]
NDBS = [("GE", 315, 40.29, -3.72), ("ZMR", 396, 41.51, -5.75), ("HLV", 352, 37.27, -6.95),
        ("ALM", 380, 36.85, -2.37), ("SNR", 335, 43.43, -3.82), ("LGR", 365, 42.45, -2.33),
        ("CUE", 402, 40.07, -2.13), ("GRN", 342, 39.74, -7.43)]

CIUDADES = [  # lat, lon, ancho, fondo (m reales), edificios
    (40.420, -3.700, 7000, 6000, 120), (41.390, 2.160, 5000, 4000, 90), (39.470, -0.380, 4000, 3500, 60),
    (37.390, -5.990, 4000, 3500, 55), (41.650, -0.890, 3500, 3000, 45), (36.720, -4.420, 3500, 2500, 45),
    (43.260, -2.930, 3000, 2500, 40), (38.720, -9.140, 4500, 4000, 70), (41.150, -8.610, 3500, 3000, 45),
    (41.650, -4.720, 3000, 2500, 35), (43.360, -8.410, 2500, 2000, 30), (42.880, -8.540, 2000, 1800, 25),
    (43.360, -5.850, 2200, 1800, 25), (43.530, -5.660, 2500, 2000, 30), (37.180, -3.600, 2800, 2400, 35),
    (37.880, -4.780, 2800, 2400, 35), (37.990, -1.130, 2800, 2400, 35), (38.350, -0.480, 3000, 2500, 40),
    (39.570, 2.650, 3000, 2500, 40), (28.120, -15.430, 3000, 2500, 40), (28.470, -16.250, 2500, 2000, 30),
    (42.810, -1.640, 2200, 1800, 25), (40.960, -5.660, 2000, 1800, 25), (39.860, -4.020, 1500, 1200, 18),
    (38.880, -6.970, 1800, 1500, 20), (43.460, -3.800, 2200, 1800, 25), (43.320, -1.980, 2000, 1800, 25),
    (38.910, 1.430, 1500, 1200, 15), (28.500, -13.860, 1200, 1000, 12), (37.020, -7.930, 1500, 1200, 15),
    (42.350, -3.700, 2000, 1800, 22), (40.630, -3.170, 1400, 1200, 14), (36.530, -6.290, 1500, 1200, 16)]

SISTEMAS = {  # cadenas de (lat, lon, altura m), radio real de la base en km
    "PIRINEOS": ([(43.05, -1.40, 1500), (42.95, -0.90, 2200), (42.80, -0.40, 2800), (42.67, 0.03, 3355),
                  (42.63, 0.66, 3404), (42.60, 1.20, 2900), (42.55, 1.70, 2900), (42.45, 2.20, 2600),
                  (42.45, 2.80, 1300)], 22),
    "CANTABRICA": ([(42.95, -7.00, 1800), (43.00, -6.20, 2100), (43.05, -5.60, 2400), (43.17, -4.85, 2648),
                    (43.05, -4.20, 2000), (43.10, -3.60, 1600)], 20),
    "MONTES DE LEON": ([(42.45, -6.60, 2100), (42.20, -7.00, 1900)], 18),
    "SISTEMA CENTRAL": ([(40.32, -7.60, 1993), (40.30, -6.70, 1500), (40.25, -5.60, 2400), (40.25, -5.20, 2592),
                         (40.45, -4.60, 1900), (40.78, -4.00, 2428), (41.00, -3.50, 2100), (41.15, -3.20, 2262)], 18),
    "SISTEMA IBERICO": ([(42.10, -3.00, 2131), (41.97, -2.60, 2313), (41.79, -1.84, 2314), (41.20, -1.50, 1600),
                         (40.40, -1.80, 1800), (40.60, -1.20, 1900), (40.10, -1.02, 2020)], 20),
    "SIERRA MORENA": ([(38.10, -6.50, 900), (38.20, -5.60, 1000), (38.30, -4.60, 1100), (38.40, -3.60, 1300),
                       (38.50, -2.80, 1100)], 18),
    "MONTES DE TOLEDO": ([(39.40, -4.90, 1400), (39.50, -4.20, 1400)], 16),
    "BETICAS": ([(36.70, -5.20, 1900), (36.85, -4.50, 2000), (37.00, -3.90, 1800), (37.05, -3.31, 3479),
                 (37.10, -2.70, 2600), (37.35, -2.40, 2000), (37.90, -2.90, 2100), (38.10, -2.20, 1800),
                 (38.30, -1.30, 1500), (38.65, -0.40, 1500)], 18),
    "TRAMUNTANA": ([(39.65, 2.55, 1100), (39.80, 2.80, 1445)], 10),
    "CATALANES": ([(41.60, 1.81, 1236), (41.77, 2.40, 1700)], 12),
}
PICOS = [(28.27, -16.64, 3715, 14), (27.96, -15.60, 1956, 12), (28.72, -17.86, 2426, 9),
         (28.40, -14.10, 800, 8), (29.00, -13.70, 600, 7), (28.10, -17.22, 1480, 6), (27.74, -18.02, 1500, 6)]

RIOS = {
    "EBRO": [(43.00, -4.10), (42.60, -3.00), (42.30, -2.20), (41.65, -0.89), (41.30, 0.30), (40.90, 0.60), (40.72, 0.87)],
    "DUERO": [(41.90, -2.80), (41.60, -3.70), (41.65, -4.72), (41.50, -5.75), (41.00, -6.80), (41.15, -7.80), (41.14, -8.66)],
    "TAJO": [(40.40, -1.80), (40.30, -2.90), (40.03, -3.60), (39.86, -4.02), (39.75, -5.60), (39.60, -6.90),
             (39.40, -8.20), (38.90, -9.00)],
    "GUADIANA": [(39.00, -3.40), (39.10, -4.30), (38.90, -6.00), (38.88, -6.97), (38.30, -7.20), (37.50, -7.45), (37.20, -7.40)],
    "GUADALQUIVIR": [(37.90, -2.90), (37.95, -3.80), (37.88, -4.78), (37.60, -5.50), (37.39, -5.99), (37.00, -6.20), (36.80, -6.35)],
    "MINO": [(43.00, -7.50), (42.30, -7.90), (42.00, -8.50), (41.87, -8.87)],
}
CARRETERAS = {
    "A-1": [(40.42, -3.70), (41.10, -3.60), (42.34, -3.70), (42.85, -2.67), (43.32, -1.98)],
    "A-2": [(40.42, -3.70), (40.63, -3.17), (41.30, -2.30), (41.65, -0.89), (41.62, 0.62), (41.39, 2.16)],
    "A-3": [(40.42, -3.70), (40.00, -2.60), (39.60, -1.30), (39.47, -0.38)],
    "A-4": [(40.42, -3.70), (39.40, -3.60), (38.09, -3.78), (37.88, -4.78), (37.39, -5.99), (36.53, -6.29)],
    "A-5": [(40.42, -3.70), (39.90, -4.90), (39.30, -5.90), (38.92, -6.34), (38.88, -6.97), (38.70, -8.20), (38.72, -9.14)],
    "A-6": [(40.42, -3.70), (41.00, -4.50), (42.00, -5.68), (42.55, -6.60), (43.00, -7.50), (43.36, -8.41)],
    "A-7": [(41.39, 2.16), (41.12, 1.25), (40.50, 0.40), (39.47, -0.38), (38.35, -0.48), (37.99, -1.13),
            (36.85, -2.45), (36.75, -3.50), (36.72, -4.42)],
    "A-8": [(43.32, -1.98), (43.26, -2.93), (43.46, -3.80), (43.53, -5.66), (43.40, -7.00)],
    "A1-PT": [(38.72, -9.14), (39.40, -8.70), (40.20, -8.42), (41.15, -8.61), (42.24, -8.72), (42.88, -8.54)],
}


def fmt(v):
    return ("%.0f" % v)


def build():
    random.seed(1)
    lines = ["; Peninsula Iberica para QFS - generado por tools/iberia.py (escala %.2f)" % S,
             "; Datos aproximados de memoria: NO usar para navegacion real.",
             'NOMBRE "IBERIA"', "TESELA 14", "ELEV 0", ""]
    land = [poly_ll(PENINSULA), poly_ll(MARRUECOS)] + [poly_ll(p) for p in ISLAS] + \
           [circle_ll(*c) for c in CIRCULOS]
    # aeropuertos: pistas a tamano real, posicion a escala
    for icao, name, lat, lon, atis, twr, vor, rwys in AEROPUERTOS:
        cx, cy = P(lat, lon)
        h0 = math.radians(rwys[0][1])
        tx, ty = cx + math.cos(h0) * 450, cy - math.sin(h0) * 450   # torre a 450 m a la derecha de la pista
        ils_txt = "ILS RUNWAY %s APPROACH IN USE. LOCALIZER %.2f." % (rwys[0][0], rwys[0][4]) if rwys[0][4] else ""
        lines.append('AEROPUERTO %s "%s" %s %s 0 ATIS %.2f TWR %.2f "LANDING AND DEPARTING RUNWAY %s. %s"'
                     % (icao, name, fmt(tx), fmt(ty), atis, twr, rwys[0][0], ils_txt))
        for num, hdg, ln, wd, ils, dx, dy in rwys:
            hx, hy = math.sin(math.radians(hdg)), math.cos(math.radians(hdg))
            rcx, rcy = cx + dx, cy + dy
            thx, thy = rcx - hx * ln / 2, rcy - hy * ln / 2
            lines.append("PISTA %s %s %s %s %d %d %d%s LUCES" % (icao, num, fmt(thx), fmt(thy), hdg, ln, wd,
                                                               (" ILS %.2f" % ils) if ils else ""))
            land.append(LineString([(thx, thy), (thx + hx * ln, thy + hy * ln)]).buffer(900))
        lines.append("TORRE %s %s" % (fmt(tx), fmt(ty)))
        lines.append("HANGAR %s %s %d" % (fmt(tx + math.sin(h0) * 200), fmt(ty + math.cos(h0) * 200), rwys[0][1]))
        lines.append("EDIFICIO %s %s 120 30 14 %d BLANCO" % (fmt(tx - math.sin(h0) * 250), fmt(ty - math.cos(h0) * 250),
                                                           (90 - rwys[0][1]) % 360))
        land.append(Point(tx, ty).buffer(1200))
        vx, vy = P(lat + 0.012, lon - 0.012)
        lines.append("VOR %s %.2f %s %s DME" % (vor[0], vor[1], fmt(vx), fmt(vy)))
        lines.append("")
    for ident, f, lat, lon in VORS:
        x, y = P(lat, lon)
        lines.append("VOR %s %.2f %s %s DME" % (ident, f, fmt(x), fmt(y)))
    for ident, f, lat, lon in NDBS:
        x, y = P(lat, lon)
        lines.append("NDB %s %d %s %s" % (ident, f, fmt(x), fmt(y)))
    # NDB en el mar, a mitad de camino entre Cadiz y Lanzarote
    ax, ay = P(36.53, -6.30)
    bx, by = P(29.00, -13.60)
    lines.append("NDB MAR 425 %s %s" % (fmt((ax + bx) / 2), fmt((ay + by) / 2)))
    lines.append("")
    for i, (lat, lon, w, d, n) in enumerate(CIUDADES):
        x, y = P(lat, lon)
        lines.append("CIUDAD %s %s %d %d %d %d" % (fmt(x), fmt(y), w, d, n, 11 + i))
        land.append(box(x - w / 2 - 300, y - d / 2 - 300, x + w / 2 + 300, y + d / 2 + 300))
    # rascacielos de Madrid y torres de Barcelona
    x, y = P(40.477, -3.688)
    for k, h in enumerate((250, 236, 224, 250)):
        lines.append("EDIFICIO %s %s 45 45 %d 0 GRISCLARO" % (fmt(x + (k - 1.5) * 160), fmt(y), h))
    x, y = P(41.403, 2.190)
    lines.append("EDIFICIO %s %s 40 40 144 0 NARANJA" % (fmt(x), fmt(y)))
    lines.append("")
    # montanas: conos a lo largo de cada sistema
    for nombre, (pts, rkm) in SISTEMAS.items():
        lines.append("; " + nombre)
        r = rkm * 1000 * S
        for (la0, lo0, h0), (la1, lo1, h1) in zip(pts, pts[1:]):
            x0, y0 = P(la0, lo0)
            x1, y1 = P(la1, lo1)
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / (r * 1.1)))
            for k in range(n):
                t = k / n
                h = (h0 + (h1 - h0) * t) * (0.85 + 0.15 * random.random())
                lines.append("MONTANA %s %s %s %d 8" % (fmt(x0 + (x1 - x0) * t), fmt(y0 + (y1 - y0) * t),
                                                        fmt(r * (0.8 + 0.4 * random.random())), h))
        la, lo, h = pts[-1]
        x, y = P(la, lo)
        lines.append("MONTANA %s %s %s %d 8" % (fmt(x), fmt(y), fmt(r), h))
    for la, lo, h, rkm in PICOS:
        x, y = P(la, lo)
        lines.append("MONTANA %s %s %s %d 9" % (fmt(x), fmt(y), fmt(rkm * 1000 * S), h))
    lines.append("")
    for nombre, pts in RIOS.items():
        lines.append("; rio " + nombre)
        lines.append("RIO " + " ".join("%s %s" % tuple(map(fmt, P(a, b))) for a, b in pts))
    for nombre, pts in CARRETERAS.items():
        lines.append("; " + nombre)
        lines.append("CARRETERA " + " ".join("%s %s" % tuple(map(fmt, P(a, b))) for a, b in pts))
    return lines, unary_union(land)


def split_lines(sc):
    """trocea las polilineas largas (rios, carreteras) por teselas"""
    T = 1 << sc.tlog
    out = []
    for o in sc.objs:
        if o.typ != mkscen.OBJ_LINES or len(o.f) != 1:
            out.append(o)
            continue
        col = o.f[0][0]
        pts = [(o.ox + o.v[i][0], o.oy + o.v[i][1]) for i in o.f[0][1]]
        ls = LineString(pts)
        x0, y0, x1, y1 = ls.bounds
        for gx in range(int(math.floor(x0 / T)), int(math.floor(x1 / T)) + 1):
            for gy in range(int(math.floor(y0 / T)), int(math.floor(y1 / T)) + 1):
                cell = box(gx * T, gy * T, (gx + 1) * T, (gy + 1) * T)
                piece = ls.intersection(cell)
                parts = getattr(piece, "geoms", [piece])
                for p in parts:
                    if p.is_empty or p.geom_type != "LineString" or p.length < 1:
                        continue
                    c = list(p.coords)
                    n = Obj(mkscen.OBJ_LINES, (gx + 0.5) * T, (gy + 0.5) * T, sc.elev, 0, lod=60)
                    n.face(col, [n.vert(x, y) for x, y in c])
                    out.append(n)
    sc.objs = out


def sea_pieces(poly, depth=0):
    """divide un poligono con agujeros (islas) en trozos sin agujeros y <= 60 vertices"""
    if poly.is_empty:
        return []
    if poly.geom_type == "MultiPolygon" or poly.geom_type == "GeometryCollection":
        r = []
        for g in poly.geoms:
            if g.geom_type == "Polygon":
                r += sea_pieces(g, depth)
        return r
    if poly.geom_type != "Polygon" or poly.area < 1000:
        return []
    poly = poly.simplify(25 * S + 10, preserve_topology=True)
    if (poly.interiors or len(poly.exterior.coords) > 61) and depth < 8:
        x0, y0, x1, y1 = poly.bounds
        if poly.interiors:
            cx = list(poly.interiors)[0].centroid.x
        else:
            cx = (x0 + x1) / 2
        if x1 - x0 < y1 - y0 and not poly.interiors:
            cy = (y0 + y1) / 2
            a, b = box(x0 - 1, y0 - 1, x1 + 1, cy), box(x0 - 1, cy, x1 + 1, y1 + 1)
        else:
            a, b = box(x0 - 1, y0 - 1, cx, y1 + 1), box(cx, y0 - 1, x1 + 1, y1 + 1)
        return sea_pieces(poly.intersection(a), depth + 1) + sea_pieces(poly.intersection(b), depth + 1)
    return [poly]


def add_sea(sc, land):
    T = 1 << sc.tlog
    x0, y0, x1, y1 = land.bounds
    gx0, gy0 = int(math.floor(x0 / T)) - 4, int(math.floor(y0 / T)) - 4
    gx1, gy1 = int(math.floor(x1 / T)) + 4, int(math.floor(y1 / T)) + 4
    n = 0
    for gx in range(gx0, gx1 + 1):
        for gy in range(gy0, gy1 + 1):
            cell = box(gx * T, gy * T, (gx + 1) * T, (gy + 1) * T)
            if land.contains(cell):
                continue
            for p in sea_pieces(cell.difference(land)):
                o = Obj(OBJ_GROUND, (gx + 0.5) * T, (gy + 0.5) * T, sc.elev, OF_WATER, lod=255)
                pts = list(p.exterior.coords)[:-1]
                o.face(attr(FILLS["AGUA"]), [o.vert(x, y) for x, y in pts])
                sc.objs.append(o)
                n += 1
    return n


if __name__ == "__main__":
    lines, land = build()
    os.makedirs(os.path.dirname(TXT) or ".", exist_ok=True)
    open(TXT, "w").write("\n".join(lines) + "\n")
    sc = Scen()
    sc.parse(TXT)
    sc.vis = 40
    split_lines(sc)
    nsea = add_sea(sc, land)
    print("escala %.2f: %d poligonos de mar" % (S, nsea))
    os.makedirs(os.path.dirname(OUT) or ".", exist_ok=True)
    sc.write(OUT)
