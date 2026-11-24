#!/usr/bin/env python3
"""cartas.py - cartas aeronauticas en PDF del escenario Peninsula Iberica de QFS.

Uso: cartas.py [escala] [carpeta]
  QFS_Carta_Ruta_A3.pdf      carta de navegacion en ruta (A3 apaisado)
  QFS_Aeropuertos_A5.pdf     por aeropuerto: plano de aerodromo, SID y STAR + ILS (A5 vertical)
  QFS_Cartas_Visuales_A4.pdf por aeropuerto: carta visual de los alrededores (A4 apaisado)

Todo sale de los mismos datos que el escenario (tools/iberia.py), asi que lo que
se ve en la carta es lo que hay en el simulador. Distancias en NM del simulador,
rumbos verdaderos (el simulador no tiene variacion magnetica), elevaciones en pies.
"""
import math, os, sys

_argv = sys.argv[:]
SCALE = _argv[1] if len(_argv) > 1 else "0.5"
OUTDIR = _argv[2] if len(_argv) > 2 else "build/cartas"
sys.argv = [_argv[0], SCALE, "/tmp/_cartas.scn", "/tmp/_cartas.txt"]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iberia as I
sys.argv = _argv

from shapely.geometry import box, Point, LineString
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A3, A4, A5, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import Color, HexColor, white, black

NM = 1852.0
S = I.S

# ---------------------------------------------------------------- colores
C_SEA = HexColor("#cfe5f2")
C_LAND = HexColor("#f6f3e7")
C_COAST = HexColor("#5b8db8")
C_GRAT = HexColor("#9aa6b0")
C_IFR = HexColor("#1d4f91")
C_NDB = HexColor("#9c2f7d")
C_MAG = HexColor("#a3226f")
C_TERR = HexColor("#b98d5f")
C_ROAD = HexColor("#c8312c")
C_RIVER = HexColor("#3c7fc4")
C_CITY = HexColor("#f2d36b")
C_VFRLAND = HexColor("#eef2d6")
C_VFRSEA = HexColor("#a9d1ec")
C_TWY = HexColor("#9a9a9a")
C_APRON = HexColor("#cfcfcf")
TINTS = [(0, HexColor("#eef2d6")), (300, HexColor("#e6e2b9")), (700, HexColor("#dccb98")),
         (1200, HexColor("#cfae7b")), (1800, HexColor("#bd9065")), (2500, HexColor("#a87655"))]

CITY_NAMES = ["MADRID", "BARCELONA", "VALENCIA", "SEVILLA", "ZARAGOZA", "MÁLAGA", "BILBAO", "LISBOA", "PORTO",
              "VALLADOLID", "A CORUÑA", "SANTIAGO", "OVIEDO", "GIJÓN", "GRANADA", "CÓRDOBA", "MURCIA", "ALICANTE",
              "PALMA", "LAS PALMAS", "S. C. DE TENERIFE", "PAMPLONA", "SALAMANCA", "TOLEDO", "BADAJOZ", "SANTANDER",
              "SAN SEBASTIÁN", "IBIZA", "PTO. DEL ROSARIO", "FARO", "BURGOS", "GUADALAJARA", "CÁDIZ"]
VOR_NAMES = {"MAD": "MADRID", "BCN": "BARCELONA", "PMI": "PALMA", "IBZ": "IBIZA", "VLC": "VALENCIA",
             "MGA": "MÁLAGA", "SVL": "SEVILLA", "STG": "SANTIAGO", "AST": "ASTURIAS", "LIS": "LISBOA",
             "TFS": "TENERIFE SUR", "FTV": "FUERTEVENTURA", "GDV": "GRAN CANARIA", "ZZA": "ZARAGOZA",
             "VLL": "VALLADOLID", "TLD": "TOLEDO", "BJZ": "BADAJOZ", "ABC": "ALBACETE", "PPN": "PAMPLONA",
             "SLM": "SALAMANCA", "PRT": "PORTO", "FAR": "FARO", "LEN": "LEÓN", "MUR": "MURCIA",
             "GDX": "GRANADA", "BLB": "BILBAO", "RES": "REUS", "MHN": "MENORCA", "LZR": "LANZAROTE",
             "CDB": "CÓRDOBA", "CCS": "CÁCERES", "TER": "TERUEL", "BRG": "BURGOS"}
NDB_NAMES = {"GE": "GETAFE", "ZMR": "ZAMORA", "HLV": "HUELVA", "ALM": "ALMERÍA", "SNR": "SANTANDER",
             "LGR": "LOGROÑO", "CUE": "CUENCA", "GRN": "GUARDA", "MAR": "MAR"}
PEAKS = [("ANETO", 42.63, 0.66, 3404), ("MONTE PERDIDO", 42.67, 0.03, 3355), ("PICOS DE EUROPA", 43.17, -4.85, 2648),
         ("ALMANZOR", 40.25, -5.20, 2592), ("PEÑALARA", 40.78, -4.00, 2428), ("MONCAYO", 41.79, -1.84, 2314),
         ("MULHACÉN", 37.05, -3.31, 3479), ("TORRE (ESTRELA)", 40.32, -7.60, 1993), ("TEIDE", 28.27, -16.64, 3715),
         ("ROQUE NUBLO", 27.96, -15.60, 1956), ("R. DE LOS MUCHACHOS", 28.72, -17.86, 2426),
         ("CANIGÓ", 42.45, 2.20, 2600), ("PUIG MAJOR", 39.80, 2.80, 1445)]
RANGES = {"PIRINEOS": (42.95, 0.6), "CORDILLERA CANTÁBRICA": (43.25, -5.6), "SISTEMA CENTRAL": (40.55, -4.9),
          "SISTEMA IBÉRICO": (41.2, -2.0), "SIERRA MORENA": (38.45, -4.8), "SIERRA NEVADA": (37.2, -3.0),
          "MONTES DE TOLEDO": (39.6, -4.6), "SERRA DA ESTRELA": (40.15, -7.6)}

# aerovias (designador, lista de identificadores)
AIRWAYS = [("A1", ["MAD", "ZZA", "RES", "BCN"]), ("A2", ["MAD", "TER", "VLC"]), ("A3", ["MAD", "ABC", "MUR"]),
           ("A4", ["MAD", "CDB", "SVL"]), ("A5", ["MAD", "GDX", "MGA"]), ("A6", ["MAD", "TLD", "BJZ", "LIS"]),
           ("A7", ["MAD", "SLM", "PRT"]), ("A8", ["MAD", "VLL", "LEN", "AST"]), ("A9", ["LEN", "STG", "PRT"]),
           ("A10", ["VLL", "BRG", "BLB"]), ("A11", ["BRG", "PPN", "ZZA"]), ("A12", ["BCN", "PMI", "MHN"]),
           ("A13", ["VLC", "IBZ", "PMI"]), ("A14", ["SVL", "FAR", "LIS"]), ("A15", ["SVL", "MAR", "LZR", "FTV"]),
           ("A16", ["FTV", "GDV", "TFS"]), ("A17", ["MGA", "SVL"]), ("A18", ["MUR", "VLC"]), ("A19", ["CCS", "SLM"]),
           ("A20", ["BJZ", "CCS", "TLD"])]

MORSE = {'A': '.-', 'B': '-...', 'C': '-.-.', 'D': '-..', 'E': '.', 'F': '..-.', 'G': '--.', 'H': '....', 'I': '..',
         'J': '.---', 'K': '-.-', 'L': '.-..', 'M': '--', 'N': '-.', 'O': '---', 'P': '.--.', 'Q': '--.-', 'R': '.-.',
         'S': '...', 'T': '-', 'U': '..-', 'V': '...-', 'W': '.--', 'X': '-..-', 'Y': '-.--', 'Z': '--..'}

# ---------------------------------------------------------------- datos del escenario
TXT_LINES, LAND = I.build()
APTS = [I.layout(a) for a in I.AEROPUERTOS]
NAVS = {}           # ident -> (tipo, freq, x, y)
CONES = []          # (x, y, r, h)
CITIES = []
RIVERS, ROADS = [], []
_name = None
_ci = 0
for ln in TXT_LINES:
    t = ln.split(";")[0].split()
    if ln.startswith(";"):
        _name = ln[1:].strip().replace("rio ", "")
        continue
    if not t:
        continue
    if t[0] == "VOR":
        NAVS[t[1]] = ("VOR", float(t[2]), float(t[3]), float(t[4]))
    elif t[0] == "NDB":
        NAVS[t[1]] = ("NDB", float(t[2]), float(t[3]), float(t[4]))
    elif t[0] == "MONTANA":
        CONES.append((float(t[1]), float(t[2]), float(t[3]), float(t[4])))
    elif t[0] == "CIUDAD":
        CITIES.append((CITY_NAMES[_ci], float(t[1]), float(t[2]), float(t[3]), float(t[4])))
        _ci += 1
    elif t[0] in ("RIO", "CARRETERA"):
        v = list(map(float, t[1:]))
        pts = list(zip(v[0::2], v[1::2]))
        (RIVERS if t[0] == "RIO" else ROADS).append((_name, pts))


def inv(x, y, canary=False):
    """coordenadas del simulador -> lat/lon geograficas"""
    x, y = x / S, y / S
    if canary:
        x -= I.CAN_DX
        y -= I.CAN_DY
    return y / I.KY + I.LAT0, x / I.KX + I.LON0


