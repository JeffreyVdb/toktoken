; CA65 (cc65 assembler for 6502/NES)
; PPU initialization and interrupt handlers

.segment "CODE"

.proc main
    sei
    cld
    ldx #$FF
    txs
    lda #$00
    sta $2000
    sta $2001
    jsr init_ppu
    rts
.endproc

; IRQ handler -- services hardware interrupts
.proc irq_handler
    pha
    txa
    pha
    tya
    pha

    lda $4015
    ; acknowledge IRQ

    pla
    tay
    pla
    tax
    pla
    rti
.endproc

.proc nmi_handler
    pha
    lda #$00
    sta $2005
    sta $2005
    pla
    rti
.endproc

.define SCREEN_WIDTH 32
.define SCREEN_HEIGHT 30
PPU_CTRL = $2000
PPU_MASK = $2001
PPU_STATUS = $2002

.macro set_ppu_addr addr
    lda #.hibyte(addr)
    sta $2006
    lda #.lobyte(addr)
    sta $2006
.endmacro

.macro wait_vblank
    bit PPU_STATUS
:
    bit PPU_STATUS
    bpl :-
.endmacro

.segment "VECTORS"
.segment "CHARS"
