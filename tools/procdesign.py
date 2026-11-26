#!/usr/bin/env python3
"""procdesign.py - diseno automatico de procedimientos instrumentales para QFS.

Para cada aeropuerto del escenario iberico genera:
  - 4 fijos de salida (N, E, S, W) a 30 DME del VOR del aeropuerto
  - 4 IAF (N, E, S, W) a 16 DME
  - SID: por cada cabecera, una ruta suave (sin esquinas) hasta cada fijo de salida
  - IAC: aproximacion ILS (si la hay) y VOR/DME para cada cabecera, con rutas
    suaves desde los IAF hasta el IF, espera en hipodromo y frustrada.
Las rutas se eligen entre varias candidatas: la mas corta que libra el relieve del
simulador (conos) con margen, y que evita en lo posible sobrevolar ciudades a baja altura.
"""
import math, random
from shapely.geometry import LineString, Point, box

NM = 1852.0


def add(a, b): return (a[0] + b[0], a[1] + b[1])
def sub(a, b): return (a[0] - b[0], a[1] - b[1])
def mul(a, k): return (a[0] * k, a[1] * k)
def dist(a, b): return math.hypot(b[0] - a[0], b[1] - a[1])


def unit(a):
    L = math.hypot(*a) or 1.0
    return (a[0] / L, a[1] / L)


def right(u): return (u[1], -u[0])          # perpendicular a la derecha (x este, y norte)
def polar(p, brg, d): return (p[0] + math.sin(math.radians(brg)) * d, p[1] + math.cos(math.radians(brg)) * d)
def brg(a, b): return math.degrees(math.atan2(b[0] - a[0], b[1] - a[1])) % 360


def chaikin(pts, it=5):
    for _ in range(it):
        out = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            out.append((a[0] * 0.75 + b[0] * 0.25, a[1] * 0.75 + b[1] * 0.25))
            out.append((a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75))
        out.append(pts[-1])
        pts = out
    return pts


HOLD_R = 1.5 * NM          # radio de los virajes del hipodromo de espera: ningun viraje de una ruta puede ser mas cerrado


def fillet(wps, R=HOLD_R, first_margin=0.4 * NM):
    """ruta suave: segmentos rectos unidos por arcos de circunferencia de radio R (tangentes a los dos tramos), sin esquinas.
    Cada vertice interior se sustituye por un arco; si el viraje no cabe (los tramos son mas cortos que las tangentes de los
    arcos que comparten) o es de ~180 grados, lanza ValueError y la ruta se descarta. El primer arco no empieza antes de
    first_margin desde el primer punto."""
    n = len(wps)
    if n < 3:
        return list(wps)
    us = [unit(sub(wps[i + 1], wps[i])) for i in range(n - 1)]
    lens = [dist(wps[i], wps[i + 1]) for i in range(n - 1)]
    tans = [0.0] * n
    ths = [0.0] * n
    for i in range(1, n - 1):
        dot = max(-1.0, min(1.0, us[i - 1][0] * us[i][0] + us[i - 1][1] * us[i][1]))
        th = math.acos(dot)
        if th > math.radians(165):
            raise ValueError("viraje imposible")
        ths[i] = th
        tans[i] = R * math.tan(th / 2) if th > math.radians(0.3) else 0.0
    for i in range(n - 1):
        need = tans[i] + tans[i + 1] + (first_margin if i == 0 and tans[1] > 0 else 0.0)
        if need > lens[i] + 1e-6:
            raise ValueError("tramo corto")
    out = [wps[0]]
    for i in range(1, n - 1):
        if tans[i] == 0.0:
            out.append(wps[i])
            continue
        A = sub(wps[i], mul(us[i - 1], tans[i]))
        left = us[i - 1][0] * us[i][1] - us[i - 1][1] * us[i][0] > 0
        nrm = (-us[i - 1][1], us[i - 1][0]) if left else (us[i - 1][1], -us[i - 1][0])
        c = add(A, mul(nrm, R))
        a0 = math.atan2(A[1] - c[1], A[0] - c[0])
        k = max(2, int(math.ceil(math.degrees(ths[i]) / 4.0)))
        for j in range(k + 1):
            a = a0 + (ths[i] * j / k) * (1 if left else -1)
            out.append((c[0] + math.cos(a) * R, c[1] + math.sin(a) * R))
    out.append(wps[-1])
    return out


