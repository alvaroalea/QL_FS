#!/usr/bin/env python3
"""qlscr2png.py - convierte un volcado de pantalla del QL (32768 bytes, modo 4)
a PNG, escalado x2 en horizontal y x3 en vertical (aspecto 4:3 del monitor).
Uso: qlscr2png.py entrada.scr salida.png [escala]"""
import sys
from PIL import Image

PAL = [(0, 0, 0), (230, 30, 30), (40, 200, 60), (245, 245, 245)]


def load(fn):
    d = open(fn, "rb").read()
    img = Image.new("RGB", (512, 256))
    px = img.load()
    for y in range(256):
        for xb in range(64):
            g = d[y * 128 + xb * 2]
            r = d[y * 128 + xb * 2 + 1]
            for b in range(8):
                c = ((g >> (7 - b)) & 1) << 1 | ((r >> (7 - b)) & 1)
                px[xb * 8 + b, y] = PAL[c]
    return img


if __name__ == "__main__":
    k = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    img = load(sys.argv[1]).resize((1024 * k, 768 * k), Image.NEAREST)
    img.save(sys.argv[2])
