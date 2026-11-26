#!/usr/bin/env python3
"""mkscen.py - compila un escenario de texto (.TXT) al binario .SCN del QL.

Uso: mkscen.py entrada.txt salida.scn

Formato (una orden por linea, ';' comenta; coordenadas en metros, x = este,
y = norte, rumbos en grados):

  NOMBRE "texto"                 TESELA log2 (14 = 16384 m)     ELEV metros
  AEROPUERTO id "nombre" x y elev ATIS f TWR f "texto ATIS"
  PISTA id num xu yu rumbo largo ancho [ILS f] [LUCES]
        (xu,yu = umbral de la cabecera 'num'; se genera tambien la opuesta)
  VOR id f x y [DME]             NDB id khz x y
  EDIFICIO x y ancho fondo alto rot [relleno]      TORRE x y     HANGAR x y rot
  CIUDAD x y ancho fondo n semilla
  CARRETERA x1 y1 x2 y2 ...      RIO x1 y1 x2 y2 ...
  LAGO x y radio [lados]         CAMPO relleno x1 y1 x2 y2 x3 y3 ...
  MONTANA x y radio alto [lados]
"""
import math, random, shlex, struct, sys

# rellenos (deben coincidir con gfx.h)
FILLS = dict(NEGRO=0, ROJO=1, VERDE=2, BLANCO=3, GRIS=4, AGUA=5, VERDEOSC=6, NARANJA=7,
             MARRON=8, ROJOOSC=9, GRISCLARO=10, BRUMA=11, GRISOSC=12, CAMPO=14)
C_BLACK, C_RED, C_GREEN, C_WHITE = 0, 1, 2, 3
FA_OUTLINE, FA_NOFILL = 0x40, 0x80
OBJ_GROUND, OBJ_LINES, OBJ_SOLID, OBJ_LIGHTS = 0, 1, 2, 3
OF_HIT, OF_WATER, OF_NIGHT, OF_DAYLIGHT, OF_MOUNTAIN, OF_SORT = 1, 2, 4, 8, 16, 32
NAV_VOR, NAV_VORDME, NAV_NDB, NAV_ILS, NAV_OM, NAV_MM, NAV_IM, NAV_GS = 1, 2, 3, 4, 5, 6, 7, 8


def attr(fill=None, outline=None):
    a = FA_NOFILL if fill is None else fill
    if outline is not None:
        a |= FA_OUTLINE | (outline << 4)
    return a


class Obj:
    def __init__(s, typ, ox, oy, oz=0, flags=0, lod=60):
        s.typ, s.ox, s.oy, s.oz, s.flags, s.lod = typ, ox, oy, oz, flags, lod
        s.v, s.f = [], []

    def vert(s, x, y, z=0.0):
        """x,y absolutos (m); z relativo al origen"""
        s.v.append((x - s.ox, y - s.oy, z))
        return len(s.v) - 1

    def face(s, a, idx):
        s.f.append((a, list(idx)))

    def pack(s):
        assert len(s.v) <= 96, "objeto con demasiados vertices"
        m = max([max(abs(a), abs(b), abs(c)) for a, b, c in s.v] + [1]) * 8
        vsh = 0
        while (m / (1 << vsh)) > 32000:
            vsh += 1
        r = int(max([math.hypot(a, b) for a, b, c in s.v] + [1]) + 1)
        fb = b"".join(bytes([len(i), a] + i) for a, i in s.f)
        h = struct.pack(">BBBBiihHBBH", s.typ, s.flags, min(255, s.lod), vsh,
                        int(round(s.ox)), int(round(s.oy)), int(round(s.oz)), min(65535, r),
                        len(s.v), len(s.f), len(fb))
        vb = b"".join(struct.pack(">hhh", *[int(round(c * 8 / (1 << vsh))) for c in v]) for v in s.v)
        if len(fb) & 1:
            fb += b"\0"
        return h + vb + fb


