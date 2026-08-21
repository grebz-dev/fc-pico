::/ @file m.BAT
::/ @brief Assembles the erasable boot ROM bank.
::/ @ingroup toolchain
::/
::/ Regenerates `dbdate.h` with the current timestamp, runs `nesasm -s`, filters
::/ the log and copies the result to `rom.NES`. The permanent bank must already
::/ have been built, because `PG_main.asm` includes `bootrom_fixr.bin`.
::/
::/ @warning The timestamp is rewritten on **every** run. It ends up at `$EFF0`
::/          and is compared against the cartridge's copy at boot, so rebuilding
::/          here without also running `update.bat` and reflashing the firmware
::/          makes every boot trigger a self-reflash.
::/          @see @ref build_pipeline, @ref boot_reflash
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
