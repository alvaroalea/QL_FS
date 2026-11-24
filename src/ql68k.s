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
;   (las llamadas a QDOS estan en qdos68k.s)

        .text

        .globl  _fxmul
        .globl  _gfx_hspan_raw
        .globl  _gfx_line_raw
        .globl  _gfx_memcpy
        .globl  _xform3

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
fm_2:   move.l  d0,d2
        swap    d2                      ; d2.w = ah
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

; ---------------------------------------------------------------- hspan
; void gfx_hspan_raw(u8 *row, s32 x0, s32 x1, s32 pat)
; pat = palabra G<<8 | R (8 pixeles); x0 <= x1, ya recortados
_gfx_hspan_raw:
        movem.l d2-d7,-(a7)             ; 24 bytes
        move.l  28(a7),a0
        move.l  32(a7),d0               ; x0
        move.l  36(a7),d1               ; x1
        move.l  40(a7),d2               ; patron
        move.l  d0,d3
        lsr.l   #3,d3
        add.l   d3,d3                   ; desplazamiento de la primera palabra
        adda.l  d3,a0
        move.l  d1,d4
        lsr.l   #3,d4
        add.l   d4,d4
        sub.l   d3,d4
        lsr.l   #1,d4                   ; palabras despues de la primera
        moveq   #7,d6
        and.w   d0,d6
        move.w  #$00FF,d5
        lsr.w   d6,d5                   ; m0
        move.w  d5,d6
        lsl.w   #8,d6
        or.w    d6,d5                   ; m0 en G y R
        moveq   #7,d7
        and.w   d1,d7
        eor.w   #7,d7                   ; 7 - (x1 & 7)
        move.w  #$00FF,d6
        lsl.w   d7,d6
        and.w   #$00FF,d6               ; m1
        move.w  d6,d7
        lsl.w   #8,d7
        or.w    d7,d6                   ; m1 en G y R
        tst.l   d4
        bne     hs_multi
        and.w   d6,d5                   ; una sola palabra: m0 & m1
        move.w  (a0),d7
        move.w  d5,d3
        not.w   d3
        and.w   d3,d7
        move.w  d2,d3
        and.w   d5,d3
        or.w    d3,d7
        move.w  d7,(a0)
        bra     hs_end
hs_multi:
        move.w  (a0),d7                 ; primera palabra (m0)
        move.w  d5,d3
        not.w   d3
        and.w   d3,d7
        move.w  d2,d3
        and.w   d5,d3
        or.w    d3,d7
        move.w  d7,(a0)+
        subq.l  #1,d4                   ; palabras completas
        bra     hs_chk
hs_loop:
        move.w  d2,(a0)+
hs_chk:
        dbra    d4,hs_loop
        move.w  (a0),d7                 ; ultima palabra (m1)
        move.w  d6,d3
        not.w   d3
        and.w   d3,d7
        move.w  d2,d3
        and.w   d6,d3
        or.w    d3,d7
        move.w  d7,(a0)
hs_end:
        movem.l (a7)+,d2-d7
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

