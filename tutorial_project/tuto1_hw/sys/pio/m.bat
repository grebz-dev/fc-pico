::/ @file m.bat
::/ @brief Regenerates fcppu.pio.h from fcppu.pio.
::/ @ingroup toolchain
::/
::/ Run this after editing any PIO program. The firmware includes the generated
::/ header, not the `.pio` source, so a change that is not regenerated will not
::/ take effect.
::/
::/ @warning The checked-in `fcppu.pio.h` was generated from a slightly earlier
::/          revision than `fcppu.pio` -- the instruction order in ::fcppu_dir
::/          differs between them. **`fcppu.pio` is authoritative.**
::/          @see @ref generated_resources
pioasm.exe fcppu.pio fcppu.pio.h
