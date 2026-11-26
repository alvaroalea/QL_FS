; ql68k.s - rutinas criticas de QFS en ensamblador 68000 (sintaxis as68 / XTC68)
;
; Convenio c68: argumentos en la pila (4 bytes cada uno, el primero en 4(a7)),
; resultado en d0. Aqui se guardan todos los registros que se usan salvo
; d0/d1/a0/a1, asi que valen con cualquier convenio de c68.
;
; Rutinas:
;   _fxmul          (a*b)>>16 con signo, 32x32 (4 MULU)
;   _gfx_hspan_raw  relleno de un tramo horizontal en modo 4 con patron
;   _gfx_line_raw   Bresenham en modo 4 (ya recortada)
;   _gfx_memcpy     copia con MOVEM de 48 bytes (volcado del buffer 3D)
;   _xform3         matriz 3x3 Q14 por n vertices s16 -> s32
;   _mdq            a*b/d con muls/divs (proyeccion y recorte), MDQ_BIG si no cabe
;   (las llamadas a QDOS estan en qdos68k.s)

        .text

        .globl  _fxmul
        .globl  _fxdiv
        .globl  _gfx_hspan_raw
        .globl  _gfx_line_raw
        .globl  _gfx_memcpy
        .globl  _xform3
        .globl  _mdq

; ---------------------------------------------------------------- fxmul
; s32 fxmul(s32 a, s32 b)
_fxmul:
        movem.l d2-d6,-(a7)             ; 20 bytes
        move.l  24(a7),d0               ; a
        move.l  28(a7),d1               ; b
        moveq   #0,d4                   ; signo
        tst.l   d0
        bpl     fm_1
        neg.l   d0
        not.w   d4
fm_1:   tst.l   d1
        bpl     fm_2
        neg.l   d1
        not.w   d4
fm_2:   move.l  d1,d3
        swap    d3                      ; d3.w = bh
        tst.w   d3
        beq     fm_f                    ; b cabe en 16 bits (casi siempre: constantes < 1.0, razones...)
        move.l  d0,d2
        swap    d2                      ; d2.w = ah
        tst.w   d2
        bne     fm_full
        exg     d0,d1                   ; a cabe en 16 bits: se cambian y vale la via corta
fm_f:   move.l  d0,d2                   ; via corta (bh = 0): (a*b)>>16 = ah*bl + ((al*bl)>>16): dos MULU en vez de cuatro
        swap    d2
        mulu    d1,d2                   ; ah*bl
        mulu    d1,d0                   ; al*bl
        clr.w   d0
        swap    d0
        add.l   d2,d0
        tst.w   d4
        beq     fm_3
        neg.l   d0
        bra     fm_3
fm_full:
        move.l  d1,d3
        swap    d3                      ; d3.w = bh
        move.w  d0,d5
        mulu    d1,d5                   ; al*bl
        clr.w   d5
        swap    d5                      ; (al*bl)>>16
        move.w  d2,d6
        mulu    d1,d6                   ; ah*bl
        add.l   d6,d5
        move.w  d0,d6
        mulu    d3,d6                   ; al*bh
        add.l   d6,d5
        mulu    d3,d2                   ; ah*bh
        swap    d2
        clr.w   d2                      ; <<16
        add.l   d2,d5
        move.l  d5,d0
        tst.w   d4
        beq     fm_3
        neg.l   d0
fm_3:   movem.l (a7)+,d2-d6
        rts

; ---------------------------------------------------------------- fxdiv
; s32 fxdiv(s32 a, s32 b): (a<<16)/b, EXACTAMENTE el mismo resultado que la version en C de fixmath.c:
; |a| se normaliza hacia arriba hasta >= 2^29 (como mucho 16 bits), |b| se desplaza lo que falte y se divide |a|/|b|.
; Normalizacion binaria (16 / 8 / 4 / 2 / 1) en vez de bit a bit; la division 32/32 sin signo se hace con DIVU de 32/16:
;   |b| < 2^16: dos DIVU encadenadas (exacta); |b| >= 2^16: el cociente cabe en 15 bits, se estima con a y b desplazados
;   a la vez y se corrige (+-1) comparando q*b con a.
_fxdiv:
        movem.l d2-d5,-(a7)             ; 16 bytes
        move.l  20(a7),d0               ; a
        move.l  24(a7),d1               ; b
        moveq   #0,d4                   ; signo
        tst.l   d0
        bpl     fd_1
        neg.l   d0
        not.w   d4
fd_1:   tst.l   d1
        bpl     fd_2
        neg.l   d1
        not.w   d4
fd_2:   moveq   #16,d2                  ; s
        cmp.l   #$4000,d0
        bcc     fd_n1
        swap    d0                      ; a < 2^14: a<<16 (s = 0)
        moveq   #0,d2
        bra     fd_dv
fd_n1:  cmp.l   #$400000,d0
        bcc     fd_n2
        lsl.l   #8,d0
        subq.w  #8,d2
fd_n2:  cmp.l   #$4000000,d0
        bcc     fd_n3
        lsl.l   #4,d0
        subq.w  #4,d2
fd_n3:  cmp.l   #$10000000,d0
        bcc     fd_n4
        lsl.l   #2,d0
        subq.w  #2,d2
fd_n4:  cmp.l   #$20000000,d0
        bcc     fd_dv
        add.l   d0,d0
        subq.w  #1,d2
fd_dv:  lsr.l   d2,d1                   ; b >>= s
        beq     fd_sat
        cmp.l   #$FFFF,d1
        bhi     fd_big
        move.l  d0,d2                   ; |b| < 2^16: a/b = (q1<<16) + q2
        clr.w   d2
        swap    d2                      ; parte alta de a
        divu    d1,d2                   ; r1:q1
        move.w  d2,d3                   ; q1
        clr.w   d2
        move.w  d0,d2                   ; r1:parte baja de a
        divu    d1,d2                   ; r2:q2
        swap    d3
        move.w  d2,d3
        move.l  d3,d0
        bra     fd_sg
fd_big: move.l  d0,d2                   ; |b| >= 2^16: estimacion con a y b desplazados a la vez
        move.l  d1,d3
fd_b1:  lsr.l   #1,d2
        lsr.l   #1,d3
        cmp.l   #$FFFF,d3
        bhi     fd_b1
        divu    d3,d2                   ; q' (<2^15)
        and.l   #$FFFF,d2
        move.l  d1,d3                   ; p = q'*b (modulo 2^32; el valor real cabe)
        swap    d3
        mulu    d2,d3
        swap    d3
        clr.w   d3
        move.w  d1,d5
        mulu    d2,d5
        add.l   d5,d3
fd_c1:  cmp.l   d0,d3                   ; mientras p > a: q'--
        bls     fd_c2
        subq.l  #1,d2
        sub.l   d1,d3
        bra     fd_c1
fd_c2:  move.l  d0,d5                   ; mientras a-p >= b: q'++
        sub.l   d3,d5
        cmp.l   d1,d5
        bcs     fd_c3
        addq.l  #1,d2
        add.l   d1,d3
        bra     fd_c2
fd_c3:  move.l  d2,d0
        bra     fd_sg
fd_sat: move.l  #$7FFFFFFF,d0
fd_sg:  tst.w   d4
        beq     fd_x
        neg.l   d0
fd_x:   movem.l (a7)+,d2-d5
        rts

; ---------------------------------------------------------------- hspan
; void gfx_hspan_raw(u8 *row, s32 x0, s32 x1, s32 pat)
; pat = palabra G<<8 | R (8 pixeles); x0 <= x1, ya recortados
_gfx_hspan_raw:
        move.l  a5,-(a7)
        move.l  8(a7),a0                ; fila
        move.l  12(a7),d0               ; x0
        move.l  16(a7),d1               ; x1
        move.l  20(a7),d2               ; patron
        lea     fc_lm,a5
        bsr     _fc_hspan
        move.l  (a7)+,a5
        rts

; void gfx_hspan(Surface *s, int y, int x0, int x1, Fill f): tramo recortado a la superficie (el patron de la fila par es la palabra
; alta de f y el de la impar la baja)
        .globl  _gfx_hspan
_gfx_hspan:
        movem.l d2-d3/a5,-(a7)          ; 12 bytes: s 16, y 20, x0 24, x1 28, f 32
        move.l  16(a7),a0
        move.l  20(a7),d0
        move.w  8(a0),d1
        ext.l   d1
        cmp.l   d1,d0
        blt     gh_ret                  ; y < cy0
        move.w  12(a0),d1
        ext.l   d1
        cmp.l   d1,d0
        bgt     gh_ret                  ; y > cy1
        move.l  24(a7),d1               ; x0
        move.l  28(a7),d2               ; x1
        cmp.l   d1,d2
        bge     gh_1
        exg     d1,d2
gh_1:   move.w  6(a0),d3
        ext.l   d3
        cmp.l   d3,d1
        bge     gh_2
        move.l  d3,d1                   ; x0 = cx0
gh_2:   move.w  10(a0),d3
        ext.l   d3
        cmp.l   d3,d2
        ble     gh_3
        move.l  d3,d2                   ; x1 = cx1
gh_3:   cmp.l   d1,d2
        blt     gh_ret
        move.l  32(a7),d3               ; f
        btst    #0,d0
        bne     gh_4
        swap    d3                      ; fila par: palabra alta
gh_4:   lsl.l   #7,d0                   ; y*128
        movea.l (a0),a1
        adda.l  d0,a1                   ; fila
        move.l  d1,d0                   ; x0
        move.l  d2,d1                   ; x1
        move.l  d3,d2                   ; patron (palabra baja)
        move.l  a1,a0
        lea     fc_lm,a5
        bsr     _fc_hspan
