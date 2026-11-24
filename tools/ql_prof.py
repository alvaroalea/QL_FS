#!/usr/bin/env python3
"""Informe del perfil del sQLux de pruebas (tools/ql_prof.sh): ciclos del 68008 por funcion entre cada dos PROF_DUMP.
Uso: ql_prof.py perfil.txt programa.MAP [n funciones por volcado]"""
import sys, bisect
prof, mapf = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
syms = []
for l in open(mapf, errors='replace'):
    p = l.split()
    if len(p) >= 3 and p[2] == 'TEXT' and p[0].startswith('_'):
        try: syms.append((int(p[1].rstrip('*'), 16), p[0]))
        except ValueError: pass
syms.sort()
addrs = [a for a, _ in syms]
main = dict((n, a) for a, n in syms).get('_MAIN', 0)
textend = 0
mods = []                                   # (inicio, nombre) de cada modulo, en orden de enlace
for l in open(mapf, errors='replace'):
    if 'TEXT=' in l:
        try:
            mods.append((textend, l.split(':')[0].strip()))
            textend += int(l.split('TEXT=')[1].split()[0], 16)
        except ValueError: pass
maddrs = [a for a, _ in mods]
cur = None; marks = []
def report(tag, base, total, hist):
    off = base - main
    by = {}; bm = {}
    for a, c in hist:
        r = a - off
        if r < 0 or r >= textend: n = mn = 'ROM/QDOS' if a < 0x20000 or a >= 0xC0000 else 'otro'
        else:
            i = bisect.bisect_right(addrs, r) - 1
            n = syms[i][1] if i >= 0 else '?'
            mn = mods[bisect.bisect_right(maddrs, r) - 1][1]
        by[n] = by.get(n, 0) + c
        bm[mn] = bm.get(mn, 0) + c
    tot = sum(by.values()) or 1
    print("== volcado %d: %.2f s (%d ciclos)" % (tag, total / 7.5e6, total))
    for n, c in sorted(by.items(), key=lambda x: -x[1])[:top]:
        print("  %7.3f s %5.1f %%  %s" % (c / 7.5e6, 100.0 * c / tot, n))
    print("  modulos: " + ", ".join("%s %.2f" % (n, c / 7.5e6) for n, c in sorted(bm.items(), key=lambda x: -x[1])[:10]))
last = None
for l in open(prof):
    p = l.split()
    if p[0] == 'MARK':
        t = int(p[2]); print("-- marca %s: %.2f s%s" % (p[1], t / 7.5e6, "" if last is None else " (+%.2f s)" % ((t - last) / 7.5e6))); last = t
    elif p[0] == 'DUMP': cur = (int(p[1]), int(p[2]), int(p[3]), [])
    elif p[0] == 'END': report(*cur); cur = None
    elif cur is not None: cur[3].append((int(p[0]), int(p[1])))
