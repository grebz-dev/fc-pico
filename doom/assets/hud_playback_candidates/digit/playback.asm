 .inesprg 2
 .ineschr 1
 .inesmap 0
 .inesmir 1
 .bank 0
 .org $8000
reset:
 sei
 cld
 ldx #$40
 stx $4017
 ldx #$ff
 txs
 ldx #$00
 stx $2000
 stx $2001
 stx $4010
 stx frame_count
 stx state
 stx pending
wait1:
 bit $2002
 bpl wait1
wait2:
 bit $2002
 bpl wait2
 jsr upload_state
 lda #$88
 sta $2000
 lda #$1e
 sta $2001
main:
 lda pending
 beq main
 lda #$00
 sta $2001
 jsr upload_state
 lda #$00
 sta $2005
 sta $2005
 sta pending
 lda #$88
 sta $2000
 lda #$1e
 sta $2001
 jmp main
nmi:
 pha
 txa
 pha
 tya
 pha
 inc frame_count
 lda frame_count
 cmp #60
 bne nmi120
 lda #1
 sta state
 sta pending
nmi120:
 lda frame_count
 cmp #120
 bne nmi_done
 lda #2
 sta state
 lda #1
 sta pending
nmi_done:
 pla
 tay
 pla
 tax
 pla
 rti
irq:
 rti
upload_state:
 lda #$20
 sta $2006
 lda #$00
 sta $2006
 ldx state
 lda nt_lo,x
 sta $00
 lda nt_hi,x
 sta $01
 ldx #4
 ldy #0
copy_page:
 lda [$00],y
 sta $2007
 iny
 bne copy_page
 inc $01
 dex
 bne copy_page
 lda #$3f
 sta $2006
 lda #$00
 sta $2006
 ldx #0
palette_loop:
 lda palettes,x
 sta $2007
 inx
 cpx #$20
 bne palette_loop
 ldx state
 lda oam_lo,x
 sta $00
 lda oam_hi,x
 sta $01
 ldy #0
copy_oam:
 lda [$00],y
 sta $0200,y
 iny
 bne copy_oam
 lda #0
 sta $2003
 lda #$02
 sta $4014
 lda #0
 sta $2005
 sta $2005
 rts
nt_lo:
 .db LOW(nt0),LOW(nt1),LOW(nt2)
nt_hi:
 .db HIGH(nt0),HIGH(nt1),HIGH(nt2)
oam_lo:
 .db LOW(oam0),LOW(oam1),LOW(oam2)
oam_hi:
 .db HIGH(oam0),HIGH(oam1),HIGH(oam2)
frame_count = $10
state = $11
pending = $12
 .bank 1
 .org $a000
nt0:
 .incbin "state-1.nt"
nt1:
 .incbin "state-2.nt"
nt2:
 .incbin "state-3.nt"
oam0:
 .incbin "state-1.oam"
oam1:
 .incbin "state-2.oam"
oam2:
 .incbin "state-3.oam"
palettes:
 .incbin "palettes.bin"
 .bank 3
 .org $fffa
 .dw nmi
 .dw reset
 .dw irq
 .bank 4
 .org $0000
 .incbin "chr.bin"