class Scen:
    def __init__(s):
        s.name, s.tlog, s.elev = "ESCENARIO", 14, 0
        s.objs, s.navs, s.apts = [], [], {}
        s.vis = 0
        s.apt_order = []

    # ---------------------------------------------------------------- objetos
    def box(s, cx, cy, w, d, h, rot, wall, roof, z0=0, hit=True, obj=None):
        o = obj or Obj(OBJ_SOLID, cx, cy, s.elev + z0, OF_HIT if hit else 0, lod=30)
        ca, sa = math.cos(math.radians(rot)), math.sin(math.radians(rot))
        pts = []
        for dx, dy in ((-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2)):
            pts.append((cx + dx * ca + dy * sa, cy - dx * sa + dy * ca))
        b = [o.vert(x, y, 0) for x, y in pts]
        t = [o.vert(x, y, h) for x, y in pts]
        for i in range(4):  # paredes (antihorario visto desde fuera)
            j = (i + 1) % 4
            o.face(attr(wall, C_BLACK), [b[i], b[j], t[j], t[i]])
        o.face(attr(roof, C_BLACK), [t[0], t[1], t[2], t[3]])
        if obj is None:
            s.objs.append(o)
        return o

    def lights(s, pts, color, lod=80, flags=0):
        """pts: lista (x,y,z); divide en objetos de <= 90 luces"""
        for k in range(0, len(pts), 90):
            chunk = pts[k:k + 90]
            cx = sum(p[0] for p in chunk) / len(chunk)
            cy = sum(p[1] for p in chunk) / len(chunk)
            o = Obj(OBJ_LIGHTS, cx, cy, s.elev, flags, lod)
            idx = [o.vert(x, y, z) for x, y, z in chunk]
            o.face(color, idx)
            s.objs.append(o)

    def runway(s, apt, num, x, y, hdg, ln, wd, ils=None, luces=False):
        a = s.apts[apt]
        el = a["elev"]
        hx, hy = math.sin(math.radians(hdg)), math.cos(math.radians(hdg))
        px, py = hy, -hx            # perpendicular (derecha)
        cx, cy = x + hx * ln / 2, y + hy * ln / 2
        o = Obj(OBJ_GROUND, cx, cy, el, OF_NIGHT * 0, lod=70)
        c = [(x - px * wd / 2, y - py * wd / 2), (x + px * wd / 2, y + py * wd / 2),
             (x + px * wd / 2 + hx * ln, y + py * wd / 2 + hy * ln), (x - px * wd / 2 + hx * ln, y - py * wd / 2 + hy * ln)]
        o.face(attr(FILLS["NEGRO"]), [o.vert(px_, py_) for px_, py_ in c])
        s.objs.append(o)
        # marcas: eje discontinuo y barras de umbral (ambas cabeceras)
        m = Obj(OBJ_GROUND, cx, cy, el, 0, lod=25)
        L = 30.0
        t = 80.0
        while t < ln - 110:
            a0, a1 = t, t + L
            q = [(x + hx * a0 - px * 0.6, y + hy * a0 - py * 0.6), (x + hx * a0 + px * 0.6, y + hy * a0 + py * 0.6),
                 (x + hx * a1 + px * 0.6, y + hy * a1 + py * 0.6), (x + hx * a1 - px * 0.6, y + hy * a1 - py * 0.6)]
            m.face(attr(FILLS["BLANCO"]), [m.vert(*p) for p in q])
            t += 2 * L
            if len(m.v) > 80:
                break
        for end in (0, 1):
            for k in (-3, -2, -1, 1, 2, 3):
                off = k * wd / 8.0
                a0, a1 = (6, 36) if end == 0 else (ln - 36, ln - 6)
                q = [(x + hx * a0 + px * (off - 1), y + hy * a0 + py * (off - 1)),
                     (x + hx * a0 + px * (off + 1), y + hy * a0 + py * (off + 1)),
                     (x + hx * a1 + px * (off + 1), y + hy * a1 + py * (off + 1)),
                     (x + hx * a1 + px * (off - 1), y + hy * a1 + py * (off - 1))]
                if len(m.v) <= 92:
                    m.face(attr(FILLS["BLANCO"]), [m.vert(*p) for p in q])
        s.objs.append(m)
        # cabeceras para el ATC
        a["rwy"].append((hdg, ln, wd, x, y))
        a["rwy"].append(((hdg + 180) % 360, ln, wd, x + hx * ln, y + hy * ln))
        if luces or ils:
            pts = []
            n = int(ln // 60)
            for i in range(n + 1):
                d = i * ln / n
                for sgn in (-1, 1):
                    pts.append((x + hx * d + sgn * px * (wd / 2 + 2), y + hy * d + sgn * py * (wd / 2 + 2), 0.5))
            s.lights(pts, C_WHITE)
            thr = [(x + px * k * wd / 10, y + py * k * wd / 10, 0.5) for k in range(-5, 6)]
            s.lights(thr, C_GREEN)
            endl = [(x + hx * ln + px * k * wd / 10, y + hy * ln + py * k * wd / 10, 0.5) for k in range(-5, 6)]
            s.lights(endl, C_RED)
        if ils:
            # luces de aproximacion 900 m
            app = [(x - hx * d, y - hy * d, 1.0) for d in range(60, 901, 60)]
            app += [(x - hx * 300 + px * k * 4, y - hy * 300 + py * k * 4, 1.0) for k in (-4, -3, -2, 2, 3, 4)]
            s.lights(app, C_WHITE)
            f = int(round(float(ils) * 100))
            loc_x, loc_y = x + hx * (ln + 300), y + hy * (ln + 300)
            s.navs.append((NAV_ILS, f, loc_x, loc_y, el, hdg, 50, 3.0, "I" + apt[-3:]))
            for typ, dist in ((NAV_OM, 7200), (NAV_MM, 1050), (NAV_IM, 300)):
                s.navs.append((typ, 0, x - hx * dist, y - hy * dist, el, 0, 0, 0, "MK"))
            # antena de senda de planeo: 300 m despues del umbral
            s.navs.append((NAV_GS, f, x + hx * 300, y + hy * 300, el, hdg, 50, 3.0, "GS"))

    # ---------------------------------------------------------------- lector
    def parse(s, fname):
        for raw in open(fname, encoding="utf-8"):
            line = raw.split(";")[0].strip()
            if not line:
                continue
            t = shlex.split(line)
            cmd, a = t[0].upper(), t[1:]
            f = lambda i: float(a[i])
            if cmd == "NOMBRE":
                s.name = a[0]
            elif cmd == "TESELA":
                s.tlog = int(a[0])
            elif cmd == "ELEV":
                s.elev = int(a[0])
            elif cmd == "AEROPUERTO":
                kw = {a[i].upper(): a[i + 1] for i in range(5, len(a) - 1, 2) if a[i].upper() in ("ATIS", "TWR")}
                s.apts[a[0]] = dict(name=a[1], x=f(2), y=f(3), elev=int(f(4)),
                                    atis=int(round(float(kw.get("ATIS", "0")) * 100)),
                                    twr=int(round(float(kw.get("TWR", "0")) * 100)),
                                    text=a[-1] if len(a) > 9 else "", rwy=[])
                s.apt_order.append(a[0])
            elif cmd == "PISTA":
                ils = None
                if "ILS" in [x.upper() for x in a]:
                    ils = a[[x.upper() for x in a].index("ILS") + 1]
                s.runway(a[0], a[1], f(2), f(3), f(4), f(5), f(6), ils, "LUCES" in [x.upper() for x in a])
            elif cmd == "VOR":
                typ = NAV_VORDME if "DME" in [x.upper() for x in a] else NAV_VOR
                s.navs.append((typ, int(round(f(1) * 100)), f(2), f(3), s.elev, 0, 200, 0, a[0]))
            elif cmd == "NDB":
                s.navs.append((NAV_NDB, int(f(1)), f(2), f(3), s.elev, 0, 80, 0, a[0]))
            elif cmd == "EDIFICIO":
                fill = FILLS[a[6].upper()] if len(a) > 6 else FILLS["GRISCLARO"]
                s.box(f(0), f(1), f(2), f(3), f(4), f(5), fill, FILLS["ROJO"])
            elif cmd == "TORRE":
                x, y = f(0), f(1)
                o = s.box(x, y, 6, 6, 24, 0, FILLS["GRISCLARO"], FILLS["GRISCLARO"])
                s.box(x, y, 10, 10, 6, 45, FILLS["GRIS"], FILLS["ROJO"], z0=0, obj=None)
                s.objs[-1].oz = s.elev + 24
                s.objs[-1].v = [(vx, vy, vz) for vx, vy, vz in s.objs[-1].v]
                s.lights([(x, y, 32)], C_RED, flags=OF_DAYLIGHT * 0)
            elif cmd == "HANGAR":
                s.box(f(0), f(1), 40, 30, 12, f(2), FILLS["BLANCO"], FILLS["GRIS"])
            elif cmd == "CIUDAD":
                s.city(f(0), f(1), f(2), f(3), int(f(4)), int(f(5)))
            elif cmd in ("CARRETERA", "RIO"):
                pts = [(f(i), f(i + 1)) for i in range(0, len(a) - 1, 2)]
                col = C_BLACK if cmd == "CARRETERA" else C_WHITE
                for k in range(0, len(pts) - 1, 40):
                    seg = pts[k:k + 41]
                    cx = sum(p[0] for p in seg) / len(seg)
                    cy = sum(p[1] for p in seg) / len(seg)
                    o = Obj(OBJ_LINES, cx, cy, s.elev, 0, lod=60)
                    o.face(col, [o.vert(x, y) for x, y in seg])
                    s.objs.append(o)
            elif cmd == "LAGO":
                x, y, r = f(0), f(1), f(2)
                n = int(f(3)) if len(a) > 3 else 12
                rnd = random.Random(int(x * 7 + y))
                o = Obj(OBJ_GROUND, x, y, s.elev, OF_WATER, lod=90)
                idx = [o.vert(x + r * (0.8 + 0.3 * rnd.random()) * math.sin(2 * math.pi * i / n),
                              y + r * (0.8 + 0.3 * rnd.random()) * math.cos(2 * math.pi * i / n)) for i in range(n)]
                o.face(attr(FILLS["AGUA"]), idx[::-1])
                s.objs.append(o)
            elif cmd == "CAMPO":
                fill = FILLS[a[0].upper()]
                pts = [(f(i), f(i + 1)) for i in range(1, len(a) - 1, 2)]
                cx = sum(p[0] for p in pts) / len(pts)
                cy = sum(p[1] for p in pts) / len(pts)
                o = Obj(OBJ_GROUND, cx, cy, s.elev, 0, lod=50)
                o.face(attr(fill), [o.vert(px_, py_) for px_, py_ in pts])
                s.objs.append(o)
            elif cmd == "CALLE":
                # CALLE ancho x1 y1 x2 y2 ... : calle de rodaje (tramos rectos)
                w = f(0) / 2
                pts = [(f(i), f(i + 1)) for i in range(1, len(a) - 1, 2)]
                cx = sum(p[0] for p in pts) / len(pts)
                cy = sum(p[1] for p in pts) / len(pts)
                o = Obj(OBJ_GROUND, cx, cy, s.elev, 0, lod=20)
                for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
                    L = math.hypot(x1 - x0, y1 - y0) or 1
                    nx, ny = -(y1 - y0) / L * w, (x1 - x0) / L * w
                    o.face(attr(FILLS["GRISOSC"]), [o.vert(x0 + nx, y0 + ny), o.vert(x0 - nx, y0 - ny),
                                                     o.vert(x1 - nx, y1 - ny), o.vert(x1 + nx, y1 + ny)])
                s.objs.append(o)
            elif cmd == "PLATAFORMA":
                # PLATAFORMA x y largo fondo rumbo : plataforma de estacionamiento
                x, y, L, D, r = f(0), f(1), f(2), f(3), math.radians(f(4))
                dx, dy = math.sin(r), math.cos(r)
                px, py = dy, -dx
                if s.apt_order and "plat" not in s.apts[s.apt_order[-1]]:     # la primera plataforma de cada aeropuerto: salida "en plataforma"
                    s.apts[s.apt_order[-1]]["plat"] = (x, y, f(4))
                o = Obj(OBJ_GROUND, x, y, s.elev, 0, lod=20)
                o.face(attr(FILLS["GRISOSC"]), [o.vert(x + sx * dx * L / 2 + sy * px * D / 2, y + sx * dy * L / 2 + sy * py * D / 2)
                                                 for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
                s.objs.append(o)
            elif cmd == "MONTANA":
                x, y, r, h = f(0), f(1), f(2), f(3)
                n = int(f(4)) if len(a) > 4 else 7
                rnd = random.Random(int(x + y * 3))
                o = Obj(OBJ_SOLID, x, y, s.elev, OF_HIT | OF_MOUNTAIN | OF_SORT, lod=255)
                base = [o.vert(x + r * (0.75 + 0.35 * rnd.random()) * math.sin(2 * math.pi * i / n),
                               y + r * (0.75 + 0.35 * rnd.random()) * math.cos(2 * math.pi * i / n), 0) for i in range(n)]
                top = o.vert(x + r * 0.1 * (rnd.random() - 0.5), y + r * 0.1 * (rnd.random() - 0.5), h)
                for i in range(n):
                    j = (i + 1) % n
                    fill = FILLS["MARRON"] if i % 2 else FILLS["VERDEOSC"]
                    o.face(attr(fill, C_BLACK), [base[j], base[i], top])
                s.objs.append(o)
            else:
                raise SystemExit("orden desconocida: " + cmd)

    def city(s, x, y, w, d, n, seed):
        rnd = random.Random(seed)
        boxes = []
        for i in range(n):
            bx = x + (rnd.random() - 0.5) * w
            by = y + (rnd.random() - 0.5) * d
            bx, by = round(bx / 120) * 120, round(by / 120) * 120
            boxes.append((bx, by, rnd.choice((30, 40, 50, 60)), rnd.choice((30, 40, 60)),
                          rnd.choice((10, 15, 20, 30, 45, 70)), rnd.choice(("GRISCLARO", "BLANCO", "GRIS", "NARANJA"))))
        boxes = list(dict(((b[0], b[1]), b) for b in boxes).values())
        # agrupar en manzanas de hasta 10 edificios (80 vertices) con orden de caras
        # manzanas compactas: los edificios se agrupan por celdas de 1500 m y cada celda en grupos de hasta 10 (80 vertices).
        # Antes eran franjas de 600 m de alto y todo el ancho de la ciudad: un objeto de 3-4 km de radio que impedia descartar
        # por esfera o por distancia y se transformaba entero aunque solo se viera una esquina.
        cells = {}
        for b in boxes:
            cells.setdefault((b[1] // 1500, b[0] // 1500), []).append(b)
        for key in sorted(cells):
            cb = sorted(cells[key], key=lambda b: (b[1], b[0]))
            for k in range(0, len(cb), 10):
                grp = cb[k:k + 10]
                cx = sum(b[0] for b in grp) / len(grp)
                cy = sum(b[1] for b in grp) / len(grp)
                o = Obj(OBJ_SOLID, cx, cy, s.elev, OF_HIT | OF_SORT, lod=40)
                for bx, by, bw, bd, bh, fl in grp:
                    s.box(bx, by, bw, bd, bh, 0, FILLS[fl], FILLS["ROJO"], obj=o)
                s.objs.append(o)
        # calles (cuadricula) y luces
        pts, lines = [], []
        gx = int(w // 240)
        gy = int(d // 240)
        for i in range(-gx, gx + 1):
            lines.append([(x + i * 120 + 60, y - d / 2), (x + i * 120 + 60, y + d / 2)])
        for j in range(-gy, gy + 1):
            lines.append([(x - w / 2, y + j * 120 + 60), (x + w / 2, y + j * 120 + 60)])
        for ln in lines:      # las calles solo se ven de noche, por sus luces
            L = math.hypot(ln[1][0] - ln[0][0], ln[1][1] - ln[0][1])
            for t in range(0, int(L), 240):
                u = t / L
                pts.append((ln[0][0] + (ln[1][0] - ln[0][0]) * u, ln[0][1] + (ln[1][1] - ln[0][1]) * u, 4))
        s.lights(pts, C_WHITE, lod=90)
        s.lights([(bx, by, bh + 1) for bx, by, bw, bd, bh, fl in boxes if bh >= 45], C_RED, lod=90)

    # ---------------------------------------------------------------- binario
    def write(s, out):
        T = 1 << s.tlog
        xs = [o.ox for o in s.objs] + [a["x"] for a in s.apts.values()]
        ys = [o.oy for o in s.objs] + [a["y"] for a in s.apts.values()]
        ox = int(math.floor(min(xs) / T)) * T - T
        oy = int(math.floor(min(ys) / T)) * T - T
        ntx = int((max(xs) - ox) // T) + 2
        nty = int((max(ys) - oy) // T) + 2
        tiles = {}
        for o in s.objs:
            k = (int((o.ox - ox) // T), int((o.oy - oy) // T))
            tiles.setdefault(k, []).append(o)
        navb = b""
        for typ, fq, x, y, z, crs, rng, gs, ident in s.navs:
            navb += struct.pack(">BBHiihHHH4s", typ, 0, fq, int(x), int(y), int(z),
                                int(round(crs * 65536 / 360)) & 0xFFFF, rng,
                                int(round(gs * 65536 / 360)), ident.encode()[:4].ljust(4))
        hdr_len, tdir_len = 64, ntx * nty * 8
        off_tdir = hdr_len
        off_nav = off_tdir + tdir_len
        off_apt = off_nav + len(navb)
        texts = b""
        aptb = b""
        off_txt = off_apt + 120 * len(s.apt_order)
        for k in s.apt_order:
            a = s.apts[k]
            rw = b""
            for (h, ln, wd, x, y) in a["rwy"][:4]:
                rw += struct.pack(">HHHhii", int(round(h * 65536 / 360)) & 0xFFFF, int(ln), int(wd), 0, int(x), int(y))
            rw = rw.ljust(64, b"\0")
            t = a["text"].upper().encode()
            # plataforma: desplazamiento respecto al aeropuerto en unidades de 8 m (signed byte, byte 47 = este, byte 118 = norte)
            # y rumbo en 1/256 de vuelta (byte 119, nunca 0); todo a 0 = el aeropuerto no tiene plataforma
            px8 = py8 = ph8 = 0
            if "plat" in a:
                px, py, ph = a["plat"]
                px8 = max(-127, min(127, int(round((px - a["x"]) / 8.0))))
                py8 = max(-127, min(127, int(round((py - a["y"]) / 8.0))))
                ph8 = max(1, int(round(ph * 256 / 360)) & 0xFF)
            aptb += struct.pack(">4s28siihHHBb", k.encode()[:4].ljust(4), a["name"].upper().encode()[:28].ljust(28),
                                int(a["x"]), int(a["y"]), a["elev"], a["atis"], a["twr"], min(4, len(a["rwy"])), px8)
            aptb += rw + struct.pack(">IHbB", off_txt + len(texts), len(t), py8, ph8)
            texts += t
        blobs = b""
        off_blob = off_txt + len(texts)
        if off_blob & 1:
            texts += b"\0"
            off_blob += 1
        tdir = bytearray(tdir_len)
        for (tx, ty), objs in sorted(tiles.items()):
            blob = struct.pack(">H", len(objs)) + b"".join(o.pack() for o in objs)
            assert len(blob) < 65536
            struct.pack_into(">IHh", tdir, (ty * ntx + tx) * 8, off_blob + len(blobs), len(blob), s.elev)
            blobs += blob
            if len(blobs) & 1:
                blobs += b"\0"
        for ty in range(nty):
            for tx in range(ntx):
                if (tx, ty) not in tiles:
                    struct.pack_into(">IHh", tdir, (ty * ntx + tx) * 8, 0, 0, s.elev)
        hdr = struct.pack(">4sHHiiHHHHIIIhH24s", b"QFS1", 1, s.tlog, ox, oy, ntx, nty, len(s.navs),
                          len(s.apt_order), off_tdir, off_nav, off_apt, s.elev, s.vis, s.name.upper().encode()[:24].ljust(24))
        hdr = hdr.ljust(64, b"\0")
        data = hdr + bytes(tdir) + navb + aptb + texts + blobs
        open(out, "wb").write(data)
        sizes = sorted(len(struct.pack(">H", 0) + b"".join(o.pack() for o in v)) for v in tiles.values())
        print("%s: %d bytes, %d teselas de %d m (rejilla %dx%d), %d objetos, %d radioayudas, %d aeropuertos"
              % (out, len(data), len(tiles), T, ntx, nty, len(s.objs), len(s.navs), len(s.apt_order)))
        print("  tesela min/media/max: %d / %d / %d bytes" % (sizes[0], sum(sizes) // len(sizes), sizes[-1]))


if __name__ == "__main__":
    sc = Scen()
    sc.parse(sys.argv[1])
    sc.write(sys.argv[2])
