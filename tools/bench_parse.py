#!/usr/bin/env python3
"""Analiza el perfil de sQLux de src/bench.c (marcas, ver doc/interrupciones.md). Uso: bench_parse.py perfil.txt
Modo 1: pares de marcas (id par = inicio, id+1 = fin) -> ms por llamada.  Modo 10: informe del planificador."""
import sys
HZ = 7.5e6
marks = []
for l in open(sys.argv[1]):
    p = l.split()
    if p and p[0] == 'MARK': marks.append((int(p[1]), int(p[2])))
names = {10:'fisica (controls_step+flight_step)', 12:'nav_update+atc_update', 14:'nav_update', 16:'teclado: 1 fila (scan_row)',
         18:'teclado: read_keys(0) = 3 filas', 20:'teclado: read_keys(1) = 8 filas', 40:'controls_frame', 42:'snd_update', 44:'build_status',
         22:'panel 0 horizonte', 24:'panel 1 velocidad+altitud', 26:'panel 2 variometro+rumbo', 28:'panel 3 viraje+rpm+CDI',
         30:'panel 4 radios+motor', 32:'panel 5 mensajes+FPS', 36:'vista SOLIDA completa (sin volcado en bandas)', 38:'vista ALAMBRE completa (trozos sin parar)',
         46:'vista SOLIDA completa en trozos', 48:'flight_engines (motores)'}
# modo 1
st = {}; i = 0; rep = []; tk = {}; pu = []; sub = {}
while i < len(marks):
    lab, cyc = marks[i]
    if lab == 250 and i + 6 < len(marks):
        v = 0
        for j in range(3, 7): v = v * 256 + marks[i + j][0]
        rep.append((marks[i + 1][0], marks[i + 2][0], v)); i += 7; continue
    if lab in (240, 241): st[lab] = cyc
    if 80 <= lab < 100 and i + 1 < len(marks) and marks[i + 1][0] == lab:
        sub.setdefault(lab, []).append(marks[i + 1][1] - cyc); i += 2; continue
    if lab == 70 and i + 1 < len(marks) and marks[i + 1][0] == 70:
        pu.append(marks[i + 1][1] - cyc); i += 2; continue
    if (lab == 60 or 128 <= lab < 240) and i + 1 < len(marks) and not (lab == 250):
        j = i + 1                                   # la pareja puede llegar despues de otras marcas (la interrupcion se intercala)
        while j < len(marks) and marks[j][0] != lab: j += 1
        if j < len(marks):
            tk.setdefault(lab, []).append(marks[j][1] - cyc); i += 1
            marks[j] = (-1, marks[j][1]); continue
    if lab % 2 == 0 and lab < 100 and i + 1 < len(marks) and marks[i + 1][0] == lab + 1:
        st.setdefault(lab, []).append(marks[i + 1][1] - cyc); i += 2; continue
    if 100 <= lab < 110 and i + 1 < len(marks) and marks[i + 1][0] == 110:
        st.setdefault(('u', lab), []).append(marks[i + 1][1] - cyc); i += 2; continue
    i += 1
for k in sorted(k for k in st if isinstance(k, int) and k < 100):
    v = st[k]
    print("%-48s n=%2d  media %8.2f ms  min %8.2f  max %8.2f   (%d ciclos, %.2f turnos de 20 ms)" % (
        names.get(k, str(k)), len(v), sum(v) / len(v) / HZ * 1e3, min(v) / HZ * 1e3, max(v) / HZ * 1e3, sum(v) / len(v), sum(v) / len(v) / HZ * 50))
for k in sorted((k for k in st if isinstance(k, tuple))):
    v = st[k]; nm = ['camara+fondo+lista de teselas', 'reunir 1 tesela', 'ordenar 4 elementos', 'dibujar 1 objeto', 'modelo exterior', 'volcado 16 filas'][k[1] - 100] if k[1] - 100 < 6 else str(k)
    v.sort()
    print("unidad %-34s n=%4d  media %7.2f ms  p90 %7.2f  max %7.2f  suma %8.1f ms" % (nm, len(v), sum(v) / len(v) / HZ * 1e3, v[int(len(v) * .9)] / HZ * 1e3, v[-1] / HZ * 1e3, sum(v) / HZ * 1e3))
