@echo off
..\bin\nesasm -s %1.asm > NUL
..\bin\bin_catcut %1.nes tmp.nam -new -size 0x400 -r_offset 0x10 > NUL
del %1.nes /f
..\bin\bpe_fc -t -e tmp.nam %1.bpe
del tmp.nam /f
