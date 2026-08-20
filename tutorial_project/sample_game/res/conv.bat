..\..\bin\stl_converter fcpico

call stlmake.bat mdl_player
call stlmake.bat mdl_enemy
call stlmake.bat mdl_enemy2
call stlmake.bat mdl_enemy3
call stlmake.bat mdl_bullet


call bpe_asm NamLicense0
call bpe_asm NamLicense1

..\..\bin\binlink binlink.lst res_id.h res.bin  0
..\..\bin\binlink binlink2.lst res_id2.h res2.bin 10000
..\..\bin\bin2c res.bin resdata.c _resdata
