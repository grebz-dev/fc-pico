copy rom.nes ..\ROM\*.*

.\bin\bin_catcut rom.nes rom.bin -new -r_offset 0x0010 -size 0x7000 > NUL
.\bin\bin2c rom.bin rom.c _rom

del /f rom.bin
del /f rom.c
