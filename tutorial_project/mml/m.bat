::/ @file m.bat
::/ @brief Compiles the MML sources into `sound.nsf`.
::/ @ingroup toolchain
::/
::/ Runs the NSD.Lib compiler, which links the driver from `bin/nsd.bin`. Re-run
::/ `tuto1_hw/res/conv.bat` afterwards to embed the result in the firmware.
::/
::/ @see @ref audio_page, @ref build_pipeline
.\bin\nsc -N sound.mml
