#!/usr/bin/env python3
"""mapa_scn.py - dibuja un .SCN visto desde arriba (comprobacion de escenarios).
Uso: mapa_scn.py escenario.scn mapa.png [ancho]"""
import struct, sys
from PIL import Image, ImageDraw

d = open(sys.argv[1], "rb").read()
W = int(sys.argv[3]) if len(sys.argv) > 3 else 1400
mag, ver, tlog, ox, oy, ntx, nty, nnav, napt, otd, onav, oapt, elev, vis = struct.unpack(">4sHHiiHHHHIIIhH", d[:40])
T = 1 << tlog
sx = W / (ntx * T)
H = int(nty * T * sx)
img = Image.new("RGB", (W, H), (60, 150, 60))
dr = ImageDraw.Draw(img)
P = lambda x, y: ((x - ox) * sx, H - (y - oy) * sx)
cols = {0: (0, 0, 0), 1: (220, 40, 40), 2: (40, 200, 60), 3: (255, 255, 255)}
mont = []
for t in range(ntx * nty):
    off, size, el = struct.unpack(">IHh", d[otd + t * 8: otd + t * 8 + 8])
    if not size:
        continue
    p = off
    n = struct.unpack(">H", d[p:p + 2])[0]
    p += 2
    for _ in range(n):
        typ, fl, lod, vsh, ox_, oy_, oz, r, nv, nf, fb = struct.unpack(">BBBBiihHBBH", d[p:p + 20])
        p += 22
        vs = [struct.unpack(">hhh", d[p + 6 * i:p + 6 * i + 6]) for i in range(nv)]
        p += nv * 6
        fbytes = d[p:p + fb]
        p += (fb + 1) & ~1
        V = [(ox_ + (v[0] << vsh) / 8, oy_ + (v[1] << vsh) / 8) for v in vs]
        q = 0
        for _ in range(nf):
            k, a = fbytes[q], fbytes[q + 1]
            idx = list(fbytes[q + 2:q + 2 + k])
            q += 2 + k
            if typ == 3:                      # luces: las filas (OF_ROWS) llevan el numero de luces como tercer indice
                continue
            pts = [P(*V[i]) for i in idx]
            if typ == 0 and fl & 2:
                dr.polygon(pts, fill=(40, 90, 200))
            elif typ == 1 and len(pts) > 1:
                dr.line(pts, fill=(250, 250, 250) if a == 3 else (40, 40, 40), width=1)
            elif typ == 2 and fl & 16:
                mont.append((ox_, oy_, r))
for x, y, r in mont:
    cx, cy = P(x, y)
    rr = r * sx
    dr.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), outline=(120, 70, 30))
for i in range(nnav):
    typ, _, fq, x, y = struct.unpack(">BBHii", d[onav + i * 24: onav + i * 24 + 12])
    ident = d[onav + i * 24 + 20: onav + i * 24 + 24].decode().strip()
    cx, cy = P(x, y)
    if typ in (1, 2):
        dr.rectangle((cx - 3, cy - 3, cx + 3, cy + 3), outline=(0, 0, 0))
        dr.text((cx + 4, cy - 4), ident, fill=(0, 0, 0))
    elif typ == 3:
        dr.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), outline=(150, 0, 150))
        dr.text((cx + 4, cy - 4), ident, fill=(150, 0, 150))
for i in range(napt):
    b = d[oapt + i * 120: oapt + i * 120 + 120]
    ident = b[:4].decode()
    x, y = struct.unpack(">ii", b[32:40])
    cx, cy = P(x, y)
    dr.ellipse((cx - 5, cy - 5, cx + 5, cy + 5), fill=(255, 220, 0), outline=(0, 0, 0))
    dr.text((cx + 6, cy + 2), ident, fill=(255, 255, 0))
img.save(sys.argv[2])
print("mapa", W, "x", H)