gh_ret: movem.l (a7)+,d2-d3/a5
        rts

; ---------------------------------------------------------------- linea
; void gfx_line_raw(u8 *base, s32 x0, s32 y0, s32 x1, s32 y1, s32 c)
; registros: d0 bit, d1 err, d2 dx, d3 -dy, d4 e2/temp, d5 contador,
;            d6 color, d7 sentido x (0 dcha), a0 puntero, a2 paso y
_gfx_line_raw:
        movem.l d2-d7/a2,-(a7)          ; 28 bytes
        move.l  32(a7),a0
        move.l  36(a7),d0               ; x0
        move.l  40(a7),d1               ; y0
        move.l  44(a7),d2               ; x1
        move.l  48(a7),d3               ; y1
        move.l  52(a7),d6               ; color
        move.l  d1,d4
        lsl.l   #7,d4                   ; y0*128
        adda.l  d4,a0
        move.l  d0,d4
        lsr.l   #3,d4
        add.l   d4,d4
        adda.l  d4,a0
        moveq   #0,d7
        sub.l   d0,d2                   ; dx
        bpl     ln_1
        neg.l   d2
        moveq   #1,d7
ln_1:   movea.l #128,a2
        sub.l   d1,d3                   ; dy
        bpl     ln_2
        neg.l   d3
        movea.l #-128,a2
ln_2:   moveq   #7,d4
        and.w   d0,d4
        move.w  #$0080,d0
        lsr.b   d4,d0                   ; bit del primer pixel
        move.l  d2,d5
        cmp.l   d3,d5
        bge     ln_3
        move.l  d3,d5                   ; n = max(dx,dy)
ln_3:   neg.l   d3                      ; d3 = -dy
        move.l  d2,d1
        add.l   d3,d1                   ; err = dx - dy
ln_loop:
        btst    #1,d6
        beq     ln_g0
        or.b    d0,(a0)
        bra     ln_g1
ln_g0:  move.b  d0,d4
        not.b   d4
        and.b   d4,(a0)
ln_g1:  btst    #0,d6
        beq     ln_r0
        or.b    d0,1(a0)
        bra     ln_r1
ln_r0:  move.b  d0,d4
        not.b   d4
        and.b   d4,1(a0)
ln_r1:  move.l  d1,d4
        add.l   d4,d4                   ; e2 = 2*err
        cmp.l   d3,d4
        ble     ln_nx                   ; e2 <= -dy: no avanza x
        add.l   d3,d1                   ; err -= dy
        tst.b   d7
        bne     ln_left
        lsr.b   #1,d0
        bne     ln_nx
        move.b  #$80,d0
        addq.l  #2,a0
        bra     ln_nx
ln_left:
        lsl.b   #1,d0
        bne     ln_nx
        moveq   #1,d0
        subq.l  #2,a0
ln_nx:  cmp.l   d2,d4
        bge     ln_ny                   ; e2 >= dx: no avanza y
        add.l   d2,d1                   ; err += dx
        adda.l  a2,a0
ln_ny:  dbra    d5,ln_loop
        movem.l (a7)+,d2-d7/a2
        rts

; ---------------------------------------------------------------- memcpy
; void gfx_memcpy(u8 *dst, const u8 *src, s32 n)
_gfx_memcpy:
        movem.l d2-d7/a2-a6,-(a7)       ; 44 bytes
        move.l  48(a7),a1               ; destino
        move.l  52(a7),a0               ; origen
        move.l  56(a7),d0               ; n
        move.l  a1,d1
        move.l  a0,d2
        or.l    d2,d1
        btst    #0,d1
        bne     mc_bytes                ; direcciones impares: byte a byte
mc_blk: cmp.l   #48,d0
        blt     mc_long
        movem.l (a0)+,d1-d7/a2-a6       ; 12 registros = 48 bytes
        movem.l d1-d7/a2-a6,(a1)
        lea     48(a1),a1
        sub.l   #48,d0
        bra     mc_blk
mc_long:
        cmp.l   #4,d0
        blt     mc_bytes
        move.l  (a0)+,(a1)+
        subq.l  #4,d0
        bra     mc_long
mc_bytes:
        tst.l   d0
        beq     mc_end
        move.b  (a0)+,(a1)+
        subq.l  #1,d0
        bra     mc_bytes
mc_end: movem.l (a7)+,d2-d7/a2-a6
        rts

; ---------------------------------------------------------------- xform3
; void xform3(const s16 *m, const s16 *in, s32 *out, int n)
_xform3:
        movem.l d2-d7/a2,-(a7)          ; 28 bytes
        move.l  32(a7),a2               ; matriz
        move.l  36(a7),a0               ; entrada
        move.l  40(a7),a1               ; salida
        move.l  44(a7),d7               ; n
        bra     xf_chk
xf_loop:
        move.w  (a0)+,d0
        move.w  (a0)+,d1
        move.w  (a0)+,d2
        move.w  (a2),d3
        muls    d0,d3
        move.w  2(a2),d4
        muls    d1,d4
        add.l   d4,d3
        move.w  4(a2),d4
        muls    d2,d4
        add.l   d4,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.l  d3,(a1)+
        move.w  6(a2),d3
        muls    d0,d3
        move.w  8(a2),d4
        muls    d1,d4
        add.l   d4,d3
        move.w  10(a2),d4
        muls    d2,d4
        add.l   d4,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.l  d3,(a1)+
        move.w  12(a2),d3
        muls    d0,d3
        move.w  14(a2),d4
        muls    d1,d4
        add.l   d4,d3
        move.w  16(a2),d4
        muls    d2,d4
        add.l   d4,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.l  d3,(a1)+
xf_chk: dbra    d7,xf_loop
        movem.l (a7)+,d2-d7/a2
        rts


; ---------------------------------------------------------------- mdq
; s32 mdq(s32 a, s32 b, s32 d): a*b/d (truncado hacia cero) por la via rapida del 68000, o $7FFFFFFF si no cabe.
;   - si d no cabe en 16 bits con signo se desplazan d y a a la vez (la razon se conserva)
;   - |a| y |b| deben caber en 16 bits con signo; el cociente tambien (DIVS.W marca V si desborda)
; Solo usa d0-d2: no hace falta salvar nada (c68 deja libres d0/d1/a0/a1; d2 se guarda aqui).
_mdq:
        move.l  d2,-(a7)
        move.l  8(a7),d0                ; a
        move.l  12(a7),d1               ; b
        move.l  16(a7),d2               ; d
md_nrm: cmp.l   #32767,d2
        bgt     md_shr
        cmp.l   #-32768,d2
        bge     md_ok
md_shr: asr.l   #1,d2
        asr.l   #1,d0
        bra     md_nrm
md_ok:  tst.w   d2
        beq     md_big                  ; d == 0
        cmp.l   #32767,d0
        bgt     md_big
        cmp.l   #-32768,d0
        blt     md_big
        cmp.l   #32767,d1
        bgt     md_big
        cmp.l   #-32768,d1
        blt     md_big
        muls    d1,d0                   ; a*b (32 bits)
        divs    d2,d0                   ; /d (cociente de 16 bits en d0.w)
        bvs     md_big
        ext.l   d0
        move.l  (a7)+,d2
        rts
md_big: move.l  #$7FFFFFFF,d0
        move.l  (a7)+,d2
        rts

; ---------------------------------------------------------------- _pj_xy / proj_all / proj_pt
; Proyeccion de un vertice de la vista (x,y,z de 4 bytes, z >= 1):  sx = 256 + x*fx/z,  sy = 48 - y*fy/z  (VIEW_CX, VIEW_CY de qfs.h)
; con division truncada hacia cero. Si x, y o z no caben en 16 bits con signo se desplazan los tres a la vez (la razon se
; conserva) hasta que quepan, y z se queda al menos en 1. El producto x*fx se hace con MULS (fx < 32768) y se divide con DIVS;
; si el cociente no cabe en 16 bits (vertice muy fuera de pantalla) se divide el valor absoluto en dos DIVU encadenadas
; (32/16 -> 32 bits, exacto). Nunca se llama a C. La version en C (view3d.c) hace lo mismo, bit a bit.
;   _pj_xy: a0 -> vertice, d4 = fx, d5 = fy  ->  d0 = sx, d1 = sy. Destruye d2, d3, a3, a4, a5.
        .globl  _pj_xy
_pj_xy:
        move.l  8(a0),d2                ; z
        move.l  (a0),d0                 ; x
        move.l  4(a0),d1                ; y
        cmp.l   #32767,d2
        bgt     pj_sh
        cmp.l   #32767,d0
        bgt     pj_sh
        cmp.l   #-32768,d0
        blt     pj_sh
        cmp.l   #32767,d1
        bgt     pj_sh
        cmp.l   #-32768,d1
        blt     pj_sh
pj_go:  movea.l d2,a5                   ; z (la division lenta lo destruye)
        muls    d4,d0                   ; x*fx
        bsr     _pj_div
        add.l   #256,d0
        movea.l d0,a3
        move.l  d1,d0                   ; y
        muls    d5,d0                   ; y*fy
        bsr     _pj_div
        neg.l   d0
        add.l   #48,d0
        move.l  d0,d1
        move.l  a3,d0
        rts
