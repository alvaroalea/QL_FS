#!/usr/bin/env python3
"""Resume los .parsed de tools/bench_sched.sh en una tabla: bench_table.py <dir> [id...]  (b0 = CPU libre de referencia)"""
import sys, re, os
d = sys.argv[1]; ids = sys.argv[2:] or sorted(f[:-7] for f in os.listdir(d) if f.endswith('.parsed') and f != 'b0.parsed')
def num(t, k): m = re.search(k, t); return int(m.group(1)) if m else 0
t0 = open(d + '/b0.parsed').read()
base = num(t0, r'idle=(\d+)') / float(re.search(r'\((\d+\.\d+) s', t0).group(1))
print("%-4s | %5s %5s | %7s %6s | %6s | %6s | %6s | tareas: ejecutadas/s, turnos saltados, veces >1 turno" % ("id", "CPU", "t/seg", "F real", "F/turno", "K/s", "panel", "vista"))
for i in ids:
    t = open(os.path.join(d, i + '.parsed')).read()
    m = re.search(r'\((\d+\.\d+) s de CPU emulada; relacion ([\d.]+)', t)
    if not m: print(i, "sin datos"); continue
    sec = float(m.group(1)); rel = float(m.group(2))
    idle = num(t, r'idle=(\d+)'); v = num(t, r'vista=(\d+)'); p = num(t, r'panel=(\d+)'); rows = num(t, r'teclado=(\d+)'); ticks = num(t, r'(\d+) turnos')
    tasks = re.findall(r'tarea (\w): ejecutada\s+(\d+) \(([\d.]+)/s\)\s+saltada\s+(\d+)\s+tarda mas de un turno\s+(\d+) veces', t)
    td = {c: (int(e), float(r), int(s), int(o)) for c, e, r, s, o in tasks}
    F = td.get('F', (0, 0, 0, 0))
    print("%-4s | %4.0f%% %5.3f | %7.2f %6.2f | %6.2f | %6.2f | %6.2f | %s" % (i, 100 * (1 - idle / sec / base), rel, F[0] / sec, F[1], rows / sec, p / sec, v / sec,
          "  ".join("%s %.1f/%d/%d" % (c, a[0] / sec, a[2], a[3]) for c, a in sorted(td.items()) if c != 'F')))
