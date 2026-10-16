; SPDX-License-Identifier: BSD-3-Clause
; Assembles only if the generated include is valid nesasm and agrees with the
; tutorial's spellings and values. Built by test_generated.py.
	.inesprg 1
	.ineschr 0
	.inesmap 0
	.include "fcbus_protocol.inc"
	.bank 0
	.org $C000
	.if PF_MAGIC_CODE <> $FC
	.fail
	.endif
	.if PPU_COUNT_VAL <> 15490
	.fail
	.endif
	.if FP_COM_INI <> $FF
	.fail
	.endif
	.db PF_COM_VRAM, FP_COM_VER, KEY_A