pj_sh:  asr.l   #1,d0                   ; no cabe: los tres a la vez
        asr.l   #1,d1
        asr.l   #1,d2
        cmp.l   #32767,d2
        bgt     pj_sh
        cmp.l   #32767,d0
        bgt     pj_sh
        cmp.l   #-32768,d0
        blt     pj_sh
        cmp.l   #32767,d1
        bgt     pj_sh
        cmp.l   #-32768,d1
        blt     pj_sh
        tst.l   d2
        bne     pj_go
        moveq   #1,d2
        bra     pj_go

; d0 = producto con signo, d2 = z (1..32767)  ->  d0 = d0 / d2 truncado hacia cero. Destruye d2, d3, a4. a5 = z original.
        .globl  _pj_div
_pj_div: divs    d2,d0
        bvs     pj_dvl
        ext.l   d0
        rts
pj_dvl: moveq   #0,d3                   ; el cociente no cabe en 16 bits (d0 sin tocar por DIVS)
        tst.l   d0
        bpl     pd_1
        neg.l   d0
        moveq   #1,d3                   ; signo negativo
pd_1:   movea.l d0,a4                   ; |p|
        clr.w   d0
        swap    d0                      ; palabra alta
        divu    d2,d0                   ; d0 = resto:q1
        move.w  d0,-(a7)                ; q1
        clr.w   d0                      ; resto en la palabra alta
        move.w  a4,d0                   ; y la palabra baja de |p|
        divu    d2,d0                   ; d0 = r2:q2
        and.l   #$FFFF,d0
        move.w  (a7)+,d2                ; q1
        swap    d2
        clr.w   d2
        or.l    d2,d0                   ; q1:q2
        tst.w   d3
        beq     pd_2
        neg.l   d0
pd_2:   move.l  a5,d2                   ; z otra vez
        rts

; int proj_all(const s32 *vw, int n, s32 near_z, int fx, int fy, s32 *pxy, u8 *front)
;   Proyecta los n vertices de la vista: para z >= near_z, front[k] = 1 y pxy[2k], pxy[2k+1] = proyeccion; si no, front[k] = 0.
;   Devuelve cuantos vertices estan delante del plano cercano.
        .globl  _proj_all
_proj_all:
        movem.l d2-d7/a2-a6,-(a7)       ; 44 bytes
        move.l  48(a7),a0               ; vw
        move.l  52(a7),d7               ; n
        move.l  56(a7),d6               ; near_z
        move.l  60(a7),d4               ; fx
        move.l  64(a7),d5               ; fy
        move.l  68(a7),a1               ; pxy
        move.l  72(a7),a2               ; front
        suba.l  a6,a6                   ; cuenta de vertices delante
        bra     pa_chk
pa_loop:
        cmp.l   8(a0),d6                ; z >= near_z ?
        bgt     pa_back
        move.b  #1,(a2)+
        bsr     _pj_xy
        move.l  d0,(a1)+
        move.l  d1,(a1)+
        lea     12(a0),a0
        addq.l  #1,a6
pa_chk: dbra    d7,pa_loop
        move.l  a6,d0
        movem.l (a7)+,d2-d7/a2-a6
        rts
pa_back:
        clr.b   (a2)+
        addq.l  #8,a1
        lea     12(a0),a0
        dbra    d7,pa_loop
        move.l  a6,d0
        movem.l (a7)+,d2-d7/a2-a6
        rts

; void proj_pt(const s32 *v, int fx, int fy, s32 *out): un solo vertice (z >= 1) -> out[0] = sx, out[1] = sy
        .globl  _proj_pt
_proj_pt:
        movem.l d2-d5/a2-a5,-(a7)       ; 32 bytes
        move.l  36(a7),a0
        move.l  40(a7),d4
        move.l  44(a7),d5
        move.l  48(a7),a2
        bsr     _pj_xy
        move.l  d0,(a2)
        move.l  d1,4(a2)
        movem.l (a7)+,d2-d5/a2-a5
        rts

; ---------------------------------------------------------------- _ld_core / lerp_div
; _ld_core: d0 = d, d1 = num, d2 = den (con signo)  ->  d0 = d*num/den truncado hacia cero, EXACTO (producto de 64 bits). Si
;   den = 0 devuelve 0; si el cociente no cabe en 32 bits, 0x7FFFFFFF (con el signo que toque). Conserva d3-d7 y a0-a6.
;   Caso rapido (|d|, |num|, |den| < 65536): MULU y dos DIVU encadenadas. Si no, producto de 64 bits con cuatro MULU y
;   division 64/32 por restas sucesivas (32 vueltas).
        .globl  _ld_core
_ld_core:
        movem.l d3-d6,-(a7)
        tst.l   d2
        beq     lc_zero
        moveq   #0,d3                   ; bit 0 = signo del resultado
        tst.l   d0
        bpl     lc_1
        neg.l   d0
        eor.w   #1,d3
lc_1:   tst.l   d1
        bpl     lc_2
        neg.l   d1
        eor.w   #1,d3
lc_2:   tst.l   d2
        bpl     lc_3
        neg.l   d2
        eor.w   #1,d3
lc_3:   move.l  d0,d4
        or.l    d1,d4
        or.l    d2,d4
        swap    d4
        tst.w   d4                      ; ¿los tres caben en 16 bits?
        bne     lc_big
        mulu    d1,d0                   ; |d|*|num| (32 bits sin signo)
        move.l  d0,d4
        clr.w   d0
        swap    d0                      ; palabra alta del producto
        divu    d2,d0                   ; r:q1
        move.w  d0,d5                   ; q1
        clr.w   d0
        move.w  d4,d0                   ; resto:palabra baja
        divu    d2,d0                   ; r2:q2
        and.l   #$FFFF,d0
        swap    d5
        clr.w   d5
        or.l    d5,d0                   ; q1:q2
        bra     lc_sgn
lc_big: move.w  d3,-(a7)                ; signo
        move.l  d0,d3
        swap    d3                      ; d3.w = ah
        move.l  d1,d4
        swap    d4                      ; d4.w = bh
        move.l  d0,d5
        mulu    d1,d5                   ; LL = al*bl
        move.l  d3,d6
        mulu    d1,d6                   ; HL = ah*bl
        mulu    d4,d0                   ; LH = al*bh
        mulu    d4,d3                   ; HH = ah*bh
        add.l   d0,d6                   ; mid = HL + LH
        moveq   #0,d4
        addx.l  d4,d4                   ; acarreo
        swap    d4                      ; acarreo << 16
        move.l  d6,d0
        swap    d0
        and.l   #$FFFF,d0               ; mid >> 16
        or.l    d4,d0
        add.l   d0,d3                   ; d3 = P_hi
        swap    d6
        clr.w   d6                      ; (mid & $FFFF) << 16
        add.l   d6,d5                   ; d5 = P_lo
        moveq   #0,d4
        addx.l  d4,d3
        cmp.l   d2,d3                   ; P_hi >= den: el cociente no cabe en 32 bits
        bcc     lc_ovf
        move.l  d3,d4                   ; resto = P_hi
        moveq   #31,d6
lc_dv:  add.l   d5,d5
        addx.l  d4,d4
        cmp.l   d2,d4
        bcs     lc_dn
        sub.l   d2,d4
        addq.l  #1,d5
lc_dn:  dbra    d6,lc_dv
        move.l  d5,d0
        move.w  (a7)+,d3
        bra     lc_sgn
lc_ovf: move.l  #$7FFFFFFF,d0
        move.w  (a7)+,d3
lc_sgn: btst    #0,d3
        beq     lc_ret
        neg.l   d0
lc_ret: movem.l (a7)+,d3-d6
        rts
lc_zero:
        moveq   #0,d0
        bra     lc_ret

; s32 lerp_div(s32 a, s32 b, s32 num, s32 den): a + (b-a)*num/den exacto (0 en el segundo termino si den = 0)
        .globl  _lerp_div
_lerp_div:
        movem.l d2/d5,-(a7)
        move.l  12(a7),d5               ; a
        move.l  16(a7),d0               ; b
        sub.l   d5,d0
        move.l  20(a7),d1               ; num
        move.l  24(a7),d2               ; den
        bsr     _ld_core
        add.l   d5,d0
        movem.l (a7)+,d2/d5
        rts

; ---------------------------------------------------------------- clip_edge
; int clip_edge(const s32 *in, int n, s32 *out, int edge, s32 lim)
;   Un paso de Sutherland-Hodgman 2D (x,y de 4 bytes): edge 0: x >= lim, 1: y >= lim, 2: x <= lim, 3: y <= lim. Hasta 24 vertices
;   de salida; devuelve cuantos. La interseccion usa lerp_div (exacta).
        .globl  _clip_edge
_clip_edge:
        movem.l d2-d7/a2-a5,-(a7)       ; 40 bytes
        move.l  44(a7),a0               ; in
        move.l  48(a7),d7               ; n
        move.l  52(a7),a1               ; out
        move.l  56(a7),d0               ; edge
        move.l  60(a7),d6               ; lim
        moveq   #0,d4                   ; m
        moveq   #0,d3                   ; desplazamiento de la coordenada comprobada (0 = x, 4 = y)
        btst    #0,d0
        beq     ce_1
        moveq   #4,d3
ce_1:   moveq   #0,d5                   ; 0: dentro si v >= lim, 1: dentro si v <= lim
        btst    #1,d0
        beq     ce_2
        moveq   #1,d5
ce_2:   tst.l   d7
        ble     ce_ret
        move.l  d7,d0
        lsl.l   #3,d0
        lea     0(a0,d0.l),a2           ; fin de la entrada
        move.l  a0,a4                   ; a
        subq.l  #1,d7
