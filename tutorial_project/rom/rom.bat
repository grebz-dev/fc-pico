::/ @file rom.bat
::/ @brief Flashes the cartridge over a kazzo programmer.
::/ @ingroup toolchain
::/
::/ `anago Ffe nrom_wx.af rom.nes AM29F040B AM29F040B` -- flash mode, full transfer
::/ on both buses, NROM board description, device named for each bus.
::/
::/ @note Only needed for a blank cartridge or for recovery. Normal updates go
::/       through the in-band self-reflash. @see @ref flashing
anago Ffe nrom_wx.af rom.nes AM29F040B AM29F040B
