@echo off

del /F *.nes 

.\bin\nesasm -s PG_main.asm > nesasm.log

.\bin\nesasm_logfilter nesasm.log
find "error" nesasm.log

.\bin\bin_catcut PG_main.NES ..\BOOTROM\bootrom_fixr.bin -new -r_offset 0x7010 -size 0x1000 > NUL