ce_lp:  lea     8(a4),a3                ; b (el siguiente; el ultimo vuelve al primero)
        cmpa.l  a2,a3
        bne     ce_b
        move.l  a0,a3
ce_b:   move.l  0(a4,d3.l),d0
        sub.l   d6,d0                   ; va - lim
        move.l  0(a3,d3.l),d1
        sub.l   d6,d1                   ; vb - lim
        tst.w   d5
        beq     ce_c
        neg.l   d0
        neg.l   d1
ce_c:   tst.l   d0                      ; dentro <=> >= 0
        bmi     ce_ao
        cmp.w   #24,d4
        bge     ce_ao
        move.l  (a4),(a1)+
        move.l  4(a4),(a1)+
        addq.w  #1,d4
ce_ao:  move.l  d0,d2
        eor.l   d1,d2
        bpl     ce_nx                   ; los dos del mismo lado: no hay corte
        cmp.w   #24,d4
        bge     ce_nx
        move.l  0(a4,d3.l),d1           ; va
        move.l  0(a3,d3.l),d2
        sub.l   d1,d2                   ; den = vb - va
        neg.l   d1
        add.l   d6,d1                   ; num = lim - va
        moveq   #4,d0
        sub.l   d3,d0                   ; desplazamiento de la otra coordenada
        movea.l d0,a5
        move.l  0(a3,a5.l),d0
        sub.l   0(a4,a5.l),d0           ; d = b[otra] - a[otra]
        bsr     _ld_core
        add.l   0(a4,a5.l),d0
        tst.l   d3
        bne     ce_y
        move.l  d6,(a1)+                ; limite en x: (lim, v)
        move.l  d0,(a1)+
        bra     ce_cn
ce_y:   move.l  d0,(a1)+                ; limite en y: (v, lim)
        move.l  d6,(a1)+
ce_cn:  addq.w  #1,d4
ce_nx:  move.l  a3,a4
        dbra    d7,ce_lp
ce_ret: move.l  d4,d0
        movem.l (a7)+,d2-d7/a2-a5
        rts

; ---------------------------------------------------------------- gfx_poly32
; void gfx_poly32(Surface *s, int n, const s32 *xy, Fill f)
;   Poligono con coordenadas de 32 bits: descarta lo que queda fuera de la ventana, recorta (cuatro pasos de clip_edge) si toca
;   el borde, y rellena con gfx_fill_convex; si el poligono no es convexo, el caso general en C (gfx_poly_gen).
;   Surface: base 0, rows 4, cx0 6, cy0 8, cx1 10, cy1 12 (s16; gfx.c lo comprueba al compilar).
        .globl  _gfx_poly32
        .extern _gfx_poly_gen
_gfx_poly32:
        link    a6,#-384                ; -192(a6) = buffer a, -384(a6) = buffer b (24 vertices de 8 bytes cada uno)
        movem.l d2-d7/a2-a5,-(a7)
        move.l  8(a6),a5                ; s
        move.l  12(a6),d7               ; n
        move.l  16(a6),a0               ; xy
        cmp.l   #3,d7
        blt     gp_ret
        cmp.l   #24,d7
        ble     gp_1
        moveq   #24,d7
gp_1:   move.l  #$7FFFFFFF,d2           ; lx
        move.l  #$80000000,d3           ; hx
        move.l  d2,d4                   ; ly
        move.l  d3,d5                   ; hy
        move.l  a0,a1
        move.l  d7,d6
        subq.l  #1,d6
gp_bb:  move.l  (a1)+,d0
        move.l  (a1)+,d1
        cmp.l   d0,d2
        ble     gp_b1
        move.l  d0,d2
gp_b1:  cmp.l   d0,d3
        bge     gp_b2
        move.l  d0,d3
gp_b2:  cmp.l   d1,d4
        ble     gp_b3
        move.l  d1,d4
gp_b3:  cmp.l   d1,d5
        bge     gp_b4
        move.l  d1,d5
gp_b4:  dbra    d6,gp_bb
        move.w  6(a5),d0
        ext.l   d0                      ; cx0
        move.w  10(a5),d1
        ext.l   d1                      ; cx1
        cmp.l   d0,d3
        blt     gp_ret                  ; hx < cx0
        cmp.l   d1,d2
        bgt     gp_ret                  ; lx > cx1
        moveq   #1,d6                   ; dentro del todo
        cmp.l   d0,d2
        bge     gp_i1
        moveq   #0,d6
gp_i1:  cmp.l   d1,d3
        ble     gp_i2
        moveq   #0,d6
gp_i2:  move.w  8(a5),d0
        ext.l   d0                      ; cy0
        move.w  12(a5),d1
        ext.l   d1                      ; cy1
        cmp.l   d0,d5
        blt     gp_ret                  ; hy < cy0
        cmp.l   d1,d4
        bgt     gp_ret                  ; ly > cy1
        cmp.l   d0,d4
        bge     gp_i3
        moveq   #0,d6
gp_i3:  cmp.l   d1,d5
        ble     gp_i4
        moveq   #0,d6
gp_i4:  tst.w   d6
        beq     gp_clip
        move.l  a0,a2                   ; sin recorte: el original
        bra     gp_fill
gp_clip:
        move.w  6(a5),d0                ; x >= cx0
        ext.l   d0
        move.l  d0,-(a7)
        clr.l   -(a7)
        pea     -192(a6)
        move.l  d7,-(a7)
        move.l  16(a6),-(a7)
        jsr     _clip_edge
        lea     20(a7),a7
        move.l  d0,d7
        move.w  8(a5),d0                ; y >= cy0
        ext.l   d0
        move.l  d0,-(a7)
        move.l  #1,-(a7)
        pea     -384(a6)
        move.l  d7,-(a7)
        pea     -192(a6)
        jsr     _clip_edge
        lea     20(a7),a7
        move.l  d0,d7
        move.w  10(a5),d0               ; x <= cx1
        ext.l   d0
        move.l  d0,-(a7)
        move.l  #2,-(a7)
        pea     -192(a6)
        move.l  d7,-(a7)
        pea     -384(a6)
        jsr     _clip_edge
        lea     20(a7),a7
        move.l  d0,d7
        move.w  12(a5),d0               ; y <= cy1
        ext.l   d0
        move.l  d0,-(a7)
        move.l  #3,-(a7)
        pea     -384(a6)
        move.l  d7,-(a7)
        pea     -192(a6)
        jsr     _clip_edge
        lea     20(a7),a7
        move.l  d0,d7
        lea     -384(a6),a2
gp_fill:
        cmp.l   #3,d7
        blt     gp_ret
        move.l  20(a6),-(a7)            ; f
        move.l  d7,-(a7)                ; m
        move.l  a2,-(a7)                ; poligono
        move.l  (a5),-(a7)              ; base de la pantalla
        jsr     _gfx_fill_convex
        lea     16(a7),a7
        tst.l   d0
        bne     gp_ret
        move.l  20(a6),-(a7)            ; no convexo: caso general
        move.l  a2,-(a7)
        move.l  d7,-(a7)
        move.l  a5,-(a7)
        jsr     _gfx_poly_gen
        lea     16(a7),a7
gp_ret: movem.l (a7)+,d2-d7/a2-a5
        unlk    a6
        rts

; ---------------------------------------------------------------- sdiv32
; s32 sdiv32(s32 a, s32 b): a / b truncado hacia cero (0 si b = 0). Si |b| < 65536, dos DIVU encadenadas; si no, restas sucesivas.
        .globl  _sdiv32
_sdiv32:
        movem.l d2-d4,-(a7)
        move.l  16(a7),d0               ; a
        move.l  20(a7),d1               ; b
        beq     sd_zero
        moveq   #0,d4                   ; signo
        tst.l   d0
        bpl     sd_1
        neg.l   d0
        not.w   d4
sd_1:   tst.l   d1
        bpl     sd_2
        neg.l   d1
        not.w   d4
sd_2:   move.l  d1,d2
        swap    d2
        tst.w   d2
        bne     sd_big
        move.l  d0,d2                   ; |a|
        clr.w   d0
        swap    d0                      ; palabra alta
        divu    d1,d0                   ; r:q1
        move.w  d0,d3                   ; q1
        clr.w   d0
        move.w  d2,d0                   ; resto:palabra baja
        divu    d1,d0                   ; r2:q2
        and.l   #$FFFF,d0
        swap    d3
        clr.w   d3
        or.l    d3,d0
        bra     sd_sg
sd_big: move.l  d0,d2                   ; dividendo (se va llenando de cociente)
        moveq   #0,d0                   ; resto
        moveq   #31,d3
sd_lp:  add.l   d2,d2
        addx.l  d0,d0
        cmp.l   d1,d0
        bcs     sd_nx
        sub.l   d1,d0
        addq.l  #1,d2
sd_nx:  dbra    d3,sd_lp
        move.l  d2,d0
sd_sg:  tst.w   d4
        beq     sd_end
        neg.l   d0
sd_end: movem.l (a7)+,d2-d4
        rts
sd_zero:
        moveq   #0,d0
        bra     sd_end

; ---------------------------------------------------------------- gfx_line
; void gfx_line(Surface *s, s32 x0, s32 y0, s32 x1, s32 y1, int c)
;   Linea recortada a la ventana de s. Los casos triviales se resuelven aqui: los dos extremos dentro -> gfx_line_raw; los dos
;   fuera por el mismo lado -> nada. Si no, el recorte de Cohen-Sutherland en C (gfx_line_c).
        .globl  _gfx_line
        .extern _gfx_line_c
