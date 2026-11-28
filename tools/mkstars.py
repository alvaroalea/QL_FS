#!/usr/bin/env python3
"""Genera src/stars.h: catalogo de estrellas brillantes del cielo de verano (vectores ecuatoriales Q14 por grupos).
Uso: python3 tools/mkstars.py > src/stars.h
Datos aproximados (J2000): ascension recta en horas, declinacion en grados, magnitud."""
import math

# (grupo, nombre, RA h, Dec grados, magnitud, flags) flags: 1 = rojiza, 2 = se ve tambien al anochecer (Polar)
G = [
 ("UMi", [("Polaris", 2.530, 89.264, 2.0, 2), ("Yildun", 17.537, 86.586, 4.3, 0), ("Eps UMi", 16.766, 82.037, 4.2, 0),
          ("Zeta UMi", 15.734, 77.795, 4.3, 0), ("Eta UMi", 16.292, 75.755, 5.0, 0), ("Kochab", 14.845, 74.156, 2.1, 0),
          ("Pherkad", 15.345, 71.834, 3.0, 0)]),
 ("UMa", [("Dubhe", 11.062, 61.751, 1.8, 0), ("Merak", 11.031, 56.382, 2.4, 0), ("Phecda", 11.897, 53.695, 2.4, 0),
          ("Megrez", 12.257, 57.033, 3.3, 0), ("Alioth", 12.900, 55.960, 1.8, 0), ("Mizar", 13.399, 54.925, 2.2, 0),
          ("Alkaid", 13.792, 49.313, 1.9, 0)]),
 ("Dra", [("Eltanin", 17.943, 51.489, 2.2, 0), ("Rastaban", 17.507, 52.301, 2.8, 0), ("Grumium", 17.892, 56.873, 3.7, 0),
          ("Altais", 19.209, 67.662, 3.1, 0), ("Aldhibah", 17.146, 65.715, 3.2, 0)]),
 ("Cas", [("Caph", 0.153, 59.150, 2.3, 0), ("Schedar", 0.675, 56.537, 2.2, 0), ("Gamma Cas", 0.945, 60.717, 2.5, 0),
          ("Ruchbah", 1.430, 60.235, 2.7, 0), ("Segin", 1.907, 63.670, 3.4, 0)]),
 ("Cep", [("Alderamin", 21.310, 62.586, 2.5, 0), ("Errai", 23.655, 77.632, 3.2, 0), ("Beta Cep", 21.478, 70.561, 3.2, 0)]),
 ("Cyg", [("Deneb", 20.691, 45.280, 1.25, 0), ("Sadr", 20.370, 40.257, 2.2, 0), ("Gienah", 20.770, 33.970, 2.5, 0),
          ("Delta Cyg", 19.750, 45.131, 2.9, 0), ("Albireo", 19.512, 27.960, 3.1, 0)]),
 ("Lyr", [("Vega", 18.616, 38.784, 0.03, 0), ("Sheliak", 18.835, 33.363, 3.5, 0), ("Sulafat", 18.982, 32.689, 3.2, 0)]),
 ("Aql", [("Altair", 19.846, 8.868, 0.77, 0), ("Tarazed", 19.771, 10.613, 2.7, 0), ("Alshain", 19.922, 6.407, 3.7, 0),
          ("Delta Aql", 19.425, 3.115, 3.4, 0)]),
 ("Boo", [("Arcturus", 14.261, 19.182, -0.05, 0), ("Izar", 14.750, 27.074, 2.4, 0), ("Seginus", 14.535, 38.308, 3.0, 0),
          ("Nekkar", 15.258, 40.391, 3.5, 0), ("Muphrid", 13.911, 18.398, 2.7, 0)]),
 ("CrB", [("Alphecca", 15.578, 26.715, 2.2, 0), ("Beta CrB", 15.464, 29.106, 3.7, 0), ("Gamma CrB", 15.713, 26.296, 3.8, 0)]),
 ("Her", [("Kornephoros", 16.503, 21.490, 2.8, 0), ("Zeta Her", 16.688, 31.603, 2.8, 0), ("Eta Her", 16.715, 38.922, 3.5, 0),
          ("Pi Her", 17.251, 36.809, 3.2, 0), ("Eps Her", 17.005, 30.926, 3.9, 0), ("Rasalgethi", 17.244, 14.390, 3.0, 0),
          ("Sarin", 17.251, 24.840, 3.1, 0)]),
 ("Oph", [("Rasalhague", 17.582, 12.560, 2.1, 0), ("Sabik", 17.173, -15.725, 2.4, 0), ("Cebalrai", 17.724, 4.567, 2.8, 0),
          ("Yed Prior", 16.239, -3.694, 2.7, 0)]),
 ("Sco", [("Graffias", 16.090, -19.805, 2.6, 0), ("Dschubba", 16.006, -22.622, 2.3, 0), ("Pi Sco", 15.981, -26.114, 2.9, 0),
          ("Antares", 16.490, -26.432, 1.0, 1), ("Sigma Sco", 16.353, -25.593, 2.9, 0), ("Tau Sco", 16.598, -28.216, 2.8, 0),
          ("Eps Sco", 16.836, -34.293, 2.3, 0), ("Mu Sco", 16.864, -38.047, 3.0, 0), ("Zeta Sco", 16.910, -42.362, 3.6, 0),
          ("Lesath", 17.513, -37.296, 2.7, 0), ("Shaula", 17.560, -37.104, 1.6, 0), ("Kappa Sco", 17.708, -39.030, 2.4, 0),
          ("Sargas", 17.622, -42.998, 1.9, 0)]),
 ("Sgr", [("Alnasl", 18.097, -30.424, 3.0, 0), ("Kaus Media", 18.350, -29.828, 2.7, 0), ("Kaus Australis", 18.403, -34.385, 1.85, 0),
          ("Kaus Borealis", 18.466, -25.422, 2.8, 0), ("Phi Sgr", 18.761, -26.991, 3.2, 0), ("Nunki", 18.921, -26.297, 2.0, 0),
          ("Ascella", 19.043, -29.880, 2.6, 0), ("Tau Sgr", 19.116, -27.670, 3.3, 0)]),
 ("VirLib", [("Spica", 13.420, -11.161, 1.0, 0), ("Vindemiatrix", 13.036, 10.959, 2.8, 0), ("Porrima", 12.694, -1.449, 2.7, 0),
             ("Zubenelgenubi", 14.848, -16.042, 2.7, 0), ("Zubeneschamali", 15.283, -9.383, 2.6, 0)]),
 ("Leo", [("Regulus", 10.139, 11.967, 1.35, 0), ("Denebola", 11.818, 14.572, 2.1, 0), ("Algieba", 10.333, 19.842, 2.0, 0),
          ("Zosma", 11.235, 20.524, 2.6, 0), ("Eta Leo", 10.122, 16.763, 3.5, 0)]),
 ("PegAnd", [("Markab", 23.079, 15.205, 2.5, 0), ("Scheat", 23.063, 28.083, 2.4, 0), ("Algenib", 0.220, 15.184, 2.8, 0),
             ("Alpheratz", 0.139, 29.091, 2.1, 0), ("Mirach", 1.162, 35.621, 2.1, 0), ("Almach", 2.065, 42.330, 2.1, 0),
             ("Enif", 21.736, 9.875, 2.4, 0), ("Fomalhaut", 22.961, -29.622, 1.16, 0)]),
 ("PerAur", [("Capella", 5.278, 45.998, 0.08, 0), ("Menkalinan", 5.992, 44.947, 1.9, 0), ("Mirfak", 3.405, 49.861, 1.8, 0),
             ("Aldebaran", 4.599, 16.509, 0.85, 1), ("Pleiades", 3.790, 24.105, 1.6, 0), ("Hamal", 2.120, 23.462, 2.0, 0)]),
 ("DelCap", [("Sualocin", 20.660, 15.912, 3.8, 0), ("Rotanev", 20.626, 14.595, 3.6, 0), ("Delta Del", 20.724, 15.075, 4.4, 0),
             ("Gamma Del", 20.777, 16.124, 3.9, 0), ("Eps Del", 20.554, 11.303, 4.0, 0),
             ("Dabih", 20.350, -14.781, 3.1, 0), ("Deneb Algedi", 21.784, -16.127, 2.9, 0)]),
]

