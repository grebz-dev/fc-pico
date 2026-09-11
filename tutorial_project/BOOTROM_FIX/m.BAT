::/ @file m.BAT
::/ @brief Assembles the permanent boot ROM bank and cuts it out for inclusion.
::/ @ingroup toolchain
::/
::/ Must run **before** `BOOTROM/m.BAT`. Extracts 4096 bytes from offset `0x7010`
::/ -- the iNES header plus `$F000` -- straight into `../BOOTROM/bootrom_fixr.bin`.
::/
::/ @see @ref build_pipeline, @ref boot_reflash
@echo off

del /F *.nes 

.\bin\nesasm -s PG_main.asm > nesasm.log

.\bin\nesasm_logfilter nesasm.log
find "error" nesasm.log

.\bin\bin_catcut PG_main.NES ..\BOOTROM\bootrom_fixr.bin -new -r_offset 0x7010 -size 0x1000 > NUL