_gfx_line:
        movem.l d2-d7/a2,-(a7)          ; 28 bytes: s 32, x0 36, y0 40, x1 44, y1 48, c 52
        move.l  32(a7),a2
        move.w  6(a2),d4
        ext.l   d4                      ; cx0
        move.w  10(a2),d5
        ext.l   d5                      ; cx1
        move.w  8(a2),d6
        ext.l   d6                      ; cy0
        move.w  12(a2),d7
        ext.l   d7                      ; cy1
        move.l  36(a7),d0               ; x0
        move.l  40(a7),d1               ; y0
        move.l  44(a7),d2               ; x1
        move.l  48(a7),d3               ; y1
        cmp.l   d4,d0
        blt     gl_nac
        cmp.l   d5,d0
        bgt     gl_nac
        cmp.l   d4,d2
        blt     gl_nac
        cmp.l   d5,d2
        bgt     gl_nac
        cmp.l   d6,d1
        blt     gl_nac
        cmp.l   d7,d1
        bgt     gl_nac
        cmp.l   d6,d3
        blt     gl_nac
        cmp.l   d7,d3
        bgt     gl_nac
        move.l  52(a7),-(a7)            ; todo dentro: gfx_line_raw(base, x0, y0, x1, y1, c)
        move.l  d3,-(a7)
        move.l  d2,-(a7)
        move.l  d1,-(a7)
        move.l  d0,-(a7)
        move.l  (a2),-(a7)
        jsr     _gfx_line_raw
        lea     24(a7),a7
gl_ret: movem.l (a7)+,d2-d7/a2
        rts
gl_nac: cmp.l   d4,d0                   ; los dos extremos fuera por el mismo lado: nada que pintar
        bge     gl_r1
        cmp.l   d4,d2
        blt     gl_ret
gl_r1:  cmp.l   d5,d0
        ble     gl_r2
        cmp.l   d5,d2
        bgt     gl_ret
gl_r2:  cmp.l   d6,d1
        bge     gl_r3
        cmp.l   d6,d3
        blt     gl_ret
gl_r3:  cmp.l   d7,d1
        ble     gl_sl
        cmp.l   d7,d3
        bgt     gl_ret
gl_sl:  move.l  52(a7),-(a7)            ; recorte general en C
        move.l  d3,-(a7)
        move.l  d2,-(a7)
        move.l  d1,-(a7)
        move.l  d0,-(a7)
        move.l  a2,-(a7)
        jsr     _gfx_line_c
        lea     24(a7),a7
        bra     gl_ret

; ---------------------------------------------------------------- draw_lights
; void draw_lights(Surface *s, const s32 *vw, const u8 *idx, int n, int step, int shift, s32 near_z, int fx, int fy, int col, int dbl)
;   Para k = 0, step, 2*step... < n: el vertice vw[idx[k] >> shift]; si esta delante del plano cercano se proyecta (como
;   proj_all) y se pinta un punto de color col (0..3) con recorte a la superficie, y otro a su derecha si dbl.
        .globl  _draw_lights
_draw_lights:
        movem.l d2-d7/a2-a6,-(a7)       ; 44 bytes: s 48, vw 52, idx 56, n 60, step 64, shift 68, near_z 72, fx 76, fy 80, col 84, dbl 88
        move.l  48(a7),a2
        move.l  72(a7),d6
        move.l  76(a7),d4
        move.l  80(a7),d5
        move.l  84(a7),a6
        moveq   #0,d7                   ; k
dl_lp:  cmp.l   60(a7),d7
        bge     dl_end
        move.l  56(a7),a1
        moveq   #0,d0
        move.b  0(a1,d7.l),d0
        move.l  68(a7),d1
        lsr.l   d1,d0
        mulu    #12,d0
        move.l  52(a7),a0
        adda.l  d0,a0
        cmp.l   8(a0),d6                ; z < near_z: no se pinta
        bgt     dl_nx
        bsr     _pj_xy
        bsr     _dl_plot
        tst.l   88(a7)
        beq     dl_nx
        addq.l  #1,d0
        bsr     _dl_plot
dl_nx:  add.l   64(a7),d7
        bra     dl_lp
dl_end: movem.l (a7)+,d2-d7/a2-a6
        rts

; punto (d0, d1) de color a6 en la superficie a2, recortado. Conserva d0, d1. Destruye d2, d3, a3.
        .globl  _dl_plot
_dl_plot:
        move.w  6(a2),d2
        ext.l   d2
        cmp.l   d2,d0
        blt     dp_ret
        move.w  10(a2),d2
        ext.l   d2
        cmp.l   d2,d0
        bgt     dp_ret
        move.w  8(a2),d2
        ext.l   d2
        cmp.l   d2,d1
        blt     dp_ret
        move.w  12(a2),d2
        ext.l   d2
        cmp.l   d2,d1
        bgt     dp_ret
        move.l  d1,d2
        lsl.l   #7,d2
        movea.l (a2),a3
        adda.l  d2,a3
        move.l  d0,d2
        lsr.l   #3,d2
        add.l   d2,d2
        adda.l  d2,a3
        moveq   #7,d2
        and.w   d0,d2
        move.w  #$80,d3
        lsr.w   d2,d3                   ; bit del pixel
        move.l  a6,d2
        btst    #1,d2
        beq     dp_g0
        or.b    d3,(a3)
        bra     dp_r
dp_g0:  not.b   d3
        and.b   d3,(a3)
        not.b   d3
dp_r:   btst    #0,d2
        beq     dp_r0
        or.b    d3,1(a3)
        rts
dp_r0:  not.b   d3
        and.b   d3,1(a3)
dp_ret: rts

; ---------------------------------------------------------------- poly_fast
; int poly_fast(const u8 *idx, int n, const u8 *front, const s32 *pxy, s32 *out, int cull)
;   Cara con n indices de vertice. Devuelve -1 si algun vertice esta detras del plano (hay que recortar, camino C),
;   si no copia las proyecciones a out (x,y de 4 bytes) y: con cull != 0 devuelve 0 si es una cara trasera
;   (area con signo >= 0, coordenadas reducidas a 11 bits como area2 de C) y 1 si se dibuja.
        .globl  _poly_fast
_poly_fast:
        movem.l d2-d7/a2-a4,-(a7)       ; 36 bytes
        move.l  40(a7),a0               ; idx
        move.l  48(a7),a1               ; front
        move.l  52(a7),a2               ; pxy
        move.l  56(a7),a3               ; out
        moveq   #-1,d0
        move.l  44(a7),d6               ; n
        cmp.l   #3,d6
        blt     pf_ret
        ; una sola pasada: comprueba que el vertice esta delante, copia su proyeccion y lleva la caja (lx,hx en d3/d4; ly,hy en d5/d7)
        move.l  #$7FFFFFFF,d3
        move.l  #$80000000,d4
        move.l  d3,d5
        move.l  d4,d7
        subq.l  #1,d6
pf_2:   moveq   #0,d0
        move.b  (a0)+,d0
        tst.b   0(a1,d0.l)
        beq     pf_notf
        lsl.l   #3,d0
        move.l  0(a2,d0.l),d1
        move.l  4(a2,d0.l),d2
        move.l  d1,(a3)+
        move.l  d2,(a3)+
        cmp.l   d1,d3
        ble     pf_b1
        move.l  d1,d3
pf_b1:  cmp.l   d1,d4
        bge     pf_b2
        move.l  d1,d4
pf_b2:  cmp.l   d2,d5
        ble     pf_b3
        move.l  d2,d5
pf_b3:  cmp.l   d2,d7
        bge     pf_b4
        move.l  d2,d7
pf_b4:  dbra    d6,pf_2
        ; cara de un solo pixel (caja <= 1 en x e y): no se pinta
        move.l  d4,d0
        sub.l   d3,d0
        cmp.l   #1,d0
        bgt     pf_big
        move.l  d7,d0
        sub.l   d5,d0
        cmp.l   #1,d0
        bgt     pf_big
        moveq   #0,d0
        bra     pf_ret
pf_big: moveq   #1,d0
        tst.l   60(a7)                  ; cull
        beq     pf_ret                  ; sin descarte trasero: dibujar
        ; mx = mayor valor absoluto de las coordenadas = max(|lx|,|hx|,|ly|,|hy|)
        move.l  d3,d0
        bpl     pf_a1
        neg.l   d0
pf_a1:  move.l  d4,d1
        bpl     pf_a2
        neg.l   d1
pf_a2:  cmp.l   d1,d0
        bge     pf_a3
        move.l  d1,d0
pf_a3:  move.l  d5,d1
        bpl     pf_a4
        neg.l   d1
pf_a4:  cmp.l   d1,d0
        bge     pf_a5
        move.l  d1,d0
pf_a5:  move.l  d7,d1
        bpl     pf_a6
        neg.l   d1
pf_a6:  cmp.l   d1,d0
        bge     pf_a7
        move.l  d1,d0
        ; sh = menor desplazamiento con (mx >> sh) <= 2047
pf_a7:  moveq   #0,d3
pf_7:   move.l  d0,d1
        lsr.l   d3,d1
        cmp.l   #2047,d1
        ble     pf_8
        addq.l  #1,d3
        bra     pf_7
        ; area: a3 apunta al final de out; el ultimo vertice es el anterior al primero
pf_8:   move.l  44(a7),d7               ; n
        move.l  d7,d0
        lsl.l   #3,d0
        sub.l   d0,a3                   ; a3 = out
        lea     -8(a3,d0.l),a4          ; a4 = ultimo vertice
        move.l  (a4),d2
        asr.l   d3,d2                   ; x0
        move.l  4(a4),d4
        asr.l   d3,d4                   ; y0
        moveq   #0,d5                   ; area
        move.l  d7,d6
        subq.l  #1,d6