def vec(ra, dec):
    a = ra * math.pi / 12; d = math.radians(dec)
    return (math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d))

def q(v): return int(round(v * 16384))

stars = []; groups = []
for gname, lst in G:
    vs = [vec(r, d) for (_, r, d, _, _) in [(n, r, d, m, f) for (n, r, d, m, f) in lst]]
    c = [sum(v[i] for v in vs) for i in range(3)]
    n = math.sqrt(sum(x * x for x in c)); c = [x / n for x in c]
    rad = max(math.degrees(math.acos(min(1, sum(c[i] * v[i] for i in range(3))))) for v in vs)
    groups.append((gname, len(stars), len(lst), c, rad, min(m for (_, _, _, m, _) in lst)))
    for (nm, r, d, m, f), v in zip(lst, vs):
        stars.append((nm, v, m, f))

print("/* stars.h - generado por tools/mkstars.py (no editar a mano): %d estrellas en %d grupos */" % (len(stars), len(groups)))
print("#define NSTARS %d" % len(stars))
print("#define NSGROUPS %d" % len(groups))
print("/* vectores ecuatoriales Q14 (x hacia RA 0h, y hacia RA 6h, z al polo norte celeste) */")
print("static const s16 star_v[NSTARS * 3] = {")
for nm, v, m, f in stars:
    print("    %d, %d, %d,   /* %s */" % (q(v[0]), q(v[1]), q(v[2]), nm))
print("};")
print("/* (magnitud + 1) * 10 */")
print("static const u8 star_m[NSTARS] = {")
print("    " + ", ".join(str(int(round((m + 1) * 10))) for _, _, m, _ in stars))
print("};")
print("/* 1 = rojiza, 2 = visible tambien al anochecer (la Polar) */")
print("static const u8 star_f[NSTARS] = {")
print("    " + ", ".join(str(f) for _, _, _, f in stars))
print("};")
print("/* grupos contiguos: primera, cuantas, centro Q14, radio (BAM16), magnitud mas brillante */")
print("typedef struct { s16 first, n, c[3]; u16 rad; u8 mmin, pad; } StarGroup;")
print("static const StarGroup star_grp[NSGROUPS] = {")
for gname, first, n, c, rad, mm in groups:
    print("    { %d, %d, { %d, %d, %d }, %d, %d, 0 },   /* %s */" % (first, n, q(c[0]), q(c[1]), q(c[2]), int(math.ceil(rad * 65536 / 360)) + 1, int(round((mm + 1) * 10)), gname))
print("};")
