@echo off

..\bin\spchr_cnv OBJ.chr OBJ_SP.chr
rem ..\bin\bin_catcut OBJ_SP.chr .\div\OBJ_SP_0.chr -new -r_offset 0x000 -size 0xC00 > NUL


call bpe_asm NamLicense0
call bpe_asm NamLicense1
