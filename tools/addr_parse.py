#!/usr/bin/env python3
"""Direcciones que da prof.c con -DPROF_ADDR: addr_parse.py perfil.txt"""
import sys
m = [int(l.split()[1]) for l in open(sys.argv[1]) if l.startswith('MARK')]
names = {1: 'codigo (prof_run)', 2: 'datos globales', 3: 'pila (variable local)', 4: 'malloc(1000)', 5: 'malloc(20000)', 6: 'plat_alloc_hi(1000)',
         7: 'plat_alloc_hi(70000)', 8: 'estructura sim', 9: 'tabla de rellenos', 10: 'RAM interna tapada (bytes)'}
i = 0
while i + 6 < len(m):
    if m[i] == 250:
        a = 0
        for j in range(3, 7): a = a * 256 + m[i + j]
        print("%-24s $%06X  %s" % (names.get(m[i + 1], m[i + 1]), a, "CONTENIDA (RAM interna $20000-$3FFFF)" if 0x20000 <= a < 0x40000 else "libre"))
        i += 7
    else: i += 1