def min_radius(path):
    """radio de curvatura minimo de una poligonal densa (circunferencia por tres puntos consecutivos), para comprobar rutas"""
    best = 1e18
    for a, b, c in zip(path, path[1:], path[2:]):
        ab, bc, ca = dist(a, b), dist(b, c), dist(c, a)
        cr = abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
        if cr > 1e-6 and ab > 1 and bc > 1:
            best = min(best, ab * bc * ca / (2 * cr))
    return best


def plen(path):
    return sum(dist(a, b) for a, b in zip(path, path[1:]))


def samples(path, step=0.25 * NM):
    """puntos cada 'step' metros con su distancia acumulada"""
    out = [(path[0], 0.0)]
    acc, carry = 0.0, 0.0
    for a, b in zip(path, path[1:]):
        L = dist(a, b)
        t = step - carry
        while t <= L:
            out.append(((a[0] + (b[0] - a[0]) * t / L, a[1] + (b[1] - a[1]) * t / L), acc + t))
            t += step
        carry = L - (t - step)
        acc += L
    return out


def point_at(path, s):
    """punto y direccion a la distancia s del origen del camino"""
    acc = 0.0
    for a, b in zip(path, path[1:]):
        L = dist(a, b)
        if acc + L >= s and L > 0:
            t = (s - acc) / L
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), unit(sub(b, a))
        acc += L
    return path[-1], unit(sub(path[-1], path[-2]))


def ft(m): return m * 3.28084


def load_fixes(path):
    """listado de puntos de notificacion (data/fijos.txt): { icao: {"SID": [(nombre, cardinal, radial, dme)], "IAF": [...]} }.
    Es la unica fuente de los fijos: todas las SID y las aproximaciones de un aeropuerto usan los mismos puntos."""
    t = {}
    for ln in open(path, encoding="utf-8"):
        ln = ln.split(";")[0].split()
        if len(ln) < 6:
            continue
        icao, name, use, card, radial, dme = ln[:6]
        t.setdefault(icao, {"SID": [], "IAF": []})[use].append((name, card, float(radial), float(dme)))
    return t


