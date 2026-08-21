::/ @file update.bat
::/ @brief Copies the assembled ROM to the flasher directory. Largely vestigial.
::/ @ingroup toolchain
::/
::/ Only the first line still has an effect: it puts `rom.nes` where `rom/rom.bat`
::/ expects it. The `bin_catcut` + `Bin2C` pair below strips the 16-byte iNES
::/ header, takes `0x7000` bytes (`$8000`-`$EFFF`) and writes `_rom[28672]` into
::/ `rom.c` -- which the next two lines then delete. Nothing consumes it.
::/
::/ The ROM reaches the firmware through `tuto1_hw/res/conv.bat` instead, which
::/ packs `rom.NES` whole -- header and all -- into the resource archive.
::/
::/ @note `..\ROM\` resolves to `..\rom\` on Windows' case-insensitive
::/       filesystem, so the copy does land in the right place.
::/ @see @ref build_pipeline, @ref generated_resources
copy rom.nes ..\ROM\*.*

.\bin\bin_catcut rom.nes rom.bin -new -r_offset 0x0010 -size 0x7000 > NUL
.\bin\bin2c rom.bin rom.c _rom

del /f rom.bin
del /f rom.c