pf_9:   move.l  (a3)+,d1
        asr.l   d3,d1                   ; x1
        move.l  (a3)+,d0
        asr.l   d3,d0                   ; y1
        move.w  d2,d7
        muls    d0,d7                   ; x0*y1
        add.l   d7,d5
        move.w  d1,d7
        muls    d4,d7                   ; x1*y0
        sub.l   d7,d5
        move.w  d1,d2
        move.w  d0,d4
        dbra    d6,pf_9
        moveq   #1,d0
        tst.l   d5
        blt     pf_ret                  ; area < 0: cara delantera, se dibuja
        moveq   #0,d0                   ; area >= 0: trasera
        bra     pf_ret
pf_notf:
        moveq   #-1,d0
pf_ret: movem.l (a7)+,d2-d7/a2-a4
        rts

; ---------------------------------------------------------------- gfx_text_raw
; void gfx_text_raw(u8 *row0, s32 x, const char *t, s32 c)
;   Texto de celdas de 6 pixeles con la fuente 5x7 (_font5x7), sin recorte (el que llama comprueba que cabe):
;   row0 = direccion de la primera fila (base + y*128). Los puntos encendidos se ponen a la tinta c (0 negro .. 3 blanco:
;   bit 1 = verde, bit 0 = rojo; con c = 0 borran); los apagados no se tocan. Se escribe por palabras (verde,rojo de un
;   grupo de 8 pixeles) y solo si la fila de la letra tiene puntos en ese grupo.
        .globl  _gfx_text_raw
_gfx_text_raw:
        movem.l d2-d7/a2-a4,-(a7)       ; 36 bytes
        move.l  40(a7),a4               ; row0
        move.l  44(a7),d7               ; x
        move.l  48(a7),a2               ; texto
        move.l  52(a7),d0               ; tinta
        moveq   #0,d6                   ; cc: palabra de color (verde en el byte alto, rojo en el bajo)
        btst    #1,d0
        beq     tx_c1
        move.w  #$FF00,d6
tx_c1:  btst    #0,d0
        beq     tx_nxt
        or.w    #$00FF,d6
tx_nxt: moveq   #0,d0
        move.b  (a2)+,d0
        beq     tx_end
        cmp.b   #97,d0
        blt     tx_up
        cmp.b   #122,d0
        bgt     tx_up
        sub.b   #32,d0                  ; minusculas a mayusculas
tx_up:  cmp.b   #32,d0
        blt     tx_q
        cmp.b   #95,d0
        ble     tx_ok
tx_q:   moveq   #63,d0                  ; '?'
tx_ok:  sub.w   #32,d0
        move.w  d0,d1
        lsl.w   #3,d1
        sub.w   d0,d1                   ; (ch-32)*7
        lea     _font5x7,a1
        adda.w  d1,a1                   ; glifo
        move.l  d7,d0
        lsr.l   #3,d0
        add.l   d0,d0                   ; byte del grupo = (x>>3)*2
        move.l  a4,a0
        adda.l  d0,a0                   ; a0 = fila 0 del caracter
        move.l  d7,d5
        and.w   #7,d5                   ; desplazamiento dentro del grupo
        moveq   #6,d4                   ; 7 filas
tx_row: moveq   #0,d1
        move.b  (a1)+,d1                ; fila del glifo (5 bits)
        beq     tx_skp
        lsl.w   #8,d1
        lsl.w   #3,d1                   ; (v << 3) << 8: el pixel izquierdo en el bit 15
        lsr.w   d5,d1                   ; campo de 16 bits repartido en dos grupos
        move.w  d1,d2
        lsr.w   #8,d2                   ; mascara del primer grupo (un byte)
        beq     tx_g2
        move.w  d2,d3
        lsl.w   #8,d3
        or.w    d3,d2                   ; mascara de palabra (verde y rojo)
        move.w  d2,d3
        not.w   d3
        and.w   (a0),d3                 ; lo que se conserva
        move.w  d6,d0
        and.w   d2,d0                   ; la tinta en los pixeles encendidos
        or.w    d0,d3
        move.w  d3,(a0)
tx_g2:  and.w   #$00FF,d1               ; mascara del segundo grupo
        beq     tx_skp
        move.w  d1,d3
        lsl.w   #8,d3
        or.w    d3,d1
        move.w  d1,d3
        not.w   d3
        and.w   2(a0),d3
        move.w  d6,d0
        and.w   d1,d0
        or.w    d0,d3
        move.w  d3,2(a0)
tx_skp: lea     128(a0),a0
        dbra    d4,tx_row
        addq.l  #6,d7
        bra     tx_nxt
tx_end: movem.l (a7)+,d2-d7/a2-a4
        rts

; ---------------------------------------------------------------- gfx_line_xor
; void gfx_line_xor(u8 *base, s32 x0, s32 y0, s32 x1, s32 y1, s32 c)
;   Como gfx_line_raw pero con OR exclusivo: dibujar dos veces la misma linea la borra y deja intacto lo que habia debajo
;   (agujas de los instrumentos). c: bit 1 = canal verde, bit 0 = canal rojo.
        .globl  _gfx_line_xor
_gfx_line_xor:
        movem.l d2-d7/a2,-(a7)          ; 28 bytes
        move.l  32(a7),a0
        move.l  36(a7),d0               ; x0
        move.l  40(a7),d1               ; y0
        move.l  44(a7),d2               ; x1
        move.l  48(a7),d3               ; y1
        move.l  52(a7),d6               ; color
        move.l  d1,d4
        lsl.l   #7,d4
        adda.l  d4,a0
        move.l  d0,d4
        lsr.l   #3,d4
        add.l   d4,d4
        adda.l  d4,a0
        moveq   #0,d7
        sub.l   d0,d2
        bpl     lx_1
        neg.l   d2
        moveq   #1,d7
lx_1:   movea.l #128,a2
        sub.l   d1,d3
        bpl     lx_2
        neg.l   d3
        movea.l #-128,a2
lx_2:   moveq   #7,d4
        and.w   d0,d4
        move.w  #$0080,d0
        lsr.b   d4,d0
        move.l  d2,d5
        cmp.l   d3,d5
        bge     lx_3
        move.l  d3,d5
lx_3:   neg.l   d3
        move.l  d2,d1
        add.l   d3,d1
lx_loop:
        btst    #1,d6
        beq     lx_g1
        eor.b   d0,(a0)
lx_g1:  btst    #0,d6
        beq     lx_r1
        eor.b   d0,1(a0)
lx_r1:  move.l  d1,d4
        add.l   d4,d4
        cmp.l   d3,d4
        ble     lx_nx
        add.l   d3,d1
        tst.b   d7
        bne     lx_left
        lsr.b   #1,d0
        bne     lx_nx
        move.b  #$80,d0
        addq.l  #2,a0
        bra     lx_nx
lx_left:
        lsl.b   #1,d0
        bne     lx_nx
        moveq   #1,d0
        subq.l  #2,a0
lx_nx:  cmp.l   d2,d4
        bge     lx_ny
        add.l   d2,d1
        adda.l  a2,a0
lx_ny:  dbra    d5,lx_loop
        movem.l (a7)+,d2-d7/a2
        rts

; ---------------------------------------------------------------- gfx_fill_convex
; int gfx_fill_convex(u8 *base, const s32 *p, int m, s32 f)
;   Rellena un poligono CONVEXO de m vertices (x,y de 4 bytes, ya recortado a la ventana: x >= 0, 0 <= y < 256) en la pantalla
;   base con el patron f (palabra alta en las filas pares, baja en las impares). Devuelve 0 si no es convexo (no dibuja
;   nada) y 1 si lo trato. Mismas reglas que la version en C: x en el centro de la fila, la ultima fila incluye el
;   extremo inferior, redondeo de la pendiente por division entera truncada hacia cero.
;   Fases: 1) convexidad (signo de los productos vectoriales), 2) ymin/ymax, 3) cada arista deja en rmin_/rmax_/rcnt_ el
;   limite izquierdo/derecho de cada una de sus filas, 4) una pasada pinta [rmin,rmax] de cada fila con cnt >= 2.
;   La pendiente (dx<<16)/dy se hace en dos DIVU de 16 bits (parte entera y fraccion): |dx| < 65536 y dy < 256.
        .globl  _gfx_fill_convex
        .extern _rcnt_
        .extern _rmin_
        .extern _rmax_
_gfx_fill_convex:
        link    a6,#-8                  ; -4(a6) ymin, -8(a6) ymax
        movem.l d2-d7/a2-a5,-(a7)
        move.l  12(a6),a0               ; p
        move.l  16(a6),d7               ; m
        move.l  d7,d0
        lsl.l   #3,d0
        lea     0(a0,d0.l),a4           ; a4 = p + 8m (fin)
        ; ---- 1) convexidad
        moveq   #0,d6                   ; signo visto: 0 ninguno, 1 positivo, -1 negativo
        move.l  a0,a1
        move.l  d7,d5
        subq.l  #1,d5
fc_cv:  lea     8(a1),a2
        cmp.l   a4,a2
        bne     fc_cj
        move.l  a0,a2
fc_cj:  lea     8(a2),a3
        cmp.l   a4,a3
        bne     fc_ck
        move.l  a0,a3
