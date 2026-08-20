@echo off

set title=map0demo
set vr=V1.00


echo 	DRAW_STRING2 " 2026 %vr%" > strver.h

del /F *.nes 

.\bin\nesasm -s PG_main.asm > nesasm.log

copy PG_main.NES %title%.NES > NUL

.\bin\bin_catcut %title%.NES gamerom.bin -new -r_offset 0x0010 -size 0x8000


type nesasm.log
find "error" nesasm.log
