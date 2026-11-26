#!/usr/bin/env python3
"""panel_anotado.py - captura del panel con los instrumentos numerados (figura 2 del libro, cabina del bimotor).
Uso: python3 tools/panel_anotado.py captura.png salida.png
Las posiciones salen de src/panel.c (COL1..COL4, ROW1, ROW2, bloque derecho) a la escala 2x3 de las capturas."""
import sys
from PIL import Image, ImageDraw, ImageFont

# (numero, x, y) sobre la captura de 1024x768
MARCAS = [
    (1, 35, 330), (2, 218, 330), (3, 402, 330), (4, 585, 330),
    (5, 35, 512), (6, 218, 512), (7, 402, 512), (8, 585, 512),
    (9, 738, 318), (10, 742, 498), (11, 992, 520), (12, 990, 697), (13, 990, 735),
    (14, 512, 40),
]

src = Image.open(sys.argv[1]).convert("RGB")
im = src.copy()
d = ImageDraw.Draw(im)
try:
    ft = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
except OSError:
    ft = ImageFont.load_default()
for n, x, y in MARCAS:
    r = 19
    d.ellipse((x - r, y - r, x + r, y + r), fill=(255, 210, 60), outline=(0, 0, 0), width=3)
    d.text((x, y), str(n), font=ft, fill=(0, 0, 0), anchor="mm")
im.save(sys.argv[2])