def brg(a, b):
    return math.degrees(math.atan2(b[0] - a[0], b[1] - a[1])) % 360


def dist(a, b):
    return math.hypot(b[0] - a[0], b[1] - a[1])


def ft(m):
    return int(round(m * 3.28084))


def terrain_at(x, y):
    """altura del terreno (conos) en un punto"""
    h = 0
    for cx, cy, r, ch in CONES:
        d = math.hypot(x - cx, y - cy)
        if d < r:
            h = max(h, ch * (1 - d / r))
    return h


def msa(p, radius_nm=25):
    """altitud minima de sector (pies) en 25 NM: obstaculo + 1000 ft (+2000 si > 5000 ft), redondeo a 100"""
    R = radius_nm * NM
    h = 0
    for cx, cy, r, ch in CONES:
        d = math.hypot(p[0] - cx, p[1] - cy)
        if d < R + r:
            h = max(h, ch * max(0.0, 1 - max(0.0, d - R) / r))
    f = ft(h)
    f += 2000 if f > 5000 else 1000
    return max(2000, int(math.ceil(f / 100.0) * 100))


def dms(v, latlon):
    hemi = ("N" if v >= 0 else "S") if latlon == "lat" else ("E" if v >= 0 else "W")
    v = abs(v)
    d = int(v)
    m = (v - d) * 60
    return ("%s%02d°%04.1f'" if latlon == "lat" else "%s%03d°%04.1f'") % (hemi, d, m)


def is_canary(p):
    return p[1] < I.P(30.5, -10)[1] and p[0] < I.P(36, -7)[0]


# ---------------------------------------------------------------- utilidades de dibujo
class View:
    def __init__(self, c, px, py, pw, ph, x0, y0, x1, y1, fit=True):
        self.c = c
        self.px, self.py, self.pw, self.ph = px, py, pw, ph
        k = min(pw / (x1 - x0), ph / (y1 - y0)) if fit else pw / (x1 - x0)
        self.k = k
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        self.x0 = cx - pw / k / 2
        self.y0 = cy - ph / k / 2
        self.x1 = self.x0 + pw / k
        self.y1 = self.y0 + ph / k

    def pt(self, x, y):
        return self.px + (x - self.x0) * self.k, self.py + (y - self.y0) * self.k

    def clip(self):
        p = self.c.beginPath()
        p.rect(self.px, self.py, self.pw, self.ph)
        self.c.clipPath(p, stroke=0, fill=0)

    def poly(self, pts, fill=None, stroke=None, width=0.5, close=True):
        if len(pts) < 2:
            return
        c = self.c
        p = c.beginPath()
        x, y = self.pt(*pts[0])
        p.moveTo(x, y)
        for q in pts[1:]:
            p.lineTo(*self.pt(*q))
        if close:
            p.close()
        if fill is not None:
            c.setFillColor(fill)
        if stroke is not None:
            c.setStrokeColor(stroke)
            c.setLineWidth(width)
        c.drawPath(p, fill=1 if fill is not None else 0, stroke=1 if stroke is not None else 0)

    def geom(self, g, fill=None, stroke=None, width=0.5):
        if g.is_empty:
            return
        if g.geom_type in ("MultiPolygon", "GeometryCollection"):
            for gg in g.geoms:
                self.geom(gg, fill, stroke, width)
            return
        if g.geom_type != "Polygon":
            return
        c = self.c
        p = c.beginPath()
        for ring in [g.exterior] + list(g.interiors):
            cs = list(ring.coords)
            p.moveTo(*self.pt(*cs[0]))
            for q in cs[1:]:
                p.lineTo(*self.pt(*q))
            p.close()
        if fill is not None:
            c.setFillColor(fill)
        if stroke is not None:
            c.setStrokeColor(stroke)
            c.setLineWidth(width)
        c.drawPath(p, fill=1 if fill is not None else 0, stroke=1 if stroke is not None else 0, fillMode=0)

    def inside(self, x, y, m=0):
        return self.x0 - m <= x <= self.x1 + m and self.y0 - m <= y <= self.y1 + m


def text(c, x, y, s, size=6, font="Helvetica", color=black, align="l", rot=0):
    c.saveState()
    c.setFillColor(color)
    c.setFont(font, size)
    c.translate(x, y)
    if rot:
        c.rotate(rot)
    if align == "c":
        c.drawCentredString(0, 0, s)
    elif align == "r":
        c.drawRightString(0, 0, s)
    else:
        c.drawString(0, 0, s)
    c.restoreState()


def tbox(c, x, y, lines, size=5.5, color=C_IFR, fill=white, align="l", pad=1.5, bold_first=True, border=True):
    """caja de texto (lineas) con esquina inferior izquierda en x,y; devuelve (ancho, alto)"""
    c.saveState()
    w = max(c.stringWidth(s, "Helvetica-Bold" if (i == 0 and bold_first) else "Helvetica", size)
            for i, s in enumerate(lines)) + 2 * pad
    h = len(lines) * (size + 1) + 2 * pad - 1
    if align == "c":
        x -= w / 2
    elif align == "r":
        x -= w
    c.setFillColor(fill)
    c.setStrokeColor(color)
    c.setLineWidth(0.4)
    c.rect(x, y, w, h, fill=1, stroke=1 if border else 0)
    c.setFillColor(color)
    for i, s in enumerate(lines):
        c.setFont("Helvetica-Bold" if (i == 0 and bold_first) else "Helvetica", size)
        c.drawString(x + pad, y + h - pad - (i + 1) * (size + 1) + 1.5, s)
    c.restoreState()
    return w, h


def morse(c, x, y, ident, color=C_IFR):
    c.saveState()
    c.setStrokeColor(color)
    c.setLineWidth(0.5)
    for ch in ident:
        for sym in MORSE.get(ch, ""):
            L = 0.6 if sym == "." else 1.8
            c.line(x, y, x + L, y)
            x += L + 0.6
        x += 1.6
    c.restoreState()


def vor_symbol(c, x, y, s=2.2, color=C_IFR, dme=True):
    c.saveState()
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(0.6)
    p = c.beginPath()
    for i in range(6):
        a = math.radians(60 * i)
        (p.moveTo if i == 0 else p.lineTo)(x + s * math.cos(a), y + s * math.sin(a))
    p.close()
    c.drawPath(p, stroke=1, fill=0)
    if dme:
        c.rect(x - s * 1.25, y - s * 1.1, s * 2.5, s * 2.2, stroke=1, fill=0)
    c.circle(x, y, 0.45, stroke=0, fill=1)
    c.restoreState()


