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


class Designer:
    def __init__(self, cones, cities, apts, navs):
        self.cones = cones            # (x, y, r, h_m)
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
                h = max(h, ch * (1 - d / r))
        return ft(h)

    def msa_sectors(self, c, radius=25 * NM):
        """4 sectores (000-090, 090-180, 180-270, 270-360): obstaculo + 1000/2000 ft"""
        res = []
        for s in range(4):
            h = 0.0
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
                            h = max(h, ch * (1 - rr / r) if r else ch)
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
            path = chaikin(wps)
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
        P = {"vid": vid, "vfreq": vfreq, "vpos": vpos, "icao": L["icao"]}
        P["msa"] = self.msa_sectors(vpos)
        cards = (("N", 0), ("E", 90), ("S", 180), ("W", 270))
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
            p1 = add(p0, mul(D["d"], 2.5 * NM))
            for (fname, cd, b0, _), dl in zip(P["exits"], range(4)):
                cands = []
                for db in (0, -15, 15, -30, 30):
                    E = polar(vpos, b0 + db, 30 * NM)
                    vias = [None]
                    r = right(D["d"])
                    for a in (1.5, 4):
                        for s in (-7, -4, 4, 7):
                            vias.append(add(p1, add(mul(D["d"], a * NM), mul(r, s * NM))))
                    mid = mul(add(p1, E), 0.5)
                    u = unit(sub(E, p1))
                    for s in (-8, 8):
                        vias.append(add(mid, mul(right(u), s * NM)))
                    for v in vias:
                        cands.append([p0, p1] + ([v] if v else []) + [E])
                grad = 300.0                      # ft/NM (~5 %)
                alt = lambda s, g=grad: 400 + g * s / NM
                forbid = None
                best = self.best_route(cands, alt, 1000, start_skip=1 * NM)
                if not best[2]:
                    grad = 450.0                  # ~7.4 %
                    alt = lambda s, g=grad: 400 + g * s / NM
                    best = self.best_route(cands, alt, 1000, start_skip=1 * NM)
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
        gp = lambda dm: (dm + 300) * math.tan(math.radians(3)) * 3.28084      # ft sobre el umbral
        if_alt = int(math.ceil(gp(10 * NM) / 100.0) * 100)
        faf_alt = int(round(gp(5 * NM) / 10.0) * 10)
        final_line = LineString([IF, thr, D["end"]]).buffer(0.6 * NM)
        arrivals = []
        r = right(d)
        for fname, cd, b, A in P["iafs"]:
            cands = []
            for s in (0, -6, 6, -10, 10):
                for a in (0, 3, 6):
                    if s == 0 and a:
                        continue
                    via = None if s == 0 else add(sub(IF, mul(d, a * NM)), mul(r, s * NM))
                    cands.append([A] + ([via] if via else []) + [ifa, IF])
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
            arrivals.append(dict(fix=fname, card=cd, A=A, path=path, mnm=best[1], length=plen(path) / NM,
                                 radial=round(brg(vpos, A))))
        # espera en el IAF con la llegada mas corta
        h = min(arrivals, key=lambda a: a["length"])
        u = unit(sub(h["path"][3], h["path"][0]))
        hold = dict(fix=h["fix"], pos=h["A"], course=brg((0, 0), u), alt=max(5000, h["mnm"]), u=u)
        hold["shape"] = self.racetrack(h["A"], u)
        # frustrada: rumbo de pista hasta 2 NM pasado el extremo, curva hacia la espera
        m0 = thr
        m1 = add(D["end"], mul(d, 2.5 * NM))
        cands = []
        for s in (-5, 5, -8, 8):
            for a in (2, 5):
                cands.append([m0, m1, add(add(m1, mul(d, a * NM)), mul(right(d), s * NM)), h["A"]])
        cands.append([m0, m1, h["A"]])
        grad = 152.0
        altf = lambda s, g=grad: 50 + g * s / NM
        mb = self.best_route(cands, altf, 500, start_skip=D["len"] + 1 * NM)
        if not mb[2]:
            grad = 250.0
            altf = lambda s, g=grad: 50 + g * s / NM
            mb = self.best_route(cands, altf, 500, start_skip=D["len"] + 1 * NM)
        missed = dict(path=mb[1], grad=grad, ok=mb[2], turn=self.turn_side(d, mb[1]), first_alt=2000)
        # minimos
        t_final = 0.0
        for p, s in samples([FAF, thr], 0.2 * NM):
            for q in (add(p, mul(right(d), NM)), sub(p, mul(right(d), NM)), p):
                t_final = max(t_final, self.terrain_ft(q))
        if kind == "ILS":
            och = [max(200, int(t_final + 250)) + k * 10 for k in range(4)]
            loc_och = [max(400, int(t_final + 350)) + k * 20 for k in range(4)]
        else:
            och = [max(400, int(t_final + 350)) + k * 20 for k in range(4)]
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