class Designer:
    def __init__(self, cones, cities, apts, navs, ground=None, fixes=None):
        self.fixes = fixes                # listado de fijos (load_fixes) o None: se inventan nombres (solo para generar el listado)
        self.cones = cones            # (x, y, r, h_m): alturas sobre el suelo de su tesela
        self.ground = ground or (lambda x, y: 0.0)       # elevacion del suelo (m) en un punto
        self.cities = [(n, box(x - w / 2 - 1500, y - d / 2 - 1500, x + w / 2 + 1500, y + d / 2 + 1500))
                       for n, x, y, w, d in cities]
        self.apts = apts
        self.navs = navs
        self.used = set(navs.keys())
        self.rnd = random.Random(1982)

    # --------------------------------------------------------------- relieve
    def terrain_ft(self, p, cones=None):
        h = 0.0
        for cx, cy, r, ch in (self.cones if cones is None else cones):
            d = math.hypot(p[0] - cx, p[1] - cy)
            if d < r:
                h = max(h, self.ground(cx, cy) + ch * (1 - d / r))       # los conos apoyan en el suelo de su tesela
        return ft(h)         # (el suelo llano sin montanas no cuenta como obstaculo: 0)

    def msa_sectors(self, c, radius=25 * NM):
        """4 sectores (000-090, 090-180, 180-270, 270-360): obstaculo + 1000/2000 ft"""
        res = []
        for s in range(4):
            h = self.ground(*c)
            for x, y, r, ch in self.cones:
                # muestreo del cono dentro del sector
                for k in range(16):
                    a = 2 * math.pi * k / 16
                    for rr in (0, r * 0.5):
                        p = (x + math.cos(a) * rr, y + math.sin(a) * rr)
                        dd = dist(c, p)
                        if dd > radius + 0.1:
                            continue
                        b = brg(c, p)
                        if s * 90 <= b < s * 90 + 90 or dd < 1:
                            h = max(h, self.ground(x, y) + (ch * (1 - rr / r) if r else ch))
            f = ft(h)
            f += 2000 if f > 5000 else 1000
            res.append(max(2000, int(math.ceil(f / 100.0) * 100)))
        return res

    # --------------------------------------------------------------- nombres
    def name(self):
        C, V = "BDFGKLMNPRSTVZ", "AEIOU"
        while True:
            n = (self.rnd.choice(C) + self.rnd.choice(V) + self.rnd.choice(C) + self.rnd.choice(V) +
                 self.rnd.choice("LNRSTZ"))
            if n not in self.used:
                self.used.add(n)
                return n

    # --------------------------------------------------------------- evaluacion de rutas
    def check(self, path, alt_fn, margin_ft, low_city_ft=5000, start_skip=0.0):
        """devuelve (ok_relieve, penalizacion_ciudades, peor_margen)"""
        pen = 0.0
        worst = 1e9
        ok = True
        xs = [q[0] for q in path]
        ys = [q[1] for q in path]
        near = [c for c in self.cones if min(xs) - c[2] < c[0] < max(xs) + c[2] and min(ys) - c[2] < c[1] < max(ys) + c[2]]
        cits = [g for n, g in self.cities if g.intersects(LineString(path))]
        for p, s in samples(path):
            alt = alt_fn(s)
            t = self.terrain_ft(p, near) if near else 0.0
            if s >= start_skip and t > 0:
                need = min(margin_ft, max(300.0, alt * 0.4))
                worst = min(worst, alt - t - need)
                if alt - t < need:
                    ok = False
            if alt < low_city_ft and cits:
                for g in cits:
                    if g.contains(Point(p)):
                        pen += 0.25 * NM * 6
        return ok, pen, (worst if worst < 1e9 else 0.0)

    def best_route(self, wps_list, alt_fn, margin_ft, forbid=None, start_skip=0.0):
        best = None
        for wps in wps_list:
            try:
                path = fillet(wps)
            except ValueError:
                continue
            ok, pen, worst = self.check(path, alt_fn, margin_ft, start_skip=start_skip)
            if forbid is not None and LineString(path[:max(2, int(len(path) * 0.82))]).intersects(forbid):
                continue
            score = plen(path) + pen + (0 if ok else 1e7 - worst * 100)
            if best is None or score < best[0]:
                best = (score, path, ok, worst, wps)
        return best

    # --------------------------------------------------------------- aeropuerto
    def design(self, L):
        vid, vfreq, vpos = L["vor"]
        P = {"vid": vid, "vfreq": vfreq, "vpos": vpos, "icao": L["icao"], "elev_ft": ft(L["elev"])}
        elev_ft = P["elev_ft"]
        P["msa"] = self.msa_sectors(vpos)
        cards = (("N", 0), ("E", 90), ("S", 180), ("W", 270))
        if self.fixes is not None:
            fx = self.fixes[L["icao"]]
            P["exits"] = [(n, cd, b, polar(vpos, b, d * NM)) for n, cd, b, d in fx["SID"]]
            P["iafs"] = [(n, cd, b, polar(vpos, b, d * NM)) for n, cd, b, d in fx["IAF"]]
        else:
            P["exits"] = [(self.name(), cd, b, polar(vpos, b, 30 * NM)) for cd, b in cards]
            P["iafs"] = [(self.name(), cd, b, polar(vpos, b, 16 * NM)) for cd, b in cards]
        # cabeceras
        dirs = []
        for rw in L["runways"]:
            d = rw["d"]
            dirs.append(dict(num=rw["num"], hdg=rw["hdg"], thr=rw["thr"], end=rw["end"], d=d, len=rw["len"],
                             ils=rw["ils"], loc=rw.get("loc"), gs=rw.get("gs"), om=rw.get("om"), mm=rw.get("mm"),
                             rw=rw))
            dirs.append(dict(num=rw["rnum"], hdg=(rw["hdg"] + 180) % 360, thr=rw["end"], end=rw["thr"],
                             d=(-d[0], -d[1]), len=rw["len"], ils=None, rw=rw))
        P["dirs"] = dirs
        runways_geo = [LineString([rw["thr"], rw["end"]]).buffer(600) for rw in L["runways"]]
        # SID
        P["sids"] = {}
        letters = "ABCDEFGH"
        for k, D in enumerate(dirs):
            sids = []
            p0 = D["end"]
            for (fname, cd, b0, _), dl in zip(P["exits"], range(4)):
                cands = []
                for L1 in (2.5, 4.5, 7.0):         # primer tramo recto en el rumbo de pista (el viraje no cabe si es corto)
                    p1 = add(p0, mul(D["d"], L1 * NM))
                    for db in (0,):                    # el fijo de salida es el del listado: no se mueve de una pista a otra
                        E = polar(vpos, b0 + db, 30 * NM)
                        vias = [None]
                        r = right(D["d"])
                        for a in (1.5, 4, 7):
                            for s in (-7, -4, 4, 7, -11, 11):
                                vias.append(add(p1, add(mul(D["d"], a * NM), mul(r, s * NM))))
                        mid = mul(add(p1, E), 0.5)
                        u = unit(sub(E, p1))
                        for s in (-8, 8, -14, 14):
                            vias.append(add(mid, mul(right(u), s * NM)))
                        for v in vias:
                            cands.append([p0, p1] + ([v] if v else []) + [E])
                grad = 300.0                      # ft/NM (~5 %)
                alt = lambda s, g=grad: elev_ft + 400 + g * s / NM
                forbid = None
                best = self.best_route(cands, alt, 1000, start_skip=1 * NM)
                if best is None or not best[2]:
                    grad = 450.0                  # ~7.4 %
                    alt = lambda s, g=grad: elev_ft + 400 + g * s / NM
                    best = self.best_route(cands, alt, 1000, start_skip=1 * NM)
                if best is None:
                    raise RuntimeError("SID sin ruta con radio >= %.1f NM: %s %s" % (HOLD_R / NM, P["icao"], fname))
                path = best[1]
                E = best[4][-1]
                exit_alt = max(6000, max(P["msa"]))
                turn = self.turn_side(D["d"], path)
                sids.append(dict(name="%s %d%s" % (fname, 1, letters[k]), fix=fname, card=cd, path=path,
                                 E=E, radial=round(brg(vpos, E)), dme=dist(vpos, E) / NM, grad=grad,
                                 ok=best[2], exit_alt=exit_alt, turn=turn, length=plen(path) / NM))
            P["sids"][D["num"]] = sids
        # IAC
        P["iacs"] = []
        for D in dirs:
            kinds = (["ILS"] if D["ils"] else []) + ["VOR"]
            for kind in kinds:
                P["iacs"].append(self.approach(P, L, D, kind, runways_geo))
        return P

    def turn_side(self, d, path):
        """lado del primer viraje significativo"""
        u0 = d
        for p, s in samples(path, 0.5 * NM)[6:]:
            pass
        q, u1 = point_at(path, min(plen(path) * 0.5, 12 * NM))
        cr = u0[0] * u1[1] - u0[1] * u1[0]
        dot = u0[0] * u1[0] + u0[1] * u1[1]
        if dot > 0.97:
            return "recto"
        return "izquierda" if cr > 0 else "derecha"

    def approach(self, P, L, D, kind, runways_geo):
        vpos = P["vpos"]
        thr, d = D["thr"], D["d"]
        IF = sub(thr, mul(d, 10 * NM))
        FAF = sub(thr, mul(d, 5 * NM))
        ifa = sub(IF, mul(d, 2.5 * NM))
        elev_ft = P["elev_ft"]
        gp = lambda dm: elev_ft + (dm + 300) * math.tan(math.radians(3)) * 3.28084      # altitud (ft) de la senda sobre el umbral
        if_alt = int(math.ceil(gp(10 * NM) / 100.0) * 100)
        faf_alt = int(round(gp(5 * NM) / 10.0) * 10)
        final_line = LineString([IF, thr, D["end"]]).buffer(0.6 * NM)
        arrivals = []
        r = right(d)
        for fname, cd, b, A in P["iafs"]:
            cands = []
            for back in (2.5, 4.5, 7.0):          # punto de interceptacion antes del IF: el viraje final no puede ser mas cerrado que la espera
                ifb = sub(IF, mul(d, back * NM))
                for s in (0, -6, 6, -10, 10, -14, 14):
                    for a in (0, 3, 6, 9):
                        if s == 0 and a:
                            continue
                        via = None if s == 0 else add(sub(IF, mul(d, a * NM)), mul(r, s * NM))
                        cands.append([A] + ([via] if via else []) + [ifb, IF])
            best = None
            for alt_try in range(4000, 13100, 500):
                altf = lambda s, a0=alt_try: a0
                bb = self.best_route(cands, altf, 1000, forbid=final_line)
                if bb and bb[2]:
                    best = (bb, alt_try)
                    break
            if best is None:
                bb = self.best_route(cands, lambda s: 13000, 1000, forbid=final_line)
                if not bb:
                    continue
                best = (bb, 13000)
            path = best[0][1]
            wps = best[0][4]
            arrivals.append(dict(fix=fname, card=cd, A=A, path=path, mnm=best[1], length=plen(path) / NM,
                                 radial=round(brg(vpos, A)), u0=unit(sub(wps[1], wps[0]))))
        # espera en el IAF con la llegada mas corta
        h = min(arrivals, key=lambda a: a["length"])
        u = h["u0"]
        hold = dict(fix=h["fix"], pos=h["A"], course=brg((0, 0), u), alt=max(5000, h["mnm"]), u=u)
        hold["shape"] = self.racetrack(h["A"], u)
        # frustrada: rumbo de pista hasta 2 NM pasado el extremo, curva hacia la espera
        m0 = thr
        m1 = add(D["end"], mul(d, 2.5 * NM))
        cands = []
        for ext in (2.5, 4.5, 7.0):            # tramo recto tras el extremo de pista antes del viraje
            m1 = add(D["end"], mul(d, ext * NM))
            for s in (-5, 5, -8, 8, -12, 12):
                for a in (2, 5, 8):
                    cands.append([m0, m1, add(add(m1, mul(d, a * NM)), mul(right(d), s * NM)), h["A"]])
            cands.append([m0, m1, h["A"]])
        grad = 152.0
        altf = lambda s, g=grad: elev_ft + 50 + g * s / NM
        mb = self.best_route(cands, altf, 500, start_skip=D["len"] + 1 * NM)
        if mb is None or not mb[2]:
            grad = 250.0
            altf = lambda s, g=grad: elev_ft + 50 + g * s / NM
            mb = self.best_route(cands, altf, 500, start_skip=D["len"] + 1 * NM)
        if mb is None:
            raise RuntimeError("frustrada sin ruta con radio >= %.1f NM: %s %s" % (HOLD_R / NM, P["icao"], D["num"]))
        missed = dict(path=mb[1], grad=grad, ok=mb[2], turn=self.turn_side(d, mb[1]),
                      first_alt=max(2000, int(math.ceil((elev_ft + 1500) / 500.0) * 500)))
        # minimos
        t_final = 0.0
        for p, s in samples([FAF, thr], 0.2 * NM):
            for q in (add(p, mul(right(d), NM)), sub(p, mul(right(d), NM)), p):
                t_final = max(t_final, self.terrain_ft(q))
        if kind == "ILS":
            och = [max(elev_ft + 200, int(t_final + 250)) + k * 10 for k in range(4)]          # altitudes (OCA)
            loc_och = [max(elev_ft + 400, int(t_final + 350)) + k * 20 for k in range(4)]
        else:
            och = [max(elev_ft + 400, int(t_final + 350)) + k * 20 for k in range(4)]
            loc_och = None
        # fijos del perfil con DME (VOR o ILS)
        dme_src = D.get("loc") if kind == "ILS" else vpos
        return dict(kind=kind, D=D, IF=IF, FAF=FAF, if_alt=if_alt, faf_alt=faf_alt, arrivals=arrivals,
                    hold=hold, missed=missed, och=och, loc_och=loc_och, dme_src=dme_src,
                    name="%s RWY %s" % ("ILS" if kind == "ILS" else "VOR/DME", D["num"]))

    def racetrack(self, fix, u, L=3 * NM, R=1.5 * NM):
        """hipodromo con virajes a la derecha: tramo de acercamiento termina en el fijo"""
        rt = right(u)
        S = sub(fix, mul(u, L))
        pts = []
        c1 = add(fix, mul(rt, R))
        a0 = math.atan2(-rt[1], -rt[0])
        for i in range(19):          # viraje sobre el fijo (a la derecha = horario)
            a = a0 - math.pi * i / 18
            pts.append((c1[0] + math.cos(a) * R, c1[1] + math.sin(a) * R))
        c2 = add(S, mul(rt, R))
        a1 = math.atan2(rt[1], rt[0])
        for i in range(19):
            a = a1 - math.pi * i / 18
            pts.append((c2[0] + math.cos(a) * R, c2[1] + math.sin(a) * R))
        pts.append(pts[0])
        return pts
