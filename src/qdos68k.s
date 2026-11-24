; qdos68k.s - llamadas a QDOS para QFS (sintaxis as68 / XTC68)
;   _plat_keyrow    KEYROW via IPC (MT.IPCOM)
;   _plat_mode4     MT.DMODE modo 4
;   _plat_timer_on / _plat_timer_off   tarea de sondeo de 50 Hz (MT.LPOLL/RPOLL)

        .text
        .globl  _plat_keyrow
        .globl  _plat_mode4
        .globl  _plat_timer_on
        .globl  _plat_timer_off
        .globl  _plat_tickcnt
        .globl  _ql_sysvars

; ---------------------------------------------------------------- QDOS
; long ql_sysvars(void): MT.INF, A0 = variables del sistema (para leer variables de SuperBASIC, qlvars.h)
_ql_sysvars:
        movem.l d2-d7/a2-a6,-(a7)
        moveq   #0,d0
        trap    #1
        move.l  a0,d0
        movem.l (a7)+,d2-d7/a2-a6
        rts

; int plat_keyrow(int row)  -> bits de la fila (1 = pulsada)
_plat_keyrow:
        move.l  4(a7),d0
        movem.l d2-d7/a2-a5,-(a7)
        lea     kr_cmd,a3
        move.b  d0,6(a3)
        moveq   #$11,d0                 ; MT.IPCOM
        trap    #1
        moveq   #0,d0
        move.b  d1,d0
        movem.l (a7)+,d2-d7/a2-a5
        rts

; void plat_mode4(void)
_plat_mode4:
        movem.l d2-d7/a2-a5,-(a7)
        moveq   #$10,d0                 ; MT.DMODE
        moveq   #0,d1                   ; modo 4 (512 pixeles)
        moveq   #-1,d2                  ; tipo de monitor sin cambios
        trap    #1
        movem.l (a7)+,d2-d7/a2-a5
        rts

; void plat_timer_on(void) / plat_timer_off(void)
_plat_timer_on:
        movem.l d2-d7/a2-a5,-(a7)
        lea     tm_link,a0
        lea     tm_poll,a1
        move.l  a1,4(a0)
        moveq   #$1C,d0                 ; MT.LPOLL
        trap    #1
        movem.l (a7)+,d2-d7/a2-a5
        rts

_plat_timer_off:
        movem.l d2-d7/a2-a5,-(a7)
        lea     tm_link,a0
        moveq   #$1D,d0                 ; MT.RPOLL
        trap    #1
        movem.l (a7)+,d2-d7/a2-a5
        rts

; se ejecuta 50 veces por segundo en modo supervisor
tm_poll:
        addq.l  #1,_plat_tickcnt
        rts

        .data
kr_cmd: dc.b    $09,$01,$00,$00,$00,$00,$00,$02     ; IPC 9 = KEYROW, 1 parametro
        even
tm_link:
        dc.l    0,0
_plat_tickcnt:
        dc.l    0
