::/ @file upload.bat
::/ @brief Flashes the MP3 archive to the board and reboots it.
::/ @ingroup toolchain
::/
::/ Writes `res2.bin` to `0x10200000`, which is #RES_DATA_ADR, then reboots. The
::/ firmware UF2 is uploaded separately.
::/
::/ @warning Nothing checks that the firmware image has not grown past that
::/          address. @see @ref generated_resources
..\..\bin\picotool load -o 0x10200000 -t bin res2.bin
..\..\bin\picotool reboot
