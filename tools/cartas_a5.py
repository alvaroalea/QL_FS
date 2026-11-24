#!/usr/bin/env python3
"""cartas_a5.py - hojas A5 por aeropuerto: datos AD 2, SID e IAC (lo llama cartas.py)."""
import math
from reportlab.lib.pagesizes import A5
from reportlab.lib.units import mm
from reportlab.lib.colors import Color, HexColor, white, black
from shapely.geometry import box

import cartas as C
from procdesign import (Designer, NM, add, sub, mul, unit, right, polar, brg, dist, point_at, plen)

text, tbox, View = C.text, C.tbox, C.View
GREY = HexColor("#eeeeee")
DARK = HexColor("#333333")
PROC = HexColor("#111111")
MA_COL = HexColor("#333333")
REF_TEMP = {"LEMD": 33.5, "LEBL": 28.7, "LEPA": 31.4, "LEIB": 30.5, "LEVC": 30.4, "LEMG": 31.0, "LEZL": 36.0,
            "LEST": 24.5, "LEAS": 22.9, "LPPT": 29.0, "GCTS": 28.5, "GCFV": 27.6, "GCLP": 27.0}
W, H = A5
M = 6 * mm


# ================================================================ cabecera y pie
def header(c, L, title, code, proc, freqs):
    c.setFillColor(black)
    c.rect(M, H - M - 13 * mm, W - 2 * M, 13 * mm, fill=1, stroke=0)
    text(c, M + 2.5 * mm, H - M - 5 * mm, "AIP-QFS", 6.5, "Helvetica-Bold", HexColor("#ffd166"))
    text(c, M + 2.5 * mm, H - M - 8.5 * mm, code, 5.5, color=white)
    text(c, M + 2.5 * mm, H - M - 11.5 * mm, "VAR 0° · TA 6000 ft", 5, color=white)
    text(c, W / 2 - 4 * mm, H - M - 5.2 * mm, title[0], 6.2, "Helvetica-Bold", white, "c")
    if len(title) > 1:
        text(c, W / 2 - 4 * mm, H - M - 8.6 * mm, title[1], 6.2, "Helvetica-Bold", white, "c")
    text(c, W - M - 2.5 * mm, H - M - 5.2 * mm, "%s  %s" % (L["name"], L["icao"]), 7, "Helvetica-Bold", white, "r")
    text(c, W - M - 2.5 * mm, H - M - 10.5 * mm, proc, 10, "Helvetica-Bold", HexColor("#ffd166"), "r")
    y = H - M - 13 * mm - 6.5 * mm
    c.setFillColor(GREY)
    c.rect(M, y, W - 2 * M, 6.5 * mm, fill=1, stroke=0)
    x = M + 2 * mm
    for s in freqs:
        text(c, x, y + 2.3 * mm, s, 5.6, "Helvetica-Bold")
        x += c.stringWidth(s, "Helvetica-Bold", 5.6) + 3.5 * mm
    C.page_frame(c, W, H)
    return y


def footer(c, code):
    text(c, M, M - 4, C.DISCLAIMER, 5, "Helvetica-Bold", HexColor("#a00000"))
    text(c, W - M, M - 4, "QFS · %s" % code, 5, align="r")


def base_freqs(L, P, extra=()):
    f = ["ATIS %.2f" % L["atis"], "TWR/APP %.2f" % L["twr"], "%s %.2f" % (P["vid"], P["vfreq"])]
    return f + list(extra) + ["ELEV 0 ft"]


# ================================================================ hoja 2 del aerodromo: tablas AD 2
def section(c, y, title):
    c.setFillColor(black)
    c.rect(M + 1 * mm, y - 1.3 * mm, W - 2 * M - 2 * mm, 4.2 * mm, fill=1, stroke=0)
    text(c, M + 2 * mm, y, title, 5.8, "Helvetica-Bold", white)
    return y - 4.6 * mm


def table(c, y, cols, rows, size=5.0, head=True, lh=3.3):
    x0 = M + 2 * mm
    for i, r in enumerate(rows):
        if head and i == 0:
            c.setFillColor(GREY)
            c.rect(M + 1 * mm, y - 1.1 * mm, W - 2 * M - 2 * mm, lh * mm, fill=1, stroke=0)
        for x, s in zip(cols, r):
            text(c, x0 + x * mm, y, s, size, "Helvetica-Bold" if (head and i == 0) else "Helvetica")
        y -= lh * mm
    return y - 0.8 * mm


def lat_lon(x, y, canary):
    la, lo = C.inv(x, y, canary)
    return dms_s(la, "lat"), dms_s(lo, "lon")


def dms_s(v, kind):
    h = ("N" if v >= 0 else "S") if kind == "lat" else ("E" if v >= 0 else "W")
    v = abs(v)
    d = int(v)
    m = int((v - d) * 60)
    s = ((v - d) * 60 - m) * 60
    return ("%02d%02d%04.1f%s" if kind == "lat" else "%03d%02d%04.1f%s") % (d, m, s, h)


def nearest_city(p):
    best = None
    for n, x, y, w, d in C.CITIES:
        dd = dist(p, (x, y))
        if best is None or dd < best[0]:
            best = (dd, n, (x, y))
    return best


