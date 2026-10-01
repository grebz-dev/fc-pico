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
 inx
 stx $2000
 stx $2001
 stx $4010
wait1:
 bit $2002
 bpl wait1
wait2:
 bit $2002
 bpl wait2
 lda #$20
 sta $2006
 lda #$00
 sta $2006
 ldx #$00
copy0:
 lda nametable,x
 sta $2007
 inx
 bne copy0
copy1:
 lda nametable+$100,x
 sta $2007
 inx
 bne copy1
copy2:
 lda nametable+$200,x
 sta $2007
 inx
 bne copy2
copy3:
 lda nametable+$300,x
 sta $2007
 inx
 bne copy3
 lda #$3f
 sta $2006
 lda #$00
 sta $2006
 ldx #$00
copy_pal:
 lda palettes,x
 sta $2007
 inx
 cpx #$20
 bne copy_pal
 ldx #$00
copy_oam:
 lda oam_data,x
 sta $0200,x
 inx
 bne copy_oam
 lda #$00
 sta $2003
 lda #$02
 sta $4014
 lda #$00
 sta $2005
 sta $2005
 lda #$08
 sta $2000
 lda #$1e
 sta $2001
forever:
 jmp forever
nmi:
 rti
irq:
 rti
 .bank 1
 .org $a000
nametable:
 .incbin "nametable.bin"
palettes:
 .incbin "palettes.bin"
oam_data:
 .incbin "oam.bin"
 .bank 3
 .org $fffa
 .dw nmi
 .dw reset
 .dw irq
 .bank 4
 .org $0000
 .incbin "chr.bin"