fc_ck:  move.l  (a2),d0
        sub.l   (a1),d0                 ; xj - xi
        move.l  4(a3),d1
        sub.l   4(a2),d1                ; yk - yj
        muls    d1,d0
        move.l  4(a2),d1
        sub.l   4(a1),d1                ; yj - yi
        move.l  (a3),d2
        sub.l   (a2),d2                 ; xk - xj
        muls    d2,d1
        sub.l   d1,d0                   ; producto vectorial
        beq     fc_cn
        bmi     fc_cg
        cmp.l   #-1,d6
        beq     fc_no
        moveq   #1,d6
        bra     fc_cn
fc_cg:  cmp.l   #1,d6
        beq     fc_no
        moveq   #-1,d6
fc_cn:  move.l  a2,a1
        dbra    d5,fc_cv
        ; ---- 2) ymin / ymax
        move.l  #32767,d3
        move.l  #-32768,d4
        move.l  a0,a1
        move.l  d7,d5
        subq.l  #1,d5
fc_ym:  move.l  4(a1),d0
        addq.l  #8,a1
        cmp.l   d0,d3
        ble     fc_y1
        move.l  d0,d3
fc_y1:  cmp.l   d0,d4
        bge     fc_y2
        move.l  d0,d4
fc_y2:  dbra    d5,fc_ym
        move.l  d3,-4(a6)
        move.l  d4,-8(a6)
        tst.l   d3
        blt     fc_ok                   ; fuera de las 256 filas: nada que pintar
        cmp.l   #255,d4
        bgt     fc_ok
        lea     _rcnt_,a1               ; cuentas a cero
        adda.l  d3,a1
        move.l  d4,d0
        sub.l   d3,d0
fc_cl:  clr.b   (a1)+
        dbra    d0,fc_cl
        ; ---- 3) aristas
        move.l  a0,a5                   ; vertice i
        subq.l  #1,d7                   ; m-1 vueltas (dbra)
fc_ed:  lea     8(a5),a3                ; vertice j
        cmp.l   a4,a3
        bne     fc_ej
        move.l  a0,a3
fc_ej:  move.l  a5,a2                   ; vertice i
        move.l  4(a2),d0
        cmp.l   4(a3),d0
        beq     fc_en                   ; arista horizontal: nada
        blt     fc_eo
        exg     a2,a3                   ; a2 = extremo de menor y
fc_eo:  move.l  (a2),d0                 ; x0
        move.l  4(a2),d1                ; y0
        move.l  (a3),d5                 ; x1
        move.l  4(a3),d6                ; y1
        sub.l   d1,d6                   ; dy
        move.l  d6,d2                   ; filas = dy (+1 si y1 es la ultima fila del poligono)
        move.l  d1,d3
        add.l   d6,d3
        cmp.l   -8(a6),d3
        bne     fc_e1
        addq.l  #1,d2
fc_e1:  subq.l  #1,d2                   ; para dbra
        sub.l   d0,d5                   ; dx
        moveq   #0,d4                   ; dx < 0 ?
        tst.l   d5
        bpl     fc_e2
        neg.l   d5
        moveq   #1,d4
fc_e2:  divu    d6,d5                   ; d5 = resto:cociente
        swap    d5                      ; d5 = cociente:resto
        moveq   #0,d3
        move.w  d5,d3                   ; resto
        swap    d3                      ; resto << 16
        divu    d6,d3                   ; fraccion (16 bits)
        move.w  d3,d5                   ; d5 = cociente:fraccion = |pendiente| Q16
        tst.w   d4
        beq     fc_e3
        neg.l   d5
fc_e3:  swap    d0
        clr.w   d0
        add.l   #32768,d0               ; x del centro de la fila y0, Q16
        lea     _rcnt_,a1
        adda.l  d1,a1
        add.l   d1,d1
        lea     _rmin_,a2
        adda.l  d1,a2
        lea     _rmax_,a3
        adda.l  d1,a3
fc_rw:  move.l  d0,d1
        swap    d1
        ext.l   d1                      ; x entera de esta fila
        tst.b   (a1)
        bne     fc_ro
        move.w  d1,(a2)
        move.w  d1,(a3)
        bra     fc_rn
fc_ro:  cmp.w   (a2),d1
        bge     fc_rh
        move.w  d1,(a2)
        bra     fc_rn
fc_rh:  cmp.w   (a3),d1
        ble     fc_rn
        move.w  d1,(a3)
fc_rn:  addq.b  #1,(a1)+
        addq.l  #2,a2
        addq.l  #2,a3
        add.l   d5,d0
        dbra    d2,fc_rw
fc_en:  lea     8(a5),a5
        dbra    d7,fc_ed
        ; ---- 4) filas
        move.l  -4(a6),d3               ; ymin
        move.l  -8(a6),d4
        sub.l   d3,d4                   ; filas - 1
        move.l  8(a6),a4                ; base
        move.l  d3,d0
        lsl.l   #7,d0
        adda.l  d0,a4                   ; puntero a la fila ymin
        lea     _rcnt_,a1
        adda.l  d3,a1
        move.l  d3,d1
        add.l   d1,d1
        lea     _rmin_,a2
        adda.l  d1,a2
        lea     _rmax_,a3
        adda.l  d1,a3
        move.l  20(a6),d6               ; f
        move.l  d6,d5
        swap    d5                      ; d5.w = patron de filas pares, d6.w = de impares
        move.l  d3,d7
        and.w   #1,d7                   ; paridad de la fila
        lea     fc_lm,a5                ; tabla de mascaras de los extremos (para _fc_hspan)
fc_sp:  cmp.b   #2,(a1)
        bcs     fc_sn
        moveq   #0,d0
        move.w  (a2),d0
        moveq   #0,d1
        move.w  (a3),d1
        tst.w   d7
        bne     fc_so
        move.w  d5,d2
        bra     fc_sg
fc_so:  move.w  d6,d2
fc_sg:  move.l  a4,a0
        bsr     _fc_hspan
fc_sn:  lea     128(a4),a4
        addq.l  #1,a1
        addq.l  #2,a2
        addq.l  #2,a3
        eor.w   #1,d7
        dbra    d4,fc_sp
fc_ok:  moveq   #1,d0
        bra     fc_ret
fc_no:  moveq   #0,d0
fc_ret: movem.l (a7)+,d2-d7/a2-a5
        unlk    a6
        rts

; tramo horizontal: a0 = fila, d0 = x0, d1 = x1 (0 <= x0 <= x1 < 512), d2 = patron (G en el byte alto, R en el bajo), a5 = fc_lm.
; Conserva d3-d7 y a1-a5 (destruye d0, d1, a0). Las mascaras de los extremos salen de dos tablas de 8 palabras y cada palabra
; parcial se mezcla con  nueva = vieja ^ ((vieja ^ patron) & mascara).
        .globl  _fc_hspan
_fc_hspan:
        movem.l d3-d5,-(a7)
        move.w  d0,d3
        and.w   #7,d3
        add.w   d3,d3
        move.w  0(a5,d3.w),d3           ; mascara izquierda (G y R)
        move.w  d1,d4
        and.w   #7,d4
        add.w   d4,d4
        move.w  16(a5,d4.w),d4          ; mascara derecha
        lsr.w   #3,d0                   ; palabra de x0
        lsr.w   #3,d1
        sub.w   d0,d1                   ; palabras despues de la primera
        add.w   d0,d0
        adda.w  d0,a0                   ; direccion de la primera palabra
        tst.w   d1
        bne     fh_m
        and.w   d4,d3                   ; una sola palabra: izquierda & derecha
        move.w  (a0),d5
        eor.w   d2,d5
        and.w   d3,d5
        eor.w   d5,(a0)
        bra     fh_e
fh_m:   move.w  (a0),d5                 ; primera palabra
        eor.w   d2,d5
        and.w   d3,d5
        eor.w   d5,(a0)+
        subq.w  #1,d1                   ; palabras completas
        move.w  d1,d3
        lsr.w   #1,d3                   ; de dos en dos: la 68008 admite accesos largos en cualquier direccion
        move.w  d2,d5
        swap    d5
        move.w  d2,d5                   ; patron en las dos mitades
        bra     fh_c
fh_l:   move.l  d5,(a0)+
fh_c:   dbra    d3,fh_l
        btst    #0,d1
        beq     fh_lst
        move.w  d2,(a0)+
fh_lst: move.w  (a0),d5                 ; ultima palabra
        eor.w   d2,d5
        and.w   d4,d5
        eor.w   d5,(a0)
fh_e:   movem.l (a7)+,d3-d5
        rts

; mascaras: fc_lm[i] = ($FF >> i) en G y R; fc_lm+16 [i] = ($FF << (7-i)) & $FF en G y R
fc_lm:  dc.w    $FFFF,$7F7F,$3F3F,$1F1F,$0F0F,$0707,$0303,$0101
fc_rm:  dc.w    $8080,$C0C0,$E0E0,$F0F0,$F8F8,$FCFC,$FEFE,$FFFF

; ---------------------------------------------------------------- xform_obj3
; void xform_obj3(const s16 *m, const s16 *v, s32 *out, int cnt, int step, int vshift, int U, int rx, int ry, int rz)
;   Prepara y transforma los vertices de un objeto en un solo paso (antes: un bucle en C que rellenaba un vector de
;   vertices relativos y luego xform3): para cada uno de los cnt vertices (cada `step`-esimo del objeto)
;       rel = (s16)(r + ((v << vshift) >> U))     (en x, y, z con rx, ry, rz)
;       out = (m * rel) >> 14                      (matriz 3x3 Q14, como xform3)
;   El resultado es el mismo que el de la version en C: la suma se trunca a 16 bits con ADD.W y los productos son MULS.
        .globl  _xform_obj3