def card16(b):
    names = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"]
    return names[int((b + 11.25) % 360 // 22.5)]


def adc_data(c, L, P, code, index):
    ytop = header(c, L, ("DATOS DEL AERÓDROMO", "AERODROME GEOGRAPHICAL AND ADMINISTRATIVE DATA"), code, "AD 2",
                  base_freqs(L, P))
    canary = C.is_canary(L["arp"])
    y = ytop - 4 * mm
    y = section(c, y, "AD 2.2  DATOS GEOGRÁFICOS Y DE ADMINISTRACIÓN / GEOGRAPHICAL AND ADMINISTRATIVE DATA")
    la, lo = C.dms(L["lat"], "lat"), C.dms(L["lon"], "lon")
    dd, cn, cp = nearest_city(L["arp"])
    rows = [("1", "Coordenadas ARP / ARP coordinates", "%s  %s  (real aprox.)" % (dms_s(L["lat"], "lat"), dms_s(L["lon"], "lon"))),
            ("", "Posición en el simulador / sim position", "X %+.0f m   Y %+.0f m" % L["arp"]),
            ("2", "Dirección y distancia / to city", "%.1f km %s de %s (simulador)" % (dd / 1000, card16(brg(cp, L["arp"])), cn)),
            ("3", "Elevación / temp. referencia", "0 ft (0 m)  /  %.1f °C" % REF_TEMP.get(L["icao"], 30)),
            ("4", "Ondulación geoidal / undulation", "No aplica: mundo plano a cota 0"),
            ("5", "Declinación magnética / MAG VAR", "0° (rumbos verdaderos en todo el simulador)"),
            ("6", "Administración / operator", "QFS Aeropuertos · TWR %.2f" % L["twr"]),
            ("7", "Tipo de tránsito / traffic", "IFR / VFR"),
            ("8", "Horario / hours (AD 2.3)", "H24 · ATS H24 · Combustible AVGAS 100LL y JET A1")]
    y = table(c, y, (0, 4, 48), rows, 5.0, head=False, lh=3.1)
    y = section(c, y, "AD 2.6  SALVAMENTO Y EXTINCIÓN / RESCUE AND FIREFIGHTING")
    cat = 9 if L["icao"] in ("LEMD", "LEBL", "LEPA", "GCLP", "GCTS") else 7
    y = table(c, y, (0, 48), [("Categoría OACI / ICAO category", "CAT %d (H24)" % cat)], 5.0, head=False, lh=3.1)
    y = section(c, y, "AD 2.12  CARACTERÍSTICAS FÍSICAS DE LAS PISTAS / RUNWAY PHYSICAL CHARACTERISTICS")
    rows = [("RWY", "BRG V", "DIM (m)", "PCN", "THR (coordenadas aprox.)", "THR ELEV", "SLOPE")]
    for D in P["dirs"]:
        tla, tlo = lat_lon(D["thr"][0], D["thr"][1], canary)
        rows.append((D["num"], "%03d°" % D["hdg"], "%d x %d" % (D["len"], D["rw"]["wid"]),
                     "80/F/B/W/T" if D["len"] > 3000 else "60/F/B/X/T", "%s %s" % (tla, tlo), "0 ft", "0 %"))
    y = table(c, y, (0, 9, 20, 37, 56, 99, 113), rows, 4.8, lh=3.0)
    y = section(c, y, "AD 2.13  DISTANCIAS DECLARADAS / DECLARED DISTANCES  ·  PERFIL / PROFILE")
    rows = [("RWY", "TORA", "TODA", "ASDA", "LDA")]
    for D in P["dirs"]:
        rows.append((D["num"], "%d" % D["len"], "%d" % (D["len"] + 150), "%d" % (D["len"] + 60), "%d" % D["len"]))
    yt = table(c, y, (0, 9, 21, 33, 45), rows, 4.8, lh=3.0)
    # perfil longitudinal (la pista es horizontal)
    px0, px1 = M + 62 * mm, W - M - 4 * mm
    yy = y - 1 * mm
    for rw in L["runways"]:
        c.setStrokeColor(black)
        c.setLineWidth(1.2)
        c.line(px0, yy - 3 * mm, px1, yy - 3 * mm)
        c.setLineWidth(0.3)
        c.line(px0, yy - 3 * mm, px0, yy - 1.3 * mm)
        c.line(px1, yy - 3 * mm, px1, yy - 1.3 * mm)
        text(c, px0, yy - 1 * mm, "THR %s 0 ft" % rw["num"], 4.6)
        text(c, px1, yy - 1 * mm, "THR %s 0 ft" % rw["rnum"], 4.6, align="r")
        text(c, (px0 + px1) / 2, yy - 5.3 * mm, "%d m · pendiente 0 %%" % rw["len"], 4.6, align="c")
        yy -= 7.5 * mm
    y = min(yt, yy + 1 * mm)
    y = section(c, y, "AD 2.14  LUCES / APPROACH AND RUNWAY LIGHTING")
    rows = [("RWY", "APCH", "THR", "PAPI", "TDZ / RCLL", "REDL", "RENL")]
    for D in P["dirs"]:
        rows.append((D["num"], "900 m + barra 300 m" if D["ils"] else "NIL", "Verdes / green", "NIL", "NIL",
                     "Blancas cada 60 m", "Rojas / red"))
    y = table(c, y, (0, 9, 35, 56, 66, 82, 106), rows, 4.8, lh=3.0)
    y = section(c, y, "AD 2.18 / 2.19  COMUNICACIONES Y RADIOAYUDAS / ATS COMMUNICATIONS AND RADIO NAVIGATION AIDS")
    rows = [("TIPO", "ID", "FREQ", "COORDENADAS (aprox.)", "OBSERVACIONES")]
    rows.append(("ATIS", "-", "%.2f MHz" % L["atis"], "-", "Información en bucle (en inglés)"))
    rows.append(("TWR", "-", "%.2f MHz" % L["twr"], "-", "Torre y aproximación; tecla K para llamar"))
    vla, vlo = lat_lon(P["vpos"][0], P["vpos"][1], canary)
    rows.append(("VOR/DME", P["vid"], "%.2f MHz" % P["vfreq"], "%s %s" % (vla, vlo), "Alcance 200 km del simulador"))
    for D in P["dirs"]:
        if D["ils"]:
            ident = ("I" + L["icao"][-3:])[:4]
            a, b = lat_lon(D["loc"][0], D["loc"][1], canary)
            rows.append(("ILS/LOC %s" % D["num"], ident, "%.2f MHz" % D["ils"], "%s %s" % (a, b), "CRS %03d° · DME ILS" % D["hdg"]))
            a, b = lat_lon(D["gs"][0], D["gs"][1], canary)
            rows.append(("GP %s" % D["num"], ident, "3.0°", "%s %s" % (a, b), "RDH 52 ft · 300 m tras el umbral"))
            for key, lab, dm in (("om", "OM", 7200), ("mm", "MM", 1050)):
                a, b = lat_lon(D[key][0], D[key][1], canary)
                rows.append((lab + " " + D["num"], "-", "75 MHz", "%s %s" % (a, b), "%.1f NM al umbral" % (dm / NM)))
            p = sub(D["thr"], mul(D["d"], 300))
            a, b = lat_lon(p[0], p[1], canary)
            rows.append(("IM " + D["num"], "-", "75 MHz", "%s %s" % (a, b), "300 m al umbral"))
    y = table(c, y, (0, 17, 25, 39, 79), rows, 4.6, lh=2.9)
    y = section(c, y, "AD 2.10 / 2.22  OBSTÁCULOS Y PUNTOS DE LOS PROCEDIMIENTOS / OBSTACLES AND PROCEDURE FIXES")
    obs = []
    for x, yy_, r, h in C.CONES:
        dd_ = dist(P["vpos"], (x, yy_))
        if dd_ < 25 * NM:
            obs.append((h, brg(P["vpos"], (x, yy_)), dd_ / NM))
    obs.sort(reverse=True)
    rows = [("OBSTÁCULO (cumbre)", "ELEV", "%s RDL/DME" % P["vid"], "", "FIJO", "USO", "%s RDL/DME" % P["vid"])]
    fixes = [(f, "SID (salida)", b, 30.0) for f, cd, b, pp in P["exits"]] + [(f, "IAF", b, 16.0) for f, cd, b, pp in P["iafs"]]
    for i in range(max(len(fixes), min(4, len(obs)))):
        o = obs[i] if i < len(obs) else None
        fx_ = fixes[i] if i < len(fixes) else None
        rows.append(("Cumbre %d" % (i + 1) if o else "", ("%d ft" % round(o[0] * 3.28084, -1)) if o else "",
                     ("R-%03d / D%.1f" % (o[1], o[2])) if o else "", "",
                     fx_[0] if fx_ else "", fx_[1] if fx_ else "", ("R-%03d / D%.1f" % (fx_[2], fx_[3])) if fx_ else ""))
    if not obs:
        rows.append(("Sin relieve en 25 NM", "", "", "", "", "", ""))
    y = table(c, y, (0, 24, 36, 58, 62, 78, 98), rows, 4.7, lh=2.9)
    y = section(c, y, "AD 2.20 / 2.24  NORMAS LOCALES Y CARTAS / LOCAL REGULATIONS AND CHARTS")
    notes = ["Pedir autorización a TWR (tecla K) antes de despegar y al entrar en el CTR (12 NM, SFC-3000 ft).",
             "Esperas: 205 KT máx., virajes a derechas, tramos de 1 MIN. Velocidad máx. 250 KT bajo FL100.",
             "Cartas: %s." % index]
    for s in notes:
        text(c, M + 2 * mm, y, s, 4.9)
        y -= 3.2 * mm
    footer(c, code)
    c.showPage()


# ================================================================ colocacion de etiquetas sin solapes
class Placer:
    def __init__(self, v):
        self.v = v
        self.rects = []

    def block(self, x0, y0, x1, y1):
        self.rects.append((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)))

    def size(self, c, lines, size):
        w = max(c.stringWidth(s, "Helvetica-Bold" if i == 0 else "Helvetica", size) for i, s in enumerate(lines)) + 3
        h = len(lines) * (size + 1) + 2
        return w, h

    def free(self, r):
        v = self.v
        if r[0] < v.px + 1 or r[2] > v.px + v.pw - 1 or r[1] < v.py + 1 or r[3] > v.py + v.ph - 1:
            return False
        for q in self.rects:
            if not (r[2] < q[0] or r[0] > q[2] or r[3] < q[1] or r[1] > q[3]):
                return False
        return True

    def put(self, c, x, y, lines, size=4.5, color=black, gap=3):
        """caja junto al punto (x,y) en la primera posicion libre; linea guia si se aleja"""
        w, h = self.size(c, lines, size)
        cands = []
        for dist_ in (gap, gap + 6, gap + 12, gap + 20):
            cands += [(x + dist_, y + 1), (x - dist_ - w, y + 1), (x + dist_, y - h - 1), (x - dist_ - w, y - h - 1),
                      (x - w / 2, y + dist_), (x - w / 2, y - dist_ - h)]
        best = None
        for bx, by in cands:
            r = (bx, by, bx + w, by + h)
            if self.free(r):
                best = (bx, by)
                break
        if best is None:
            bx, by = cands[0]
            bx = min(max(bx, self.v.px + 1), self.v.px + self.v.pw - w - 1)
            by = min(max(by, self.v.py + 1), self.v.py + self.v.ph - h - 1)
            best = (bx, by)
        bx, by = best
        if math.hypot(bx + w / 2 - x, by + h / 2 - y) > w / 2 + h / 2 + 4:
            c.setStrokeColor(HexColor("#888888"))
            c.setLineWidth(0.3)
            c.line(x, y, min(max(x, bx), bx + w), min(max(y, by), by + h))
        tbox(c, bx, by, lines, size, color)
        self.block(bx, by, bx + w, by + h)
        return best


# ================================================================ dibujo comun de planta
def plan_view(c, pts, y0, y1, margin=3 * NM):
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    v = View(c, M + 2 * mm, y0, W - 2 * M - 4 * mm, y1 - y0, min(xs) - margin, min(ys) - margin,
             max(xs) + margin, max(ys) + margin)
    c.saveState()
    v.clip()
    c.setFillColor(white)
    c.rect(v.px, v.py, v.pw, v.ph, fill=1, stroke=0)
    vb = box(v.x0, v.y0, v.x1, v.y1)
    v.geom(vb.difference(C.LAND), fill=HexColor("#e6f1f8"), stroke=HexColor("#9fbfd8"), width=0.4)
    for x, y, r, h in sorted(C.CONES, key=lambda t: t[3]):
        if v.inside(x, y, r):
            c.setFillColor(Color(0.62, 0.45, 0.28, alpha=0.10 + 0.12 * min(1, h / 3000)))
            c.circle(*v.pt(x, y), r * v.k, stroke=0, fill=1)
            if v.inside(x, y) and h > 600:
                px, py = v.pt(x, y)
                text(c, px, py - 1, "▲" if False else "%d" % round(h * 3.28084, -1), 4.2, color=HexColor("#7a5530"), align="c")
    for n, x, y, w, d in C.CITIES:
        if v.inside(x, y, 4000):
            v.poly([(x - w / 2, y - d / 2), (x + w / 2, y - d / 2), (x + w / 2, y + d / 2), (x - w / 2, y + d / 2)],
                   fill=HexColor("#f7e7b0"), stroke=HexColor("#c9a94a"), width=0.3)
            px, py = v.pt(x, y + d / 2)
            text(c, px, py + 1.2, n, 4.3, "Helvetica-Oblique", HexColor("#8a6d1a"), "c")
    return v


def end_plan(c, v):
    c.restoreState()
    c.setStrokeColor(black)
    c.setLineWidth(0.6)
    c.rect(v.px, v.py, v.pw, v.ph)
    C.north_arrow(c, v.px + 5 * mm, v.py + v.ph - 11 * mm, 7, "N")
    # escala
    L = 5 * NM * v.k
    x, y = v.px + 3 * mm, v.py + 3 * mm
    c.setLineWidth(0.5)
    c.line(x, y, x + L, y)
    for i in range(6):
        c.line(x + i * L / 5, y, x + i * L / 5, y + 1.2)
    text(c, x + L + 2, y - 0.5, "5 NM", 4.6)


def draw_path(c, v, path, color=PROC, width=0.85, dash=None, arrows=(0.35, 0.72)):
    c.saveState()
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(width)
    if dash:
        c.setDash(*dash)
    p = c.beginPath()
    p.moveTo(*v.pt(*path[0]))
    for q in path[1:]:
        p.lineTo(*v.pt(*q))
    c.drawPath(p, stroke=1, fill=0)
    c.setDash()
    total = plen(path)
    for t in arrows:
        q, u = point_at(path, total * t)
        x, y = v.pt(*q)
        a = math.atan2(u[1], u[0])
        pp = c.beginPath()
        pp.moveTo(x + 2.6 * math.cos(a), y + 2.6 * math.sin(a))
        pp.lineTo(x - 1.6 * math.cos(a) + 1.4 * math.sin(a), y - 1.6 * math.sin(a) - 1.4 * math.cos(a))
        pp.lineTo(x - 1.6 * math.cos(a) - 1.4 * math.sin(a), y - 1.6 * math.sin(a) + 1.4 * math.cos(a))
        pp.close()
        c.drawPath(pp, fill=1, stroke=0)
    c.restoreState()


def label_on(c, v, path, t, s, size=4.6, off=2.2, color=PROC, bold=True):
    q, u = point_at(path, plen(path) * t)
    x, y = v.pt(*q)
    ang = math.degrees(math.atan2(u[1], u[0]))
    if ang > 90:
        ang -= 180
    if ang < -90:
        ang += 180
    c.saveState()
    c.translate(x, y)
    c.rotate(ang)
    w = c.stringWidth(s, "Helvetica-Bold" if bold else "Helvetica", size)
    c.setFillColor(white)
    c.rect(-w / 2 - 0.6, off - 0.8, w + 1.2, size + 0.8, fill=1, stroke=0)
    text(c, 0, off, s, size, "Helvetica-Bold" if bold else "Helvetica", color, "c")
    c.restoreState()


def fix_symbol(c, x, y, kind="tri", color=black, s=2.0):
    c.saveState()
    c.setFillColor(color)
    c.setStrokeColor(color)
    c.setLineWidth(0.6)
    if kind == "tri":
        p = c.beginPath()
        p.moveTo(x, y + s * 1.1)
        p.lineTo(x - s, y - s * 0.65)
        p.lineTo(x + s, y - s * 0.65)
        p.close()
        c.drawPath(p, fill=1, stroke=0)
    elif kind == "otri":
        p = c.beginPath()
        p.moveTo(x, y + s * 1.1)
        p.lineTo(x - s, y - s * 0.65)
        p.lineTo(x + s, y - s * 0.65)
        p.close()
        c.drawPath(p, fill=0, stroke=1)
    elif kind == "cross":                       # cruz de Malta: FAF/FAP
        for a in range(4):
            r = math.radians(90 * a)
            p = c.beginPath()
            p.moveTo(x, y)
            for da in (-0.45, 0.45):
                p.lineTo(x + math.cos(r + da) * s * 1.4, y + math.sin(r + da) * s * 1.4)
            p.close()
            c.drawPath(p, fill=1, stroke=0)
    c.restoreState()


def runway(c, v, rws):
    for rw in rws:
        c.setStrokeColor(black)
        c.setLineWidth(2.4)
        c.line(*v.pt(*rw["thr"]), *v.pt(*rw["end"]))


def vor_draw(c, v, P, pl, rose=True):
    px, py = v.pt(*P["vpos"])
    if rose:
        C.compass_rose(c, px, py, 9, HexColor("#555555"))
    C.vor_symbol(c, px, py, 1.9, black)
    pl.put(c, px, py, ["%s VOR/DME" % C.VOR_NAMES.get(P["vid"], P["vid"]), "%s %.2f" % (P["vid"], P["vfreq"])], 4.6)


def msa_diagram(c, x, y, r, P):
    """diagrama MSA de 4 sectores centrado en el VOR"""
    c.saveState()
    c.setFillColor(white)
    c.setStrokeColor(black)
    c.setLineWidth(0.5)
    c.circle(x, y, r, fill=1, stroke=1)
    vals = P["msa"]
    for k in range(4):
        a = math.radians(90 * k)
        if vals[k] != vals[k - 1]:
            c.line(x, y, x + math.sin(a) * r, y + math.cos(a) * r)
    for k in range(4):
        a = math.radians(90 * k + 45)
        text(c, x + math.sin(a) * r * 0.55, y + math.cos(a) * r * 0.55 - 2, "%d" % vals[k], 5.4, "Helvetica-Bold",
             black, "c")
    C.vor_symbol(c, x, y, 1.2, black, dme=False)
    text(c, x, y - r - 4, "MSA 25 NM %s" % P["vid"], 4.8, "Helvetica-Bold", black, "c")
    c.restoreState()


def hold_draw(c, v, hold, pl):
    shape = hold["shape"]
    draw_path(c, v, shape, PROC, 0.7, arrows=(0.2, 0.7))
    cx = sum(q[0] for q in shape) / len(shape)
    cy = sum(q[1] for q in shape) / len(shape)
    px, py = v.pt(cx, cy)
    pl.put(c, px, py, ["ESPERA / HOLD %s" % hold["fix"], "%03d° IN · 1 MIN · DCHA" % round(hold["course"]),
                       "MHA %d · 205 KT MAX" % hold["alt"]], 4.3, PROC, gap=8)


# ================================================================ SID
def sid_chart(c, L, P, D, code):
    sids = P["sids"][D["num"]]
    ytop = header(c, L, ("CARTA DE SALIDA NORMALIZADA", "VUELO POR INSTRUMENTOS (SID) - OACI"), code,
                  "SID RWY %s" % D["num"], base_freqs(L, P))
    pts = [D["thr"], D["end"]]
    for s in sids:
        pts += s["path"]
    v = plan_view(c, pts, M + 52 * mm, ytop - 1.5 * mm, margin=4 * NM)
    pl = Placer(v)
    msa_r = 9 * mm
    mx, my = v.px + v.pw - msa_r - 3 * mm, v.py + v.ph - msa_r - 4 * mm
    pl.block(mx - msa_r - 1, my - msa_r - 6, mx + msa_r + 1, my + msa_r + 1)
    pl.block(v.px, v.py + v.ph - 14 * mm, v.px + 10 * mm, v.py + v.ph)
    pl.block(v.px, v.py, v.px + 30 * mm, v.py + 5 * mm)
    runway(c, v, L["runways"])
    for s in sids:
        draw_path(c, v, s["path"], PROC, 0.9, arrows=(0.18, 0.5, 0.82))
        label_on(c, v, s["path"], 0.62, s["name"], 4.8)
    vor_draw(c, v, P, pl, rose=True)
    q = add(D["end"], mul(D["d"], 1.2 * NM))
    pl.put(c, *v.pt(*q), ["%03d°" % D["hdg"], "1500"], 4.6)
    for s in sids:
        ex, ey = v.pt(*s["E"])
        fix_symbol(c, ex, ey, "tri")
        pl.put(c, ex, ey, [s["fix"], "D%.1f %s R-%03d" % (s["dme"], P["vid"], s["radial"]), "A%d+" % s["exit_alt"]], 4.6)
    msa_diagram(c, mx, my, msa_r, P)
    end_plan(c, v)
    # texto de los procedimientos
    y = M + 49 * mm
    grad = max(s["grad"] for s in sids)
    text(c, M + 2 * mm, y, "PENDIENTE MÍNIMA DE ASCENSO %.1f %% (%d ft/NM) hasta la altitud mínima del fijo de salida." %
         (grad / 60.76, grad), 5.2, "Helvetica-Bold")
    text(c, M + 2 * mm, y - 3.2 * mm, "Ascenso inicial: 6000 ft o según autorización de TWR. Velocidad máx. 250 KT bajo FL100.", 5)
    y -= 6.5 * mm
    for s in sids:
        c.setFillColor(GREY)
        c.rect(M + 1 * mm, y - 6.4 * mm, W - 2 * M - 2 * mm, 7.4 * mm, fill=1, stroke=0)
        text(c, M + 2 * mm, y - 1.6 * mm, s["name"], 6, "Helvetica-Bold")
        text(c, M + 2 * mm, y - 4.6 * mm, "(%s) %d NM" % (s["card"], round(s["length"])), 4.8)
        turn = "sin viraje" if s["turn"] == "recto" else "viraje progresivo a la %s" % s["turn"]
        l1 = "Rumbo de pista %03d° hasta 1500 ft; %s para interceptar el R-%03d %s" % (D["hdg"], turn, s["radial"], P["vid"])
        l2 = "hasta %s (D%.1f %s). Cruzar %s a %d ft o superior." % (s["fix"], s["dme"], P["vid"], s["fix"], s["exit_alt"])
        text(c, M + 21 * mm, y - 1.6 * mm, l1, 4.8)
        text(c, M + 21 * mm, y - 4.2 * mm, l2, 4.8)
        y -= 8.2 * mm
    footer(c, code)
    c.showPage()


# ================================================================ IAC
def iac_chart(c, L, P, A, code):
    D = A["D"]
    ils = A["kind"] == "ILS"
    ident = ("I" + L["icao"][-3:])[:4]
    extra = ["ILS %s %.2f" % (ident, D["ils"])] if ils else []
    ytop = header(c, L, ("CARTA DE APROXIMACIÓN", "POR INSTRUMENTOS - OACI"), code, A["name"],
                  base_freqs(L, P, extra))
    pts = [D["thr"], D["end"], A["IF"]]
    for a in A["arrivals"]:
        pts += a["path"]
    pts += A["hold"]["shape"] + A["missed"]["path"]
    v = plan_view(c, pts, M + 90 * mm, ytop - 1.5 * mm, margin=2.5 * NM)
    pl = Placer(v)
    msa_r = 8 * mm
    mx, my = v.px + v.pw - msa_r - 3 * mm, v.py + v.ph - msa_r - 4 * mm
    pl.block(mx - msa_r - 1, my - msa_r - 6, mx + msa_r + 1, my + msa_r + 1)
    pl.block(v.px, v.py + v.ph - 14 * mm, v.px + 10 * mm, v.py + v.ph)
    pl.block(v.px, v.py, v.px + 30 * mm, v.py + 5 * mm)
    runway(c, v, L["runways"])
    # llegadas
    for a in A["arrivals"]:
        draw_path(c, v, a["path"], PROC, 0.8, arrows=(0.3, 0.7))
        label_on(c, v, a["path"], 0.45, "%d NM  MNM %d" % (round(a["length"]), a["mnm"]), 4.4)
    # espera, frustrada
    draw_path(c, v, A["missed"]["path"], MA_COL, 0.7, dash=(2.5, 1.5), arrows=(0.4, 0.8))
    # final
    c.setStrokeColor(black)
    c.setLineWidth(1.5)
    c.line(*v.pt(*A["IF"]), *v.pt(*D["thr"]))
    ix, iy = v.pt(*A["IF"])
    fix_symbol(c, ix, iy, "otri", s=2.2)
    dme_if = dist(A["dme_src"], A["IF"]) / NM
    dme_faf = dist(A["dme_src"], A["FAF"]) / NM
    src = ident if ils else P["vid"]
    fx, fy = v.pt(*A["FAF"])
    fix_symbol(c, fx, fy, "cross", s=1.8)
    # reservar la linea de la final para que no la tapen las etiquetas
    tx, ty = v.pt(*D["thr"])
    for k in range(11):
        qx, qy = ix + (tx - ix) * k / 10, iy + (ty - iy) * k / 10
        pl.block(qx - 1.5, qy - 1.5, qx + 1.5, qy + 1.5)
    vor_draw(c, v, P, pl, rose=False)
    pl.put(c, ix, iy, ["IF", "D%.1f %s" % (dme_if, src), "%d" % A["if_alt"]], 4.4)
    pl.put(c, fx, fy, ["FAP" if ils else "FAF", "D%.1f %s" % (dme_faf, src), "%d" % A["faf_alt"]], 4.4)
    for a in A["arrivals"]:
        ax, ay = v.pt(*a["A"])
        fix_symbol(c, ax, ay, "tri")
        pl.put(c, ax, ay, ["%s IAF" % a["fix"], "D16.0 %s R-%03d" % (P["vid"], a["radial"]), "%d" % a["mnm"]], 4.4)
    hold_draw(c, v, A["hold"], pl)
    mid = mul(add(A["IF"], D["thr"]), 0.5)
    label_on(c, v, [A["IF"], D["thr"]], 0.25, "%03d°" % D["hdg"], 5.2, off=2.6)
    if ils:
        # pluma del localizador
        e = sub(A["IF"], mul(D["d"], 1.5 * NM))
        pa, pb = v.pt(*A["IF"]), v.pt(*e)
        ang = math.atan2(pb[1] - pa[1], pb[0] - pa[0])
        c.setLineWidth(0.4)
        for sgn in (-1, 1):
            c.line(pa[0] - math.sin(ang) * 1.6 * sgn, pa[1] + math.cos(ang) * 1.6 * sgn,
                   pb[0] - math.sin(ang) * 1.6 * sgn, pb[1] + math.cos(ang) * 1.6 * sgn)
        c.line(pb[0] - math.sin(ang) * 1.6, pb[1] + math.cos(ang) * 1.6, pb[0] + math.sin(ang) * 1.6, pb[1] - math.cos(ang) * 1.6)
    msa_diagram(c, mx, my, msa_r, P)
    end_plan(c, v)
    profile(c, A, ils, ident, P)
    minima(c, A, ils)
    footer(c, code)
    c.showPage()


def profile(c, A, ils, ident, P):
    D = A["D"]
    y0, y1 = M + 47 * mm, M + 87 * mm
    x0, x1 = M + 4 * mm, W - M - 4 * mm
    c.setStrokeColor(black)
    c.setLineWidth(0.5)
    c.rect(M + 1 * mm, M + 37.5 * mm, W - 2 * M - 2 * mm, y1 - M - 37.5 * mm, stroke=1, fill=0)
    gy = y0 + 2 * mm
    maxd = 11.0
    X = lambda dn: x1 - 14 * mm - dn / maxd * (x1 - x0 - 18 * mm)
    Y = lambda ftv: gy + ftv / (A["if_alt"] + 600) * (y1 - gy - 4 * mm)
    c.setLineWidth(0.8)
    c.line(x0, gy, x1, gy)
    c.setFillColor(HexColor("#d9d9d9"))
    c.rect(X(0), gy - 1.2, X(-D["len"] / NM) - X(0), 1.2, fill=1, stroke=0)
    gpf = lambda dn: (dn * NM + 300) * math.tan(math.radians(3)) * 3.28084
    # nivel de llegada hasta el IF y senda
    c.setStrokeColor(PROC)
    c.setLineWidth(1.0)
    c.line(x0, Y(A["if_alt"]), X(10), Y(A["if_alt"]))
    c.line(X(10), Y(gpf(10)), X(0), Y(gpf(0)))
    text(c, (X(10) + X(5)) / 2 + 3 * mm, Y(gpf(7.5)) + 3, ("GP 3.0°  RDH 52 ft" if ils else "Descenso 3.0° (5.2 %)"),
         5, "Helvetica-Bold", PROC, "c")
    fixes = [(10, "IF", A["if_alt"]), (5, "FAP" if ils else "FAF", A["faf_alt"])]
    if ils:
        fixes += [(7200 / NM, "OM", gpf(7200 / NM)), (1050 / NM, "MM", gpf(1050 / NM))]
    else:
        fixes += [(dn, "", gpf(dn)) for dn in (4, 3, 2)]
    for dn, lab, alt in fixes:
        c.setStrokeColor(HexColor("#666666"))
        c.setLineWidth(0.3)
        c.setDash(1, 1)
        c.line(X(dn), gy, X(dn), Y(alt))
        c.setDash()
        text(c, X(dn), Y(alt) + 2.5, "%d" % round(alt, -1), 5, "Helvetica-Bold", black, "c")
        text(c, X(dn), Y(alt) + 7.5, "(%d)" % round(alt, -1), 4.2, color=black, align="c")
        if lab:
            text(c, X(dn), gy - 3.2 * mm, lab, 4.8, "Helvetica-Bold", black, "c")
        dme = dist(A["dme_src"], sub(D["thr"], mul(D["d"], dn * NM))) / NM
        text(c, X(dn), gy - 6.2 * mm, "D%.1f" % dme, 4.4, color=black, align="c")
        if lab in ("FAP", "FAF"):
            fix_symbol(c, X(dn), Y(alt), "cross", s=1.6)
    text(c, x0, gy - 6.2 * mm, "DME %s" % (ident if ils else P["vid"]), 4.4, "Helvetica-Bold")
    text(c, X(0), gy - 3.2 * mm, "THR", 4.8, "Helvetica-Bold", black, "c")
    # MDA / DA
    och = A["och"][0]
    c.setStrokeColor(HexColor("#a00000"))
    c.setLineWidth(0.5)
    c.setDash(2, 1)
    c.line(X(3), Y(och), X(-0.3), Y(och))
    c.setDash()
    text(c, X(3) - 1, Y(och) - 1.5, ("DA" if ils else "MDA") + " %d" % och, 4.6, "Helvetica-Bold", HexColor("#a00000"), "r")
    # frustrada
    c.setStrokeColor(MA_COL)
    c.setLineWidth(0.7)
    c.setDash(2, 1.2)
    xe = x1 - 3 * mm
    c.line(X(0), Y(och), xe, Y(och) + (xe - X(0)) * 0.6)
    c.setDash()
    c.setFillColor(MA_COL)
    text(c, xe - 1, Y(och) + (xe - X(0)) * 0.6 + 2, "MA 2000", 4.8, "Helvetica-Bold", MA_COL, "r")
    text(c, X(0) + 2, Y(och) + 5, "MAPt" if not ils else "", 4.4, "Helvetica-Bold")
    # distancias al umbral
    for a_, b_ in ((10, 5), (5, 0)):
        yy = gy - 8.8 * mm
        c.setStrokeColor(black)
        c.setLineWidth(0.3)
        c.line(X(a_), yy + 1.2, X(b_), yy + 1.2)
        text(c, (X(a_) + X(b_)) / 2, yy + 2, "%.1f NM" % (a_ - b_), 4.6, "Helvetica-Bold", black, "c")


def minima(c, A, ils):
    D = A["D"]
    y = M + 33.5 * mm
    # tabla de minimos
    x0 = M + 2 * mm
    c.setFillColor(GREY)
    c.rect(M + 1 * mm, y - 1.3 * mm, 74 * mm, 4 * mm, fill=1, stroke=0)
    text(c, x0, y, "OCA (OCH)", 5.2, "Helvetica-Bold")
    for i, cat in enumerate("ABCD"):
        text(c, x0 + (24 + i * 12.5) * mm, y, cat, 5.2, "Helvetica-Bold", align="c")
    rows = [("ILS CAT I" if ils else "VOR/DME", A["och"])]
    if ils:
        rows.append(("LOC (GP INOP)", A["loc_och"]))
    rows.append(("CIRCUITO / CIRCLING", None))
    for name, vals in rows:
        y -= 3.6 * mm
        text(c, x0, y, name, 5)
        if vals:
            for i, v_ in enumerate(vals):
                text(c, x0 + (24 + i * 12.5) * mm, y, "%d (%d)" % (v_, v_), 5, "Helvetica-Bold", align="c")
        else:
            text(c, x0 + 43 * mm, y, "NO AUTORIZADO / NOT AUTH", 5, "Helvetica-Bold", align="c")
    y -= 3.4 * mm
    text(c, x0, y, "Elevación del umbral 0 ft: OCA = OCH. RVR/VIS según el operador.", 4.4, "Helvetica-Oblique")
    # tabla de regimen de descenso
    tx = M + 78 * mm
    y = M + 33.5 * mm
    c.setFillColor(GREY)
    c.rect(tx - 1 * mm, y - 1.3 * mm, W - M - tx, 4 * mm, fill=1, stroke=0)
    text(c, tx, y, "GS (KT)", 4.8, "Helvetica-Bold")
    gss = (80, 100, 120, 140, 160, 180)
    for i, g in enumerate(gss):
        text(c, tx + (16 + i * 7.2) * mm, y, "%d" % g, 4.8, "Helvetica-Bold", align="c")
    y -= 3.4 * mm
    text(c, tx, y, ("FAP" if ils else "FAF") + "-THR 5.0 NM", 4.4)
    for i, g in enumerate(gss):
        t = 5.0 / g * 3600
        text(c, tx + (16 + i * 7.2) * mm, y, "%d:%02d" % (t // 60, t % 60), 4.6, align="c")
    y -= 3.4 * mm
    text(c, tx, y, "ROD 5.2 % (ft/min)", 4.4)
    for i, g in enumerate(gss):
        text(c, tx + (16 + i * 7.2) * mm, y, "%d" % round(g * 318.4 / 60), 4.6, align="c")
    # textos
    Ld = A["missed"]
    hold = A["hold"]
    turn = "en línea recta" if Ld["turn"] == "recto" else "viraje progresivo a la %s" % Ld["turn"]
    eng = {"derecha": "right", "izquierda": "left", "recto": "straight"}[Ld["turn"]]
    y = M + 15.6 * mm
    lines = ["FRUSTRADA: ascender en rumbo de pista %03d° hasta 2000 ft; %s directo a %s ascendiendo a %d ft y esperar." % (
                 D["hdg"], turn, hold["fix"], hold["alt"]),
             "MISSED APCH: climb on RWY HDG %03d° to 2000 ft, continuous %s turn direct %s climbing to %d ft and hold." % (
                 D["hdg"], eng, hold["fix"], hold["alt"]),
             "Pendiente mínima de frustrada %.1f %%. %s y DME requeridos. Llegadas desde los IAF: radar o propia navegación." % (
                 Ld["grad"] / 60.76, "ILS, VOR" if ils else "VOR"),
             "Distancias en NM del simulador. Altitudes en ft (alturas entre paréntesis)."]
    for i, s in enumerate(lines):
        text(c, M + 2 * mm, y - i * 3 * mm, s, 4.7, "Helvetica-Bold" if i == 0 else "Helvetica")


# ================================================================ conjunto
def build(fn):
    from reportlab.pdfgen import canvas
    des = Designer(C.CONES, C.CITIES, C.APTS, C.NAVS)
    cv = canvas.Canvas(fn, pagesize=A5)
    cv.setTitle("QFS - Cartas de aeropuerto: ADC, datos AD 2, SID e IAC")
    cv.setAuthor("QFS")
    total = 0
    for L in C.APTS:
        P = des.design(L)
        icao = L["icao"]
        sid_codes = ["AD 2-%s SID %s" % (icao, D["num"]) for D in P["dirs"]]
        iac_codes = ["AD 2-%s IAC %d" % (icao, i + 1) for i in range(len(P["iacs"]))]
        index = "ADC, " + ", ".join("SID RWY %s" % D["num"] for D in P["dirs"]) + ", " + ", ".join(a["name"] for a in P["iacs"])
        C.aerodrome(cv, L)
        adc_data(cv, L, P, "AD 2-%s ADC 2" % icao, index)
        for D, code in zip(P["dirs"], sid_codes):
            sid_chart(cv, L, P, D, code)
        for A, code in zip(P["iacs"], iac_codes):
            iac_chart(cv, L, P, A, code)
        total += 2 + len(P["dirs"]) + len(P["iacs"])
        print("  %s: %d SID, %d IAC" % (icao, len(P["dirs"]), len(P["iacs"])))
    cv.save()
    return total