for lab in sorted(sub):
    nm = {80: 'AI ai_begin', 81: 'AI 53 filas completas', 82: 'AI avion de referencia', 83: 'AI aro (2 elipses)', 84: 'rosa: copia del fondo', 85: 'rosa: 12 rayitas largas',
          86: 'rosa: 12 numeros', 87: 'rosa: rumbo en el centro', 88: 'ROSA ENTERA (hdg_paint)', 95: '18 gfx_glyph de 2 grupos (asm solo)', 96: '18 gfx_glyph de 1 grupo (asm solo)', 89: 'AI: +3 grados de alabeo (filas+avion)', 90: 'copiar 56x24 B hi -> pantalla', 91: 'copiar 56x24 B hi -> hi', 92: 'copiar 56x24 B pantalla -> hi', 93: 'texto 3 car. en RAM alta', 94: 'texto 3 car. en pantalla'}.get(lab, str(lab))
    v = sub[lab]; print("%-30s media %7.2f ms  max %7.2f  (n=%d)" % (nm, sum(v) / len(v) / HZ * 1e3, max(v) / HZ * 1e3, len(v)))
if pu:
    n = len(pu) // 12
    nm = ['AI: decide', 'AI banda 1', 'AI banda 2', 'AI banda 3', 'AI banda 4', 'AI banda 5', 'AI banda 6', 'AI banda 7', 'AI banda 8', 'AI banda 9', 'AI: avion y aro',
          'velocidad', 'altitud', 'variometro', 'rumbo', 'viraje', 'rpm', 'CDI', 'radios', 'motor', 'mensajes+FPS']
    tot = 0
    for k in range(n):
        v = [pu[c * n + k] for c in range(12)]
        tot += sum(v) / len(v)
        print("panel unidad %2d %-16s media %7.2f ms  max %7.2f" % (k, nm[k] if k < len(nm) else '?', sum(v) / len(v) / HZ * 1e3, max(v) / HZ * 1e3))
    print("panel: vuelta completa (todo cambiando) %.1f ms en %d unidades" % (tot / HZ * 1e3, n))
for lab in sorted(tk):
    v = sorted(tk[lab]); nm = 'F en la interrupcion' if lab == 60 else 'tarea %s (tiempo de reloj en su turno, con la interrupcion dentro)' % chr(lab - 128)
    print("%-62s n=%4d  media %7.2f ms  p90 %7.2f  max %7.2f" % (nm, len(v), sum(v) / len(v) / HZ * 1e3, v[int(len(v) * .9)] / HZ * 1e3, v[-1] / HZ * 1e3))
if any(a == 203 for a, _, _ in rep):
    sec = (st[241] - st[240]) / HZ if 240 in st and 241 in st else 0
    r = {}
    for idn, what, v in rep: r[(idn, what)] = v
    ticks = r.get((203, 0), 0)
    print("duracion: %d turnos (%.2f s de CPU emulada; relacion %.3f)" % (ticks, sec, ticks / 50.0 / sec if sec else 0))
    print("idle=%d  cuadros de vista=%d  cuadros de panel=%d  filas de teclado=%d" % (r.get((200, 0), 0), r.get((201, 0), 0), r.get((202, 0), 0), r.get((204, 0), 0)))
    if (205, 0) in r: print("pasos de fisica a toda marcha: %d en %.2f s = %.2f pasos/s" % (r[(205, 0)], ticks / 50.0, r[(205, 0)] / (ticks / 50.0)))
    if (204, 0) in r and (205, 0) in r and not (200, 0) in r:
        pass
    t = ticks / 50.0 or 1
    if (ticks and False): pass
    for ch in sorted(set(i for i, w in r if i < 128)):
        print("  tarea %s: ejecutada %5d (%.2f/s)  saltada %5d  tarda mas de un turno %4d veces, maximo %d turnos" % (
            chr(ch), r[(ch, 0)], r[(ch, 0)] / t, r[(ch, 1)], r[(ch, 3)], r[(ch, 2)]))
    print("RESULT idle=%d v=%d p=%d ticks=%d" % (r.get((200, 0), 0), r.get((201, 0), 0), r.get((202, 0), 0), ticks))