_xform_obj3:
        movem.l d2-d7/a2-a5,-(a7)       ; 40 bytes
        move.l  44(a7),a2               ; matriz
        move.l  48(a7),a0               ; vertices del objeto (s16 x3)
        move.l  52(a7),a1               ; salida (s32 x3)
        move.l  56(a7),d7               ; cnt
        move.l  60(a7),d0
        subq.l  #1,d0
        mulu    #6,d0
        move.l  d0,60(a7)               ; bytes a saltar tras cada vertice leido = (step-1)*6
        move.l  64(a7),d0               ; vshift
        sub.l   68(a7),d0               ; desplazamiento neto (vshift - U): (v << vshift) >> U = v << neto  o  v >> -neto
        moveq   #0,d5                   ; a la izquierda
        moveq   #0,d6                   ; a la derecha
        tst.l   d0
        bmi     xo_n
        move.l  d0,d5
        bra     xo_s
xo_n:   neg.l   d0
        move.l  d0,d6
xo_s:   move.l  72(a7),a3               ; rx
        move.l  76(a7),a4               ; ry
        move.l  80(a7),a5               ; rz
        bra     xo_chk
xo_loop:
        tst.w   d6
        bne     xo_r
        move.w  (a0)+,d0                ; desplazamiento a la izquierda: solo importan los 16 bits bajos, asi que bastan palabras
        asl.w   d5,d0
        add.w   a3,d0                   ; x relativo (16 bits)
        move.w  (a0)+,d1
        asl.w   d5,d1
        add.w   a4,d1                   ; y
        move.w  (a0)+,d2
        asl.w   d5,d2
        add.w   a5,d2                   ; z
        bra     xo_m
xo_r:   move.w  (a0)+,d0                ; a la derecha (el resultado cabe en 16 bits)
        asr.w   d6,d0
        add.w   a3,d0
        move.w  (a0)+,d1
        asr.w   d6,d1
        add.w   a4,d1
        move.w  (a0)+,d2
        asr.w   d6,d2
        add.w   a5,d2
xo_m:   adda.l  60(a7),a0
        move.w  (a2),d3
        muls    d0,d3
        move.w  2(a2),d4
        muls    d1,d4
        add.l   d4,d3
        move.w  4(a2),d4
        muls    d2,d4
        add.l   d4,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.l  d3,(a1)+
        move.w  6(a2),d3
        muls    d0,d3
        move.w  8(a2),d4
        muls    d1,d4
        add.l   d4,d3
        move.w  10(a2),d4
        muls    d2,d4
        add.l   d4,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.l  d3,(a1)+
        move.w  12(a2),d3
        muls    d0,d3
        move.w  14(a2),d4
        muls    d1,d4
        add.l   d4,d3
        move.w  16(a2),d4
        muls    d2,d4
        add.l   d4,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.l  d3,(a1)+
xo_chk: dbra    d7,xo_loop
        movem.l (a7)+,d2-d7/a2-a5
        rts

; ---------------------------------------------------------------- xform_pre
; int xform_pre(const Camera *c, const Obj *o, s32 dmax, s32 *res)
;   Prologo de xform_obj: posicion del objeto respecto a la camara, escala U, descartes por distancia, por esfera
;   envolvente contra la piramide de vision y por tamano (solidos de menos de un pixel). Devuelve 0 si el objeto no se
;   dibuja; si no, res[0] = U y res[1..3] = rx, ry, rz (16 bits con signo, en 4 bytes).
;   Lee Camera y Obj por desplazamientos (view3d.c los comprueba en compilacion):
;     Camera: x 0, y 4, z 8, m 18..35, fx 36, fy 38      Obj: type 0, ox 4, oy 8, oz 12 (s16), radius 14 (u16)
;   Las multiplicaciones por fx, fy son 32x16 exactas (xp_mul); las de 240 y 48 (VIEW_W/2, VIEW_H/2), desplazamientos.
        .globl  _xform_pre
_xform_pre:
        movem.l d2-d7/a2-a4,-(a7)       ; 36 bytes
        move.l  40(a7),a0               ; c
        move.l  44(a7),a1               ; o
        move.l  4(a1),d0
        asl.l   #8,d0
        sub.l   (a0),d0                 ; dx
        move.l  8(a1),d1
        asl.l   #8,d1
        sub.l   4(a0),d1                ; dy
        move.w  12(a1),d2
        ext.l   d2
        asl.l   #8,d2
        sub.l   8(a0),d2                ; dz
        move.l  d0,d3
        bpl     xp_1
        neg.l   d3
xp_1:   move.l  d1,d4
        bpl     xp_2
        neg.l   d4
xp_2:   cmp.l   d4,d3
        bge     xp_3
        move.l  d4,d3
xp_3:   asr.l   #8,d3                   ; d = max(|dx|,|dy|) >> 8
        moveq   #0,d4
        move.w  14(a1),d4               ; radio
        move.l  d3,d5
        sub.l   d4,d5
        cmp.l   48(a7),d5               ; d - radio > dmax ?
        bgt     xp_cull
        add.l   d4,d3
        move.l  d2,d5
        bpl     xp_4
        neg.l   d5
xp_4:   asr.l   #8,d5
        add.l   d5,d3                   ; d += radio + |dz| >> 8
        moveq   #0,d6                   ; U
xp_u:   move.l  d3,d5
        asl.l   #3,d5
        asr.l   d6,d5
        cmp.l   #28000,d5
        ble     xp_5
        addq.l  #1,d6
        cmp.l   #9,d6
        bgt     xp_cull
        bra     xp_u
xp_5:   move.l  d6,d7
        addq.l  #5,d7
        asr.l   d7,d0
        asr.l   d7,d1
        asr.l   d7,d2
        ext.l   d0                      ; (s16)
        ext.l   d1
        ext.l   d2
        move.l  52(a7),a2
        move.l  d6,(a2)
        move.l  d0,4(a2)
        move.l  d1,8(a2)
        move.l  d2,12(a2)
        lea     18(a0),a3               ; matriz
        move.w  (a3),d3                 ; cv0 = m0*rx + m1*ry + m2*rz >> 14
        muls    d0,d3
        move.w  2(a3),d7
        muls    d1,d7
        add.l   d7,d3
        move.w  4(a3),d7
        muls    d2,d7
        add.l   d7,d3
        asr.l   #7,d3
        asr.l   #7,d3
        move.w  6(a3),d4                ; cv1
        muls    d0,d4
        move.w  8(a3),d7
        muls    d1,d7
        add.l   d7,d4
        move.w  10(a3),d7
        muls    d2,d7
        add.l   d7,d4
        asr.l   #7,d4
        asr.l   #7,d4
        move.w  12(a3),d5               ; cv2
        muls    d0,d5
        move.w  14(a3),d7
        muls    d1,d7
        add.l   d7,d5
        move.w  16(a3),d7
        muls    d2,d7
        add.l   d7,d5
        asr.l   #7,d5
        asr.l   #7,d5
        moveq   #0,d6
        move.w  14(a1),d6               ; r = ((radio*9) >> U) + 8
        move.l  d6,d7
        asl.l   #3,d6
        add.l   d7,d6
        move.l  52(a7),a2
        move.l  (a2),d7
        asr.l   d7,d6
        addq.l  #8,d6
        move.l  d5,d7
        add.l   d6,d7                   ; z2 = cv2 + r
        bmi     xp_cull
        move.l  d3,d0                   ; (|cv0| - r) * fx  >  z2 * 240 ?
        bpl     xp_6
        neg.l   d0
xp_6:   sub.l   d6,d0
        moveq   #0,d1
        move.w  36(a0),d1
        bsr     xp_mul
        move.l  d7,d2
        asl.l   #4,d2
        move.l  d2,d1
        asl.l   #4,d1
        sub.l   d2,d1                   ; z2 * 240
        cmp.l   d1,d0
        bgt     xp_cull
        move.l  d4,d0                   ; (|cv1| - r) * fy  >  z2 * 48 ?
        bpl     xp_7
        neg.l   d0
xp_7:   sub.l   d6,d0
        moveq   #0,d1
        move.w  38(a0),d1
        bsr     xp_mul
        move.l  d7,d2
        asl.l   #4,d2
        move.l  d7,d1
        asl.l   #5,d1
        add.l   d2,d1                   ; z2 * 48
        cmp.l   d1,d0
        bgt     xp_cull
        cmp.b   #2,(a1)                 ; OBJ_SOLID: r * fx < cv2 ?  (menos de un pixel)
        bne     xp_ok
        move.l  d6,d0
        moveq   #0,d1
        move.w  36(a0),d1
        bsr     xp_mul
        cmp.l   d5,d0
        blt     xp_cull
xp_ok:  moveq   #1,d0
        bra     xp_ret
xp_cull:
        moveq   #0,d0
xp_ret: movem.l (a7)+,d2-d7/a2-a4
        rts

; d0 = a (32 bits con signo), d1 = b (0..32767)  ->  d0 = a*b exacto (dos MULU). Conserva todo salvo d0/d1.
xp_mul: movem.l d2-d3,-(a7)
        moveq   #0,d2
        tst.l   d0
        bpl     xm_1
        neg.l   d0
        moveq   #1,d2
xm_1:   move.l  d0,d3
        swap    d3
        mulu    d1,d3
        swap    d3
        clr.w   d3
        mulu    d1,d0
        add.l   d3,d0
        tst.w   d2
        beq     xm_2
        neg.l   d0
xm_2:   movem.l (a7)+,d2-d3
        rts
