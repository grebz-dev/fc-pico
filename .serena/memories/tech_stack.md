# Tech stack

- Active port: C11/C++/6502 assembly with CMake >=3.20 and Ninja. `FCPICO_HOST_ONLY=ON` builds host libraries and CTest without pico-sdk; the device build targets RP2350.
- Device pins: pico-sdk 2.1.1; `PICO_BOARD=fcpico`; `PICO_PLATFORM=rp2350-arm-s`; Arm GNU `arm-none-eabi-gcc` 13.2.Rel1; picotool 2.1.1. Device CMake imports pico-sdk from `doom/rp2040-doom/pico_sdk_import.cmake`, then `$PICO_SDK_PATH`, then CMake's `PICO_SDK_PATH`.
- Boot ROM assembly uses native NESASM CE at commit `6fc41cda37b934aa29aa2639d0baa74424268e31`; MesenCE co-simulation uses .NET 10 and the pinned `doom/sim/mesen2/Mesen2` submodule.
- Python 3.11+ host tools/tests use numpy, pillow, pytest, py65, rp2040-pio-emulator and adafruit-circuitpython-pioasm, listed in `doom/tools/requirements.txt`.
- `doom/tools/setup_env.sh` is an idempotent Ubuntu 24.04 provisioning script that installs apt dependencies, Python requirements, SDK, compiler and picotool. It uses network and privileged installs; run only when provisioning a real host.