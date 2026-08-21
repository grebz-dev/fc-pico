::/ @file conv.bat
::/ @brief Packs the binary assets into one archive and emits it as a C array.
::/ @ingroup toolchain
::/
::/ `binlink` concatenates every file listed in `binlink.lst` -- the boot ROM, the
::/ music, the sprite sheet and the font -- into `res.bin`, prefixed by an index
::/ table of offset/size pairs, and writes `res_id.h` with one `#define` per entry.
::/ The identifier is built from the file name as `<EXT>_<BASENAME>`, uppercased,
::/ so `rom.NES` becomes `NES_ROM`; renaming an input renames the constant.
::/ `bin2c` then wraps `res.bin` as `_resdata` for ap_data.cpp to include.
::/
::/ The trailing `0` is the id base. It is the archive selector getResHead()
::/ decodes: a separately flashed archive is built with `10000` here and lands at
::/ #RES_DATA_ADR.
::/
::/ @warning The boot ROM and the music are pulled from `BOOTROM/` and `mml/`, so
::/          rebuild those first; `binlink` reads whatever is on disk.
::/
::/ @see @ref generated_resources
..\..\bin\binlink binlink.lst res_id.h res.bin  0
..\..\bin\bin2c res.bin resdata.c _resdata