def compass_rose(c, x, y, r, color=C_IFR):
    c.saveState()
    c.setStrokeColor(color)
    c.setLineWidth(0.3)
    c.circle(x, y, r, stroke=1, fill=0)
    for d in range(0, 360, 10):
        a = math.radians(d)
        L = r * (0.13 if d % 30 else 0.22)
        c.line(x + math.sin(a) * r, y + math.cos(a) * r, x + math.sin(a) * (r - L), y + math.cos(a) * (r - L))
        if d % 90 == 0:
            text(c, x + math.sin(a) * (r - L - 2.2), y + math.cos(a) * (r - L - 2.2) - 1.2, "%02d" % (d // 10), 3.6,
                 color=color, align="c")
    # flecha del norte
    c.setFillColor(color)
    p = c.beginPath()
    p.moveTo(x, y + r + 2.5)
    p.lineTo(x - 1.2, y + r)
    p.lineTo(x + 1.2, y + r)
    p.close()
    c.drawPath(p, fill=1, stroke=0)
    c.restoreState()


def ndb_symbol(c, x, y, r=2.6, color=C_NDB):
    c.saveState()
    c.setFillColor(color)
    for ring, n in ((r, 16), (r * 0.65, 11)):
        for i in range(n):
            a = 2 * math.pi * i / n
            c.circle(x + ring * math.cos(a), y + ring * math.sin(a), 0.28, stroke=0, fill=1)
    c.circle(x, y, 0.7, stroke=0, fill=1)
    c.restoreState()


def peak(c, x, y, h_m, label=None, color=HexColor("#6b4b2a"), size=5):
    c.saveState()
    c.setFillColor(color)
    p = c.beginPath()
    p.moveTo(x, y + 1.6)
    p.lineTo(x - 1.4, y - 0.8)
    p.lineTo(x + 1.4, y - 0.8)
    p.close()
    c.drawPath(p, fill=1, stroke=0)
    c.restoreState()
    text(c, x + 2, y - 1, "%d" % ft(h_m), size, "Helvetica-Bold", color)
    if label:
        text(c, x + 2, y - 1 - size, label, size - 1.2, "Helvetica-Oblique", color)


def scale_bar(c, x, y, k, nm_max, color=black, km=True):
    """barra de escala: k = puntos por metro del simulador"""
    c.saveState()
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(0.5)
    L = nm_max * NM * k
    c.line(x, y, x + L, y)
    step = nm_max / 5.0
    for i in range(6):
        xx = x + i * step * NM * k
        c.line(xx, y, xx, y + 1.5)
        text(c, xx, y + 2.2, "%g" % (i * step), 5, align="c", color=color)
        if i < 5 and i % 2 == 0:
            c.rect(xx, y - 1.2, step * NM * k, 1.2, fill=1, stroke=1)
    text(c, x + L + 3, y + 0.5, "NM", 5.5, "Helvetica-Bold", color)
    if km:
        kmmax = nm_max * NM / 1000.0
        stepk = 10 ** math.floor(math.log10(kmmax / 2))
        n = int(kmmax // stepk)
        c.line(x, y - 6, x + n * stepk * 1000 * k, y - 6)
        for i in range(n + 1):
            xx = x + i * stepk * 1000 * k
            c.line(xx, y - 6, xx, y - 7.5)
            if i % max(1, n // 5) == 0:
                text(c, xx, y - 10.5, "%g" % (i * stepk), 5, align="c", color=color)
        text(c, x + n * stepk * 1000 * k + 3, y - 7, "km", 5.5, "Helvetica-Bold", color)
    c.restoreState()


def north_arrow(c, x, y, size=8, label="N V"):
    c.saveState()
    c.setFillColor(black)
    p = c.beginPath()
    p.moveTo(x, y + size)
    p.lineTo(x - size * 0.3, y)
    p.lineTo(x, y + size * 0.25)
    p.close()
    c.drawPath(p, fill=1, stroke=0)
    p = c.beginPath()
    p.moveTo(x, y + size)
    p.lineTo(x + size * 0.3, y)
    p.lineTo(x, y + size * 0.25)
    p.close()
    c.setLineWidth(0.5)
    c.drawPath(p, fill=0, stroke=1)
    text(c, x, y + size + 2, label, 6, "Helvetica-Bold", align="c")
    c.restoreState()


DISCLAIMER = "CARTA DEL SIMULADOR QFS - NO APTA PARA LA NAVEGACIÓN REAL"
SCALE_NOTE = "Distancias a escala %s del simulador. Rumbos verdaderos. Elevaciones y altitudes en pies." % (
    "1:%g" % (1 / S) if S != 1 else "1:1")


def page_frame(c, W, H, m=6 * mm):
    c.setStrokeColor(black)
    c.setLineWidth(0.8)
    c.rect(m, m, W - 2 * m, H - 2 * m)


# ================================================================ CARTA EN RUTA (A3)
def draw_mora_digits(c, x, y, val, color):
    th, hu = val // 1000, (val % 1000) // 100
    c.saveState()
    c.setFillColor(color)
    c.setFont("Helvetica-Bold", 11)
    s = "%d" % th
    c.drawString(x, y, s)
    w = c.stringWidth(s, "Helvetica-Bold", 11)
    c.setFont("Helvetica-Bold", 7)
    c.drawString(x + w + 0.3, y + 4.2, "%d" % hu)
    c.restoreState()


def enroute(fn):
    W, H = landscape(A3)
    c = canvas.Canvas(fn, pagesize=(W, H))
    c.setTitle("QFS - Carta de navegación en ruta - Península Ibérica")
    c.setAuthor("QFS")
    m = 8 * mm
    # limites: todo el escenario util (sin el interior de Francia ni Marruecos)
    xs = [I.P(43.9, -9.6)[0], I.P(40, 4.6)[0], I.P(28.6, -18.3)[0]]
    ys = [I.P(44.1, -3)[1], I.P(27.5, -16)[1]]
    v = View(c, m, m + 4 * mm, W - 2 * m, H - 2 * m - 22 * mm, min(xs), min(ys), max(xs), max(ys))
    c.saveState()
    v.clip()
    c.setFillColor(C_SEA)
    c.rect(v.px, v.py, v.pw, v.ph, fill=1, stroke=0)
    view_box = box(v.x0, v.y0, v.x1, v.y1)
    v.geom(LAND.intersection(view_box), fill=C_LAND, stroke=C_COAST, width=0.6)
    # relieve: conos sombreados
    for x, y, r, h in sorted(CONES, key=lambda t: t[3]):
        col = Color(0.62, 0.45, 0.28, alpha=0.10 + 0.20 * min(1, h / 3500))
        c.setFillColor(col)
        px, py = v.pt(x, y)
        c.circle(px, py, r * v.k, stroke=0, fill=1)
    # reticula geografica (peninsula) y MORA
    c.setStrokeColor(C_GRAT)
    c.setLineWidth(0.25)
    c.setDash(2, 2)
    for lat in range(34, 46):
        c.line(*v.pt(*I.P(lat, -11)), *v.pt(*I.P(lat, 6)))
    for lon in range(-11, 6):
        c.line(*v.pt(*I.P(33.6, lon)), *v.pt(*I.P(46, lon)))
    c.setDash()
    for lat in range(35, 45):
        for lon in range(-10, 5):
            a, b = I.P(lat, lon), I.P(lat + 1, lon + 1)
            cell = box(a[0], a[1], b[0], b[1])
            if not LAND.intersects(cell):
                continue
            h = 0
            for x, y, r, ch in CONES:
                if cell.distance(Point(x, y)) < r:
                    h = max(h, ch)
            f = ft(h)
            f += 2000 if f > 5000 else 1000
            f = int(math.ceil(f / 100.0) * 100)
            px, py = v.pt((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            draw_mora_digits(c, px - 5, py - 4, f, Color(0.35, 0.47, 0.6, alpha=0.55))
    for lat in range(35, 45):
        text(c, v.pt(*I.P(lat, -10.8))[0] + 1, v.pt(*I.P(lat, -10.8))[1] + 1, "N%02d°" % lat, 5, color=C_GRAT)
    for lon in range(-10, 5):
        px, py = v.pt(*I.P(44.0, lon))
        text(c, px + 1, py - 6, "%s%03d°" % ("W" if lon < 0 else "E", abs(lon)), 5, color=C_GRAT)
    # rios
    for name, pts in RIVERS:
        v.poly(pts, stroke=C_RIVER, width=0.5, close=False)
    # sistemas montanosos y picos
    for name, (la, lo) in RANGES.items():
        px, py = v.pt(*I.P(la, lo))
        text(c, px, py, name, 6.5, "Helvetica-BoldOblique", HexColor("#7a5530"), "c")
    for name, la, lo, h in PEAKS:
        px, py = v.pt(*I.P(la, lo))
        peak(c, px, py, h, name, size=5)
    # ciudades (punto y nombre)
    for name, x, y, w, d in CITIES:
        px, py = v.pt(x, y)
        c.setFillColor(HexColor("#d9b44a"))
        c.circle(px, py, 1.3, stroke=0, fill=1)
        text(c, px + 2, py + 1.5, name, 4.6, color=HexColor("#555555"))
    # aerovias
    pos = {k: (vv[2], vv[3]) for k, vv in NAVS.items()}
    c.setLineWidth(0.7)
    for name, seq in AIRWAYS:
        for a, b in zip(seq, seq[1:]):
            pa, pb = pos[a], pos[b]
            c.setStrokeColor(C_IFR)
            c.line(*v.pt(*pa), *v.pt(*pb))
            mx, my = v.pt((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2)
            ang = math.degrees(math.atan2(v.pt(*pb)[1] - v.pt(*pa)[1], v.pt(*pb)[0] - v.pt(*pa)[0]))
            if ang > 90:
                ang -= 180
            if ang < -90:
                ang += 180
            c.saveState()
            c.translate(mx, my)
            c.rotate(ang)
            w = c.stringWidth(name, "Helvetica-Bold", 5.5) + 3
            c.setFillColor(C_IFR)
            c.rect(-w / 2, 1, w, 6.2, fill=1, stroke=0)
            text(c, 0, 2.6, name, 5.5, "Helvetica-Bold", white, "c")
            text(c, 0, -5.6, "%d" % round(dist(pa, pb) / NM), 5.5, "Helvetica-Bold", C_IFR, "c")
            c.restoreState()
            # rumbos en cada extremo
            for p0, p1 in ((pa, pb), (pb, pa)):
                qx = p0[0] + (p1[0] - p0[0]) * 0.22
                qy = p0[1] + (p1[1] - p0[1]) * 0.22
                px, py = v.pt(qx, qy)
                c.saveState()
                c.translate(px, py)
                c.rotate(ang)
                text(c, 0, 1.6, "%03d°" % round(brg(p0, p1)), 4.4, color=C_IFR, align="c")
                c.restoreState()
    # radioayudas
    for ident, (typ, f, x, y) in NAVS.items():
        px, py = v.pt(x, y)
        if typ == "VOR":
            compass_rose(c, px, py, 9)
            vor_symbol(c, px, py, 1.8)
            bw, bh = tbox(c, px + 6, py + 5, ["%s %.2f %s" % (VOR_NAMES.get(ident, ident), f, ident)], 5)
            morse(c, px + 7, py + 3.3, ident)
        else:
            ndb_symbol(c, px, py, 2.4)
            tbox(c, px + 4, py + 3, ["%s %d %s" % (NDB_NAMES.get(ident, ident), f, ident)], 4.8, C_NDB)
            morse(c, px + 5, py + 1.5, ident, C_NDB)
    # aeropuertos
    for L in APTS:
        px, py = v.pt(*L["arp"])
        c.setFillColor(HexColor("#2a7d3e"))
        c.setStrokeColor(HexColor("#2a7d3e"))
        c.circle(px, py, 3.2, stroke=1, fill=0)
        c.setLineWidth(1.1)
        for rw in L["runways"]:
            h = math.radians(rw["hdg"])
            c.line(px - math.sin(h) * 2.5, py - math.cos(h) * 2.5, px + math.sin(h) * 2.5, py + math.cos(h) * 2.5)
        c.setLineWidth(0.5)
        tbox(c, px - 5, py - 13, ["%s %s" % (L["icao"], L["name"]), "ILS %.2f  TWR %.2f" % (L["runways"][0]["ils"], L["twr"])],
             4.8, HexColor("#2a7d3e"), align="r")
    # recuadro de Canarias
    a, b = I.P(27.45, -18.35), I.P(29.45, -13.2)
    a0, b0 = v.pt(*a), v.pt(*b)
    c.setStrokeColor(black)
    c.setLineWidth(0.9)
    c.rect(a0[0], a0[1], b0[0] - a0[0], b0[1] - a0[1], fill=0, stroke=1)
    text(c, a0[0] + 2, b0[1] - 7, "ISLAS CANARIAS (recuadro desplazado: no está en su posición real)", 6,
         "Helvetica-Bold")
    c.setStrokeColor(C_GRAT)
    c.setLineWidth(0.25)
    c.setDash(2, 2)
    for lat in (28, 29):
        p1, p2 = I.P(lat, -18.3), I.P(lat, -13.25)
        c.line(*v.pt(*p1), *v.pt(*p2))
        text(c, v.pt(*p1)[0] + 1, v.pt(*p1)[1] + 1, "N%02d°" % lat, 5, color=C_GRAT)
    for lon in range(-18, -12):
        p1, p2 = I.P(27.5, lon), I.P(29.4, lon)
        c.line(*v.pt(*p1), *v.pt(*p2))
        text(c, v.pt(*p2)[0] + 1, v.pt(*p2)[1] - 6, "W%03d°" % abs(lon), 5, color=C_GRAT)
    c.setDash()
    c.restoreState()
    c.setStrokeColor(black)
    c.setLineWidth(0.8)
    c.rect(v.px, v.py, v.pw, v.ph)
    # cajetin superior
    top = H - m
    c.setFillColor(C_IFR)
    c.rect(m, top - 14 * mm, W - 2 * m, 14 * mm, fill=1, stroke=0)
    text(c, m + 4 * mm, top - 7 * mm, "CARTA DE NAVEGACIÓN EN RUTA  ·  PENÍNSULA IBÉRICA, BALEARES Y CANARIAS", 15,
         "Helvetica-Bold", white)
    text(c, m + 4 * mm, top - 11.5 * mm, "QFS – Simulador de vuelo para Sinclair QL  ·  ENR 1  ·  " + SCALE_NOTE, 7.5,
         color=white)
    text(c, W - m - 4 * mm, top - 7 * mm, "ENR 1", 15, "Helvetica-Bold", white, "r")
    text(c, W - m - 4 * mm, top - 11.5 * mm, DISCLAIMER, 7, "Helvetica-Bold", HexColor("#ffd166"), "r")
    # leyenda
    lx, ly = W - m - 66 * mm, m + 8 * mm
    c.setFillColor(white)
    c.setStrokeColor(black)
    c.setLineWidth(0.5)
    c.rect(lx, ly, 62 * mm, 62 * mm, fill=1, stroke=1)
    text(c, lx + 3, ly + 62 * mm - 9, "LEYENDA", 8, "Helvetica-Bold")
    yy = ly + 62 * mm - 20
    vor_symbol(c, lx + 8, yy + 2, 1.8)
    text(c, lx + 16, yy, "VOR/DME  (nombre, frecuencia MHz, ident, Morse)", 5.5)
    yy -= 12
    ndb_symbol(c, lx + 8, yy + 2, 2.2)
    text(c, lx + 16, yy, "NDB  (nombre, frecuencia kHz, ident)", 5.5)
    yy -= 12
    c.setStrokeColor(HexColor("#2a7d3e"))
    c.circle(lx + 8, yy + 2, 3, stroke=1, fill=0)
    text(c, lx + 16, yy, "Aeropuerto con ILS (OACI, frecuencias ILS y TWR)", 5.5)
    yy -= 12
    c.setFillColor(C_IFR)
    c.rect(lx + 3, yy + 1.2, 10, 4.5, fill=1, stroke=0)
    text(c, lx + 8, yy + 2.3, "A1", 4.5, "Helvetica-Bold", white, "c")
    text(c, lx + 16, yy, "Aerovía: designador, distancia NM, rumbo en cada tramo", 5.5)
    yy -= 12
    peak(c, lx + 8, yy + 2, 3404 / 3.28084 * 3.28084 / 3.28084)
    text(c, lx + 22, yy, "Punto culminante (pies)", 5.5)
    yy -= 13
    draw_mora_digits(c, lx + 3, yy - 1, 11500, Color(0.35, 0.47, 0.6, alpha=0.7))
    text(c, lx + 16, yy, "Altitud mínima de cuadrícula (1°x1°), 11.500 ft", 5.5)
    yy -= 12
    c.setFillColor(Color(0.62, 0.45, 0.28, alpha=0.3))
    c.circle(lx + 8, yy + 2, 3.5, stroke=0, fill=1)
    text(c, lx + 16, yy, "Relieve del simulador (conos montañosos)", 5.5)
    yy -= 12
    text(c, lx + 3, yy, "Altitud de transición: 6000 ft. Mundo plano a cota 0.", 5.2, "Helvetica-Oblique")
    yy -= 7
    text(c, lx + 3, yy, "Frecuencias y posiciones aproximadas.", 5.2, "Helvetica-Oblique")
    scale_bar(c, lx + 4, ly + 9 + 4, v.k, 50, km=False)
    text(c, lx + 4, ly + 4, "NM del simulador", 5, "Helvetica-Oblique")
    c.showPage()
    c.save()


# ================================================================ AERODROMO (A5)
def a5_header(c, W, H, L, title, code):
    m = 6 * mm
    c.setFillColor(black)
    c.rect(m, H - m - 13 * mm, W - 2 * m, 13 * mm, fill=1, stroke=0)
    text(c, m + 3 * mm, H - m - 6 * mm, L["icao"], 14, "Helvetica-Bold", white)
    text(c, m + 3 * mm, H - m - 11 * mm, L["name"], 7.5, "Helvetica-Bold", HexColor("#ffd166"))
    text(c, W - m - 3 * mm, H - m - 6 * mm, title, 9, "Helvetica-Bold", white, "r")
    text(c, W - m - 3 * mm, H - m - 11 * mm, code, 6.5, color=white, align="r")
    # banda de comunicaciones
    y = H - m - 13 * mm - 7 * mm
    c.setFillColor(HexColor("#eeeeee"))
    c.rect(m, y, W - 2 * m, 7 * mm, fill=1, stroke=0)
    rw = L["runways"][0]
    items = ["ATIS %.2f" % L["atis"], "TWR %.2f" % L["twr"], "%s %.2f" % (L["vor"][0], L["vor"][1])]
    if rw["ils"]:
        items.append("ILS %s %.2f" % (rw["num"], rw["ils"]))
    items.append("ELEV 0 ft")
    x = m + 2 * mm
    for s in items:
        text(c, x, y + 2.4 * mm, s, 6.5, "Helvetica-Bold")
        x += c.stringWidth(s, "Helvetica-Bold", 6.5) + 5 * mm
    return y


def a5_footer(c, W, L, page):
    m = 6 * mm
    text(c, m, m - 4, DISCLAIMER, 5, "Helvetica-Bold", HexColor("#a00000"))
    text(c, W - m, m - 4, "QFS · AD 2-%s-%d" % (L["icao"], page), 5, align="r")


def aerodrome(c, L):
    W, H = A5
    m = 6 * mm
    ytop = a5_header(c, W, H, L, "PLANO DE AERÓDROMO", "AD 2-%s-1 · ADC" % L["icao"])
    page_frame(c, W, H)
    # extension: pistas, calles, torre (+ margen)
    pts = []
    for rw in L["runways"]:
        pts += [rw["thr"], rw["end"]]
    for _, p, _w in L["taxiways"]:
        pts += p
    pts.append(L["tower"])
    x0, x1 = min(p[0] for p in pts) - 500, max(p[0] for p in pts) + 500
    y0, y1 = min(p[1] for p in pts) - 500, max(p[1] for p in pts) + 500
    mapy0 = m + 34 * mm
    v = View(c, m + 2 * mm, mapy0, W - 2 * m - 4 * mm, ytop - mapy0 - 2 * mm, x0, y0, x1, y1)
    c.saveState()
    v.clip()
    c.setFillColor(HexColor("#f4f6ee"))
    c.rect(v.px, v.py, v.pw, v.ph, fill=1, stroke=0)
    vb = box(v.x0, v.y0, v.x1, v.y1)
    sea = vb.difference(LAND)
    v.geom(sea, fill=C_SEA, stroke=C_COAST, width=0.4)
    # plataforma
    for (ax, ay), aL, aD, ah in L["aprons"]:
        h = math.radians(ah)
        d, r = (math.sin(h), math.cos(h)), (math.cos(h), -math.sin(h))
        q = [(ax + sx * d[0] * aL / 2 + sy * r[0] * aD / 2, ay + sx * d[1] * aL / 2 + sy * r[1] * aD / 2)
             for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        v.poly(q, fill=C_APRON, stroke=HexColor("#777777"), width=0.4)
        px, py = v.pt(ax, ay)
        text(c, px, py - 1.5, "PLATAFORMA", 4.5, "Helvetica-Bold", HexColor("#444444"), "c")
        # edificio terminal (como en el escenario)
        tx = L["tower"][0] - math.sin(math.radians(L["runways"][0]["hdg"])) * 250
        ty = L["tower"][1] - math.cos(math.radians(L["runways"][0]["hdg"])) * 250
        px, py = v.pt(tx, ty)
        c.setFillColor(HexColor("#555555"))
        c.circle(px, py, 1.6, stroke=0, fill=1)
        text(c, px + 2.5, py - 1.5, "TERMINAL", 4.5, "Helvetica-Bold", HexColor("#444444"))
    # calles de rodaje
    for name, p, w in L["taxiways"]:
        g = LineString(p).buffer(w / 2, cap_style=2)
        v.geom(g, fill=C_TWY, stroke=HexColor("#6f6f6f"), width=0.3)
    # pistas
    for rw in L["runways"]:
        thr, end, d, r = rw["thr"], rw["end"], rw["d"], rw["r"]
        hw = rw["wid"] / 2
        q = [(thr[0] - r[0] * hw, thr[1] - r[1] * hw), (thr[0] + r[0] * hw, thr[1] + r[1] * hw),
             (end[0] + r[0] * hw, end[1] + r[1] * hw), (end[0] - r[0] * hw, end[1] - r[1] * hw)]
        v.poly(q, fill=black, stroke=black, width=0.3)
        # eje
        c.setStrokeColor(white)
        c.setLineWidth(0.3)
        c.setDash(1.5, 1.5)
        c.line(*v.pt(*thr), *v.pt(*end))
        c.setDash()
        ang = math.degrees(math.atan2(v.pt(*end)[1] - v.pt(*thr)[1], v.pt(*end)[0] - v.pt(*thr)[0]))
        # designadores en cada cabecera
        for p0, num, sgn in ((thr, rw["num"], -1), (end, rw["rnum"], 1)):
            px, py = v.pt(p0[0] + d[0] * sgn * 260, p0[1] + d[1] * sgn * 260)
            c.saveState()
            c.translate(px, py)
            w = c.stringWidth(num, "Helvetica-Bold", 7) + 3
            c.setFillColor(white)
            c.setStrokeColor(black)
            c.rect(-w / 2, -3, w, 8, fill=1, stroke=1)
            text(c, 0, -0.8, num, 7, "Helvetica-Bold", black, "c")
            c.restoreState()
        # rumbo y dimensiones a lo largo de la pista
        mid = v.pt(*rw["c"])
        c.saveState()
        c.translate(mid[0], mid[1])
        a2 = ang if -90 <= ang <= 90 else ang + 180
        c.rotate(a2)
        text(c, 0, 4.5, "%d x %d m" % (rw["len"], rw["wid"]), 5.5, "Helvetica-Bold", black, "c")
        c.restoreState()
        for p0, hd, sgn in ((thr, rw["hdg"], 1), (end, (rw["hdg"] + 180) % 360, -1)):
            px, py = v.pt(p0[0] + d[0] * sgn * 700 - r[0] * 120, p0[1] + d[1] * sgn * 700 - r[1] * 120)
            text(c, px, py, "%03d°" % hd, 5, "Helvetica-Bold", HexColor("#333333"), "c")
        if rw["ils"]:
            for key, lab in (("gs", "GP"), ("loc", "LOC")):
                gx, gy = rw[key]
                px, py = v.pt(gx - r[0] * 160, gy - r[1] * 160)
                c.setFillColor(C_MAG)
                c.rect(px - 1.2, py - 1.2, 2.4, 2.4, fill=1, stroke=0)
                text(c, px + 2, py - 1.2, lab, 4.5, "Helvetica-Bold", C_MAG)
    # nombres de calles (estilo cartel: fondo negro, letra amarilla)
    for name, p, w in L["taxiways"]:
        if len(name) == 1:
            q = (p[0][0] * 0.62 + p[1][0] * 0.38, p[0][1] * 0.62 + p[1][1] * 0.38)
        else:
            q = ((p[0][0] + p[1][0]) / 2, (p[0][1] + p[1][1]) / 2)
        px, py = v.pt(*q)
        w = c.stringWidth(name, "Helvetica-Bold", 5) + 2
        c.setFillColor(black)
        c.rect(px - w / 2, py - 2.6, w, 5.6, fill=1, stroke=0)
        text(c, px, py - 1.4, name, 5, "Helvetica-Bold", HexColor("#ffd400"), "c")
    # torre y VOR
    px, py = v.pt(*L["tower"])
    c.setFillColor(HexColor("#2a2a2a"))
    c.rect(px - 1.5, py - 1.5, 3, 3, fill=1, stroke=0)
    text(c, px + 2.5, py - 1.5, "TWR", 5, "Helvetica-Bold")
    vx, vy = L["vor"][2]
    if v.inside(vx, vy):
        px, py = v.pt(vx, vy)
        vor_symbol(c, px, py, 1.8)
        text(c, px + 4, py - 1.5, "%s %.2f" % (L["vor"][0], L["vor"][1]), 5, "Helvetica-Bold", C_IFR)
    c.restoreState()
    c.setStrokeColor(black)
    c.setLineWidth(0.5)
    c.rect(v.px, v.py, v.pw, v.ph)
    north_arrow(c, v.px + v.pw - 8 * mm, v.py + v.ph - 13 * mm, 9, "N")
    # escala en metros
    c.saveState()
    L1 = 1000 * v.k
    c.setLineWidth(0.6)
    c.line(v.px + 4 * mm, v.py + 4 * mm, v.px + 4 * mm + L1, v.py + 4 * mm)
    for i in range(5):
        c.line(v.px + 4 * mm + i * L1 / 4, v.py + 4 * mm, v.px + 4 * mm + i * L1 / 4, v.py + 5 * mm)
    text(c, v.px + 4 * mm + L1 + 2, v.py + 3.4 * mm, "1000 m", 5)
    c.restoreState()
    # tabla de pistas
    lat, lon = L["lat"], L["lon"]
    y = m + 30 * mm
    text(c, m + 2 * mm, y, "ARP %s %s (posición real aproximada)" % (dms(lat, "lat"), dms(lon, "lon")), 5.8, "Helvetica-Bold")
    y -= 5.5 * mm
    cols = [m + 2 * mm, m + 15 * mm, m + 32 * mm, m + 58 * mm, m + 82 * mm, m + 108 * mm]
    hdrs = ["RWY", "RUMBO V", "DIMENSIONES", "TORA / LDA", "ILS", "AYUDAS"]
    c.setFillColor(HexColor("#eeeeee"))
    c.rect(m + 1 * mm, y - 1.5, W - 2 * m - 2 * mm, 7, fill=1, stroke=0)
    for x, s in zip(cols, hdrs):
        text(c, x, y, s, 5.5, "Helvetica-Bold")
    for rw in L["runways"]:
        for num, hd, ils in ((rw["num"], rw["hdg"], rw["ils"]), (rw["rnum"], (rw["hdg"] + 180) % 360, None)):
            y -= 4.2 * mm
            vals = [num, "%03d°" % hd, "%d x %d m" % (rw["len"], rw["wid"]), "%d m" % rw["len"],
                    ("%.2f" % ils) if ils else "-", "ILS CAT I, PAPI no" if ils else "VOR/DME %s" % L["vor"][0]]
            for x, s in zip(cols, vals):
                text(c, x, y, s, 5.5)
    text(c, m + 2 * mm, m + 3 * mm, "Calles de rodaje: 23 m de anchura. Plataforma 500 x 120 m. Luces de borde, umbral y extremo de pista.",
         4.8, "Helvetica-Oblique")
    a5_footer(c, W, L, 1)
    c.showPage()


# ---------------------------------------------------------------- procedimientos
def neighbours(ident):
    r = []
    for name, seq in AIRWAYS:
        for a, b in zip(seq, seq[1:]):
            if a == ident and b not in [x[0] for x in r]:
                r.append((b, name))
            if b == ident and a not in [x[0] for x in r]:
                r.append((a, name))
    return r[:4]


def turn_dir(h_from, h_to):
    d = (h_to - h_from + 540) % 360 - 180
    return "derecha" if d > 0 else "izquierda"


def proc_view(c, W, ytop, pts, m, bottom=46):
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    span = max(x1 - x0, y1 - y0) * 0.12 + 8000
    mapy0 = m + bottom * mm
    v = View(c, m + 2 * mm, mapy0, W - 2 * m - 4 * mm, ytop - mapy0 - 2 * mm, x0 - span, y0 - span, x1 + span, y1 + span)
    c.saveState()
    v.clip()
    c.setFillColor(white)
    c.rect(v.px, v.py, v.pw, v.ph, fill=1, stroke=0)
    vb = box(v.x0, v.y0, v.x1, v.y1)
    v.geom(vb.difference(LAND), fill=HexColor("#e8f2f9"), stroke=HexColor("#9fbfd8"), width=0.4)
    for x, y, r, h in CONES:
        if v.inside(x, y, r):
            c.setFillColor(Color(0.62, 0.45, 0.28, alpha=0.12))
            c.circle(*v.pt(x, y), r * v.k, stroke=0, fill=1)
    return v


def proc_end(c, v):
    c.restoreState()
    c.setStrokeColor(black)
    c.setLineWidth(0.5)
    c.rect(v.px, v.py, v.pw, v.ph)


def arrow_line(c, v, a, b, color=black, width=0.9, dash=None, head=True):
    c.saveState()
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(width)
    if dash:
        c.setDash(*dash)
    pa, pb = v.pt(*a), v.pt(*b)
    c.line(*pa, *pb)
    c.setDash()
    if head:
        ang = math.atan2(pb[1] - pa[1], pb[0] - pa[0])
        mx, my = (pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2
        p = c.beginPath()
        p.moveTo(mx + 3 * math.cos(ang), my + 3 * math.sin(ang))
        p.lineTo(mx - 2 * math.cos(ang) + 1.6 * math.sin(ang), my - 2 * math.sin(ang) - 1.6 * math.cos(ang))
        p.lineTo(mx - 2 * math.cos(ang) - 1.6 * math.sin(ang), my - 2 * math.sin(ang) + 1.6 * math.cos(ang))
        p.close()
        c.drawPath(p, fill=1, stroke=0)
    c.restoreState()


def along_label(c, v, a, b, s, t=0.5, off=3, size=5, color=black, bold=True):
    pa, pb = v.pt(*a), v.pt(*b)
    ang = math.degrees(math.atan2(pb[1] - pa[1], pb[0] - pa[0]))
    if ang > 90:
        ang -= 180
    if ang < -90:
        ang += 180
    c.saveState()
    c.translate(pa[0] + (pb[0] - pa[0]) * t, pa[1] + (pb[1] - pa[1]) * t)
    c.rotate(ang)
    text(c, 0, off, s, size, "Helvetica-Bold" if bold else "Helvetica", color, "c")
    c.restoreState()


def fix_tri(c, x, y, color=black, fill=True):
    c.saveState()
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(0.6)
    p = c.beginPath()
    p.moveTo(x, y + 2.2)
    p.lineTo(x - 2, y - 1.3)
    p.lineTo(x + 2, y - 1.3)
    p.close()
    c.drawPath(p, fill=1 if fill else 0, stroke=1)
    c.restoreState()


def draw_nav(c, v, ident, big=False):
    typ, f, x, y = NAVS[ident]
    px, py = v.pt(x, y)
    if typ == "VOR":
        if big:
            compass_rose(c, px, py, 11)
        vor_symbol(c, px, py, 2)
        tbox(c, px + 4, py + 3, ["%s" % VOR_NAMES.get(ident, ident), "%.2f %s" % (f, ident)], 5, C_IFR)
        morse(c, px + 5, py + 1.2, ident)
    else:
        ndb_symbol(c, px, py, 2.4)
        tbox(c, px + 4, py + 3, [NDB_NAMES.get(ident, ident), "%d %s" % (f, ident)], 5, C_NDB)


def sid_page(c, L):
    W, H = A5
    m = 6 * mm
    ytop = a5_header(c, W, H, L, "SALIDAS NORMALIZADAS (SID)", "AD 2-%s-2 · SID RWY %s" % (L["icao"], L["runways"][0]["num"]))
    page_frame(c, W, H)
    rw = L["runways"][0]
    vid = L["vor"][0]
    vpos = L["vor"][2]
    nb = neighbours(vid)
    mins = msa(vpos)
    # punto de viraje: 5 NM en rumbo de pista desde el extremo de la pista
    tp = (rw["end"][0] + rw["d"][0] * 5 * NM, rw["end"][1] + rw["d"][1] * 5 * NM)
    pts = [rw["thr"], rw["end"], tp, vpos] + [(NAVS[n][2], NAVS[n][3]) for n, _ in nb]
    v = proc_view(c, W, ytop, pts, m)
    # pista
    c.setStrokeColor(black)
    c.setLineWidth(2.2)
    c.line(*v.pt(*rw["thr"]), *v.pt(*rw["end"]))
    arrow_line(c, v, rw["end"], tp, width=1)
    along_label(c, v, rw["end"], tp, "%03d°" % rw["hdg"], off=2.5)
    px, py = v.pt(*tp)
    c.setFillColor(white)
    c.setStrokeColor(black)
    c.circle(px, py, 1.6, fill=1, stroke=1)
    text(c, px + 3, py - 4.5, "D%.1f %s / 3000" % (dist(vpos, tp) / NM, vid), 4.8, "Helvetica-Bold")
    names = []
    for k, (n, awy) in enumerate(nb):
        tgt = (NAVS[n][2], NAVS[n][3])
        arrow_line(c, v, tp, tgt, width=0.9)
        nm = dist(tp, tgt) / NM
        crs = brg(tp, tgt)
        pname = "%s 1%s" % (n, "ABCD"[0])
        names.append((pname, n, crs, nm, awy, turn_dir(rw["hdg"], crs)))
        along_label(c, v, tp, tgt, "%s  %03d°  %d NM" % (pname, crs, nm), t=0.55, off=2.5, size=5)
        draw_nav(c, v, n)
    draw_nav(c, v, vid, big=True)
    px, py = v.pt(*L["arp"])
    text(c, px - 3, py - 8, L["icao"], 6, "Helvetica-Bold", align="r")
    proc_end(c, v)
    north_arrow(c, v.px + v.pw - 8 * mm, v.py + v.ph - 13 * mm, 8)
    # cuadro MSA
    tbox(c, v.px + 2 * mm, v.py + v.ph - 12 * mm, ["MSA 25 NM %s" % vid, "%d ft" % mins], 6, black)
    # descripcion
    y = m + 42 * mm
    text(c, m + 2 * mm, y, "PISTA %s · Altitud de transición 6000 ft · Velocidad máxima 250 KT por debajo de FL100" % rw["num"],
         5.6, "Helvetica-Bold")
    y -= 2
    for pname, n, crs, nm, awy, td in names:
        y -= 7 * mm
        c.setFillColor(HexColor("#eeeeee"))
        c.rect(m + 1 * mm, y - 1.2 * mm, W - 2 * m - 2 * mm, 6.5 * mm, fill=1, stroke=0)
        text(c, m + 2 * mm, y + 2.2 * mm, pname, 6.5, "Helvetica-Bold")
        alt = max(6000, mins)
        s1 = "Subir en rumbo de pista %03d° hasta D%.1f %s y 3000 ft; virar a la %s directo a %s" % (
            rw["hdg"], dist(vpos, tp) / NM, vid, td, n)
        s2 = "(rumbo %03d°, %d NM). Altitud inicial %d ft, enlace con la aerovía %s." % (crs, nm, alt, awy)
        text(c, m + 19 * mm, y + 2.6 * mm, s1, 4.8)
        text(c, m + 19 * mm, y + 0.2 * mm, s2, 4.8)
    a5_footer(c, W, L, 2)
    c.showPage()


def star_page(c, L):
    W, H = A5
    m = 6 * mm
    rw = L["runways"][0]
    ytop = a5_header(c, W, H, L, "LLEGADAS (STAR) E ILS", "AD 2-%s-3 · STAR / ILS RWY %s" % (L["icao"], rw["num"]))
    page_frame(c, W, H)
    vid = L["vor"][0]
    vpos = L["vor"][2]
    nb = neighbours(vid)
    mins = msa(vpos)
    d = rw["d"]
    IF = (rw["thr"][0] - d[0] * 9 * NM, rw["thr"][1] - d[1] * 9 * NM)
    pts = [rw["thr"], rw["end"], IF, vpos] + [(NAVS[n][2], NAVS[n][3]) for n, _ in nb]
    nlines = len(nb)
    v = proc_view(c, W, ytop, pts, m, bottom=56 + 2.3 * nlines)
    # rutas de llegada
    names = []
    iaf_alt = max(6000, mins)
    for n, awy in nb:
        src = (NAVS[n][2], NAVS[n][3])
        arrow_line(c, v, src, vpos, width=0.9)
        crs = brg(src, vpos)
        nm = dist(src, vpos) / NM
        pname = "%s 1%s" % (n, "B")
        names.append((pname, n, crs, nm, awy))
        along_label(c, v, src, vpos, "%s  %03d°  %d NM" % (pname, crs, nm), t=0.45, off=2.5, size=5)
        draw_nav(c, v, n)
    # del VOR al IF y aproximacion final
    arrow_line(c, v, vpos, IF, width=0.9, dash=(3, 2))
    along_label(c, v, vpos, IF, "%03d°  %d NM  3000" % (brg(vpos, IF), dist(vpos, IF) / NM), off=2.5, size=5)
    c.setStrokeColor(C_MAG)
    c.setLineWidth(1.4)
    c.line(*v.pt(*IF), *v.pt(*rw["thr"]))
    # pluma del localizador
    fx, fy = v.pt(IF[0] - d[0] * 2 * NM, IF[1] - d[1] * 2 * NM)
    c.setLineWidth(0.4)
    c.line(*v.pt(*IF), fx, fy)
    fix_tri(c, *v.pt(*IF))
    px, py = v.pt(*IF)
    text(c, px + 3, py + 2, "IF %s  D9.0  3000" % rw["num"], 5, "Helvetica-Bold")
    if rw["ils"]:
        px, py = v.pt(*rw["om"])
        fix_tri(c, px, py, C_MAG, fill=False)
        text(c, px + 3, py - 5, "FAF/OM", 4.8, "Helvetica-Bold", C_MAG)
    c.setStrokeColor(black)
    c.setLineWidth(2.2)
    c.line(*v.pt(*rw["thr"]), *v.pt(*rw["end"]))
    draw_nav(c, v, vid, big=True)
    text(c, v.pt(*vpos)[0] + 4, v.pt(*vpos)[1] - 8, "IAF  %d" % iaf_alt, 5, "Helvetica-Bold")
    proc_end(c, v)
    north_arrow(c, v.px + v.pw - 8 * mm, v.py + v.ph - 13 * mm, 8)
    tbox(c, v.px + 2 * mm, v.py + v.ph - 12 * mm, ["MSA 25 NM %s" % vid, "%d ft" % mins], 6, black)
    # perfil ILS
    y0 = m + 8 * mm
    x0, x1 = m + 4 * mm, W - m - 30 * mm
    hpx = 26 * mm
    c.setStrokeColor(black)
    c.setLineWidth(0.4)
    c.line(x0, y0, x1, y0)
    dist_if = 9 * NM
    def X(dm):            # distancia al umbral (m) -> x
        return x1 - dm / dist_if * (x1 - x0)
    def Y(ftv):
        return y0 + ftv / 3200.0 * hpx
    gs_h = lambda dm: (dm + 300) * math.tan(math.radians(3)) * 3.28084
    c.setStrokeColor(C_MAG)
    c.setLineWidth(1.1)
    c.line(X(dist_if), Y(3000), X(dist_if * 0.0 + (3000 / 3.28084) / math.tan(math.radians(3)) - 300), Y(3000))
    p = c.beginPath()
    dd = (3000 / 3.28084) / math.tan(math.radians(3)) - 300
    p.moveTo(X(min(dd, dist_if)), Y(gs_h(min(dd, dist_if))))
    p.lineTo(X(0), Y(gs_h(0)))
    c.drawPath(p, stroke=1, fill=0)
    for dm, lab in ((7200, "OM"), (1050, "MM")):
        c.setStrokeColor(HexColor("#777777"))
        c.setLineWidth(0.3)
        c.line(X(dm), y0, X(dm), Y(gs_h(dm)))
        text(c, X(dm), y0 - 5, "%s %.1f NM" % (lab, dm / NM), 4.6, align="c")
        text(c, X(dm) + 1, Y(gs_h(dm)) + 2, "%d" % round(gs_h(dm), -1), 4.8, "Helvetica-Bold")
    text(c, X(dist_if), y0 - 5, "IF 9.0 NM", 4.6)
    text(c, X(dist_if) + 1, Y(3000) + 2, "3000", 4.8, "Helvetica-Bold")
    text(c, X(0), y0 - 10, "THR %s" % rw["num"], 4.6, align="c")
    text(c, (x0 + x1) / 2, Y(2500), "GS 3.00°  (%d ft/NM)" % round(NM * math.tan(math.radians(3)) * 3.28084), 5,
         "Helvetica-Bold", C_MAG, "c")
    bx = x1 + 3 * mm
    tbox(c, bx, y0 + 2, ["ILS %s" % rw["num"], "LOC %.2f" % rw["ils"], "CRS %03d°" % rw["hdg"], "GS 3.00°",
                         "DA 200 ft", "VIS 800 m"], 5.2, C_MAG)
    text(c, x0, y0 + hpx + 6, "FRUSTRADA: subir en rumbo %03d° a 3000 ft y volver al %s. Esperas sobre %s, virajes a derechas." % (
        rw["hdg"], vid, vid), 4.8, "Helvetica-Bold")
    # descripcion de las STAR
    y = y0 + hpx + 14
    for pname, n, crs, nm, awy in names:
        text(c, x0, y, "%s: desde %s (aerovía %s) rumbo %03d° %d NM a %s, cruzar %s a %d ft; después a IF %s a 3000 ft." % (
            pname, n, awy, crs, nm, vid, vid, iaf_alt, rw["num"]), 4.7)
        y += 6.2
    text(c, x0, y + 1, "Llegadas a la pista %s en uso. Velocidad máxima 250 KT por debajo de FL100, 180 KT desde el IAF." %
         rw["num"], 5.2, "Helvetica-Bold")
    a5_footer(c, W, L, 3)
    c.showPage()


def airports_a5(fn):
    c = canvas.Canvas(fn, pagesize=A5)
    c.setTitle("QFS - Cartas de aeropuerto (ADC, SID, STAR/ILS)")
    c.setAuthor("QFS")
    for L in APTS:
        aerodrome(c, L)
        sid_page(c, L)
        star_page(c, L)
    c.save()


# ================================================================ CARTA VISUAL (A4)
def tint(h):
    col = TINTS[0][1]
    for lim, cc in TINTS:
        if h >= lim:
            col = cc
    return col


def vfr_page(c, L):
    W, H = landscape(A4)
    m = 6 * mm
    page_frame(c, W, H)
    cx, cy = L["arp"]
    canary = is_canary(L["arp"])
    # marco del mapa: escala 1:300.000 del simulador (0.6 m de mapa = 180 km... ) -> 1 mm = 300 m
    mx0, my0 = m + 3 * mm, m + 16 * mm
    mw, mh = W - 2 * m - 6 * mm - 62 * mm, H - 2 * m - 34 * mm
    k = 1 * mm / 300.0
    hw, hh = mw / k / 2, mh / k / 2
    v = View(c, mx0, my0, mw, mh, cx - hw, cy - hh, cx + hw, cy + hh)
    vb = box(v.x0, v.y0, v.x1, v.y1)
    c.saveState()
    v.clip()
    c.setFillColor(C_VFRSEA)
    c.rect(v.px, v.py, v.pw, v.ph, fill=1, stroke=0)
    v.geom(LAND.intersection(vb), fill=C_VFRLAND, stroke=HexColor("#3a6f9e"), width=0.6)
    # tintas hipsometricas: conos en anillos
    for x, y, r, h in sorted(CONES, key=lambda t: t[3]):
        if not v.inside(x, y, r):
            continue
        for lim, col in TINTS[1:]:
            if h <= lim:
                break
            rr = r * (1 - lim / h)
            c.setFillColor(col)
            c.circle(*v.pt(x, y), rr * v.k, stroke=0, fill=1)
    # rios y carreteras
    for name, pts in RIVERS:
        v.poly(pts, stroke=C_RIVER, width=0.9, close=False)
    for name, pts in ROADS:
        v.poly(pts, stroke=HexColor("#7a1e1a"), width=1.6, close=False)
        v.poly(pts, stroke=HexColor("#f0a43a"), width=0.9, close=False)
    # ciudades
    for name, x, y, w, d in CITIES:
        if not v.inside(x, y, 3000):
            continue
        q = [(x - w / 2, y - d / 2), (x + w / 2, y - d / 2), (x + w / 2, y + d / 2), (x - w / 2, y + d / 2)]
        v.poly(q, fill=C_CITY, stroke=HexColor("#8a6d1a"), width=0.4)
        px, py = v.pt(x, y + d / 2)
        text(c, px, py + 2, name, 7, "Helvetica-Bold", black, "c")
    # picos
    for x, y, r, h in CONES:
        if v.inside(x, y) and h > 600:
            peak(c, *v.pt(x, y), h, None, size=5.5)
    for name, la, lo, h in PEAKS:
        p = I.P(la, lo)
        if v.inside(*p):
            px, py = v.pt(*p)
            text(c, px + 2, py - 7, name, 5, "Helvetica-Oblique", HexColor("#6b4b2a"))
    # CTR
    R = 12 * NM
    px, py = v.pt(cx, cy)
    c.setStrokeColor(C_MAG)
    c.setLineWidth(1.2)
    c.setDash(4, 2)
    c.circle(px, py, R * v.k, stroke=1, fill=0)
    c.setDash()
    tbox(c, px - R * v.k * 0.7, py + R * v.k * 0.72, ["CTR %s" % L["name"], "D  SFC - 3000 ft"], 6.5, C_MAG)
    # puntos de notificacion visual (N, E, S, W sobre el limite del CTR)
    for lab, ang, word in (("N", 0, "NOVEMBER"), ("E", 90, "ECHO"), ("S", 180, "SIERRA"), ("W", 270, "WHISKEY")):
        q = (cx + math.sin(math.radians(ang)) * R, cy + math.cos(math.radians(ang)) * R)
        qx, qy = v.pt(*q)
        c.setFillColor(C_MAG)
        p = c.beginPath()
        p.moveTo(qx, qy + 3.2)
        p.lineTo(qx - 2.8, qy - 1.8)
        p.lineTo(qx + 2.8, qy - 1.8)
        p.close()
        c.drawPath(p, fill=1, stroke=0)
        tbox(c, qx + 4, qy - 2, ["%s (%s)" % (lab, word), "%03d°/%d NM %s" % (ang, round(R / NM), L["vor"][0])], 5.2, C_MAG)
    # pistas del aeropuerto principal y otros aeropuertos en la carta
    for LL in APTS:
        if not v.inside(*LL["arp"], 2000):
            continue
        for rw in LL["runways"]:
            c.setStrokeColor(C_MAG)
            c.setLineWidth(max(1.2, rw["wid"] * v.k))
            c.line(*v.pt(*rw["thr"]), *v.pt(*rw["end"]))
        ax, ay = v.pt(*LL["arp"])
        c.setStrokeColor(C_MAG)
        c.setLineWidth(0.8)
        c.circle(ax, ay, 7, stroke=1, fill=0)
        tbox(c, ax - 9, ay - 16, [LL["icao"] + " " + LL["name"], "TWR %.2f  ATIS %.2f" % (LL["twr"], LL["atis"])],
             5.5, C_MAG, align="r")
        # pluma del ILS (6 NM)
        rw = LL["runways"][0]
        if rw["ils"]:
            e = (rw["thr"][0] - rw["d"][0] * 6 * NM, rw["thr"][1] - rw["d"][1] * 6 * NM)
            c.setStrokeColor(C_IFR)
            c.setLineWidth(0.5)
            pa, pb = v.pt(*rw["thr"]), v.pt(*e)
            c.line(*pa, *pb)
            ang = math.atan2(pb[1] - pa[1], pb[0] - pa[0])
            n = (math.sin(ang), -math.cos(ang))
            c.line(*pb, pb[0] + n[0] * 3 + math.cos(ang) * -6, pb[1] + n[1] * 3 + math.sin(ang) * -6)
            c.line(*pb, pb[0] - n[0] * 3 + math.cos(ang) * -6, pb[1] - n[1] * 3 + math.sin(ang) * -6)
    # radioayudas
    for ident, (typ, f, x, y) in NAVS.items():
        if not v.inside(x, y):
            continue
        px, py = v.pt(x, y)
        if typ == "VOR":
            compass_rose(c, px, py, 16)
            vor_symbol(c, px, py, 2.2)
            tbox(c, px + 18, py + 6, ["%s" % VOR_NAMES.get(ident, ident), "%.2f %s" % (f, ident)], 5.5, C_IFR)
            morse(c, px + 19, py + 4, ident)
        else:
            ndb_symbol(c, px, py, 3)
            tbox(c, px + 5, py + 3, [NDB_NAMES.get(ident, ident), "%d %s" % (f, ident)], 5.5, C_NDB)
    # reticula geografica en el borde (cada 10')
    c.restoreState()
    c.setStrokeColor(black)
    c.setLineWidth(0.8)
    c.rect(v.px, v.py, v.pw, v.ph)
    la0, lo0 = inv(v.x0, v.y0, canary)
    la1, lo1 = inv(v.x1, v.y1, canary)
    step = 10 / 60.0
    lat = math.ceil(la0 / step) * step
    while lat < la1:
        yy = v.py + (lat - la0) / (la1 - la0) * v.ph
        c.setLineWidth(0.4)
        c.line(v.px, yy, v.px + 4, yy)
        c.line(v.px + v.pw, yy, v.px + v.pw - 4, yy)
        text(c, v.px + 5, yy - 2, dms(lat, "lat")[:-3] + "'", 5)
        lat += step
    lon = math.ceil(lo0 / step) * step
    while lon < lo1:
        xx = v.px + (lon - lo0) / (lo1 - lo0) * v.pw
        c.line(xx, v.py, xx, v.py + 4)
        c.line(xx, v.py + v.ph, xx, v.py + v.ph - 4)
        text(c, xx, v.py + 5, dms(lon, "lon")[:-3] + "'", 5, align="c")
        lon += step
    # cabecera
    top = H - m
    c.setFillColor(C_MAG)
    c.rect(m, top - 12 * mm, W - 2 * m, 12 * mm, fill=1, stroke=0)
    text(c, m + 3 * mm, top - 6.5 * mm, "CARTA DE NAVEGACIÓN VISUAL  ·  %s (%s)" % (L["name"], L["icao"]), 13,
         "Helvetica-Bold", white)
    text(c, m + 3 * mm, top - 10.3 * mm, "QFS · VAC %s · Escala 1:300.000 del simulador (1 cm = 3 km del simulador) · %s" % (
        L["icao"], SCALE_NOTE), 6.5, color=white)
    text(c, W - m - 3 * mm, top - 6.5 * mm, "VAC", 13, "Helvetica-Bold", white, "r")
    text(c, W - m - 3 * mm, top - 10.3 * mm, DISCLAIMER, 6, "Helvetica-Bold", HexColor("#ffe08a"), "r")
    # columna derecha: informacion
    rx = W - m - 60 * mm
    ry = top - 16 * mm
    rw = L["runways"][0]
    blocks = [
        ("COMUNICACIONES", ["ATIS  %.2f" % L["atis"], "TWR   %.2f" % L["twr"], "Llamar en los puntos N, E, S o W",
                            "antes de entrar en el CTR."]),
        ("RADIOAYUDAS", ["%s  VOR/DME %.2f" % (L["vor"][0], L["vor"][1])] +
         (["ILS %s  %.2f  CRS %03d°" % (rw["num"], rw["ils"], rw["hdg"])] if rw["ils"] else [])),
        ("AERÓDROMO", ["Elevación 0 ft", "ARP %s" % dms(L["lat"], "lat"), "      %s" % dms(L["lon"], "lon")] +
         ["RWY %s/%s  %d x %d m" % (r["num"], r["rnum"], r["len"], r["wid"]) for r in L["runways"]]),
        ("CIRCUITO DE TRÁNSITO", ["Altitud 1000 ft, virajes a izquierdas", "salvo indicación de TWR.",
                                  "Velocidad máx. 140 KT en el CTR."]),
        ("ALTITUDES", ["MSA 25 NM %s: %d ft" % (L["vor"][0], msa(L["vor"][2])), "Espacio aéreo D: SFC-3000 ft",
                       "Por encima: espacio aéreo C."]),
    ]
    for title, lines in blocks:
        c.setFillColor(C_MAG)
        c.rect(rx, ry - 5 * mm, 60 * mm, 5 * mm, fill=1, stroke=0)
        text(c, rx + 2 * mm, ry - 3.6 * mm, title, 6.5, "Helvetica-Bold", white)
        ry -= 5 * mm
        for s in lines:
            ry -= 3.6 * mm
            text(c, rx + 2 * mm, ry, s, 6.2)
        ry -= 3 * mm
    # leyenda
    ly = m + 50 * mm
    text(c, rx, ly, "LEYENDA", 7, "Helvetica-Bold")
    items = [(C_CITY, "Ciudad"), (HexColor("#f0a43a"), "Autovía"), (C_RIVER, "Río"), (C_VFRSEA, "Mar")]
    yy = ly - 5 * mm
    for col, s in items:
        c.setFillColor(col)
        c.rect(rx, yy, 6 * mm, 2.5 * mm, fill=1, stroke=0)
        text(c, rx + 8 * mm, yy + 0.4 * mm, s, 6)
        yy -= 4 * mm
    text(c, rx, yy, "Tintas: 300 / 700 / 1200 / 1800 / 2500 m", 5.5)
    yy -= 3.2 * mm
    xx = rx
    for lim, col in TINTS:
        c.setFillColor(col)
        c.rect(xx, yy - 2.5 * mm, 9 * mm, 2.5 * mm, fill=1, stroke=0)
        xx += 9.5 * mm
    scale_bar(c, m + 6 * mm, m + 10 * mm, v.k, 10)
    north_arrow(c, v.px + v.pw - 9 * mm, v.py + v.ph - 15 * mm, 10, "N")
    c.showPage()


def vfr_a4(fn):
    W, H = landscape(A4)
    c = canvas.Canvas(fn, pagesize=(W, H))
    c.setTitle("QFS - Cartas de navegación visual (VAC)")
    c.setAuthor("QFS")
    for L in APTS:
        vfr_page(c, L)
    c.save()


def check_approaches():
    """comprueba que la senda ILS libra el relieve del simulador"""
    bad = []
    for L in APTS:
        rw = L["runways"][0]
        for i in range(1, 91):
            dm = i * 0.1 * NM
            p = (rw["thr"][0] - rw["d"][0] * dm, rw["thr"][1] - rw["d"][1] * dm)
            hg = min((dm + 300) * math.tan(math.radians(3)), 3000 / 3.28084)
            t = terrain_at(*p)
            if t > hg - min(150, hg / 2):
                bad.append((L["icao"], round(dm / NM, 1), round(t), round(hg)))
                break
    return bad


if __name__ == "__main__":
    os.makedirs(OUTDIR, exist_ok=True)
    enroute(os.path.join(OUTDIR, "QFS_Carta_Ruta_A3.pdf"))
    import cartas_a5
    n = cartas_a5.build(os.path.join(OUTDIR, "QFS_Aeropuertos_A5.pdf"))
    print("A5: %d paginas" % n)
    vfr_a4(os.path.join(OUTDIR, "QFS_Cartas_Visuales_A4.pdf"))
    print("cartas en", OUTDIR)
    for b in check_approaches():
        print("AVISO relieve en final: %s a %.1f NM: terreno %d m, senda %d m" % b)
