#!/usr/bin/env python3
"""Resultado de prof.c con -DPROF_SCHED_RUN (simulador entero con el planificador): sched_parse.py perfil.txt"""
import sys
m = [(int(l.split()[1]), int(l.split()[2])) for l in open(sys.argv[1]) if l.startswith('MARK')]
st = {a: c for a, c in m if a in (240, 241)}
r = {}; i = 0
while i < len(m):
    if m[i][0] == 250 and i + 6 < len(m) + 0:
        v = 0
        for j in range(3, 7): v = v * 256 + m[i + j][0]
        r[m[i + 1][0]] = v; i += 7
    else: i += 1
sec = (st[241] - st[240]) / 7.5e6; ticks = r[203]; t = ticks / 50.0
print("duracion %d turnos = %.2f s de reloj (%.2f s de CPU emulada, relacion %.3f)" % (ticks, t, sec, t / sec))
print("fisica %.2f pasos/s (reloj) %.2f (real) | vistas completas %.2f/s | teclado %.2f/s" % (r[204] / t, r[204] / sec, r[205] / t, r[206] / t))
if 207 in r:
    print("panel %.2f vueltas/s | Y (nav+atc) %.2f/s | E (motores) %.2f/s | tareas K/E/Y recuperadas %d | turnos saltados %d (%.1f %%)" % (r[207] / t, r[208] / t, r[209] / t, r[210], r[211], 100.0 * r[211] / ticks))
    if 212 in r: print("vueltas de panel empezadas con holgura (fuera de Q): %.2f/s" % (r[212] / t))
