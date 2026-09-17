@page dev_setup Development environment

## What you need

| For | Tool |
|---|---|
| RP2350 firmware | Arduino IDE + the arduino-pico core (Earle Philhower) |
| 6502 boot ROM | nothing -- `nesasm` is checked in under `BOOTROM*/bin/` |
| Music | nothing -- `nsc.exe` is checked in under `mml/bin/` |
| Cartridge flashing | kazzo programmer (only for out-of-band flashing) |
| Documentation | Doxygen 1.18+, Graphviz, Python 3 |

impact soft publishes setup walkthroughs from the product page; see @ref references.

## Firmware

Install the arduino-pico core through the Arduino IDE board manager, select a Pico 2 /
RP2350 board, and open `tutorial_project/tuto1_hw/tuto1_hw.ino`.

The sketch is a unity build -- `system.h` includes every `.cpp` in `sys/`. If you add a
source file there you must add it to `system.h` as well, and it must come **before**
`rp_core0.h` / `rp_core1.h`, which reference the global instances defined at the bottom
of the other files. See @ref architecture.

Libraries used beyond the core: `EEPROM`, `BackgroundAudio`, `PWMAudio`.

Deploy by holding **BOOT** while connecting USB-C and copying the UF2 onto the drive
that appears.

## Serial debugging

`Serial.begin(115200)` in `setup()`, with a 1800 ms settle before anything else runs, so
you have time to attach a terminal before the first output.

@warning Printing from anything on the bus path -- rp_system::jobRcvCom() and everything
it calls -- risks overrunning the 6502's 1.3 ms spin-loop handshake and desynchronising
the frame stream. Log outside the interrupt where you can.

The 6502 has its own channel: #FP_COM_LOG sends up to seven bytes to the cartridge,
which prints them. See `setDebugLog` and `setErrorLog` in `SysPico.asm`.

## Tracing and the watchdog

rp_debug.h provides a 16-entry ring trace and asserts:

```cpp
TRACE(DTR_MAIN);          // record { trace id, __LINE__ }
ASSERT(ptr != NULL);      // print and halt
```

Uncomment `#define NDEBUG 1` at the top of rp_debug.h to compile both away.

The watchdog resets the board after 5000 ms. Core 0 feeds it from `loop()`; core 1 feeds
it on core 0's behalf via `WDT_check()` when the bus is idle. `WDT_mode` selects which:
mode 1 means the console link is live and core 0 must feed the dog itself, so a stall
there reboots rather than hangs.

`watchdog_caused_reboot()` is reported at startup -- check the serial log for
`Rebooted by Watchdog!` after an unexplained restart.

## Editing the sources

All sources are UTF-8. Several were Shift-JIS until recently, and the original Japanese
comments have been preserved alongside the English documentation. Configure your editor
for UTF-8 and do not let it re-encode on save. See @ref conventions.

## Building the documentation

```
build_docs.bat
```

Requires `doxygen` on PATH (or installed at `C:\Program Files\doxygen`), plus `dot` and
`python`. Output lands in `docs/html/index.html`; the script fails if Doxygen emitted
any warning.

To confirm a documentation change touched only comments:

```
python tools/doxygen/check_code_unchanged.py
```

## A safe first change

`ap_title.cpp` is the smallest useful edit target. It draws three strings and a frame
counter, and demonstrates key repeat, music and sound triggers. Changing a `drawString`
call there and reflashing exercises the whole pipeline end to end.
