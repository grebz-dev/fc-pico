@echo off

set yyyy=%date:~0,4%
set yy=%date:~2,2%
set mm=%date:~5,2%
set dd=%date:~8,2%
 
set time2=%time: =0%
 
set hh=%time2:~0,2%
set mn=%time2:~3,2%
set ss=%time2:~6,2%

set title=FC_PICO


echo 	DB "%yyyy%-%mm%%dd%-%hh%%mn%",0 > dbdate.h

del /F *.nes 

.\bin\nesasm -s PG_main.asm > nesasm.log

copy PG_main.NES rom.NES > NUL


.\bin\nesasm_logfilter nesasm.log
find "error" nesasm.log
