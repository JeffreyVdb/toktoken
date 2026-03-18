; Boot and initialization routines
; for the OOP engine subsystem
.include "src/config/config.inc"

.define maxNumberOopObjs 48
.define oopStackTst $aa55
.def oopStackObj.length _sizeof_oopStackObj

.ENUM 0 export
OBJR_noErr db
OBJR_kill db
.ende

.STRUCT oopStackObj
flags       db
id          db
num         dw
properties  dw
dp          dw
init        dw
play        dw
kill        dw
.ENDST

.ramsection "global vars" bank 0 slot 2
  STACK_strt dw
  STACK_end dw
.ends

.section "oophandler"

;clear oop stack
core.object.init:
    php
    phd
    rep #$31
    lda #ZP
    tcd
    lda #0
    ldy #OopStackEnd-OopStack
    ldx #OopStack
    jsr ClearWRAM
    pld
    plp
    rts

;in:y=number of object to create, a:call parameter x:pointer
core.object.create:
    php
    phd
    rep #$31
    pha
    tdc
    pea ZP
    pld
    rts

.ends

.section "ScummVM dispatch" superfree

scummvm.executeOpcode:
    rep #$31
    jsr _fetchByte
    sta SCUMM.currentOpcode
    rts

_fetchByte:
    sep #$20
    lda [SCUMM.scriptPtr]
    rep #$20
    and #$00FF
    rts

.ends

.macro CLASS
.redefine __classid \1
T_CLSS_\@:
.db "\1", 0
.endm

.macro METHOD
.redefine __method \1
_\1:
.endm

.macro SCRIPT
\1:
.dw $BADE
.endm
