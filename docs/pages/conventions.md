@page conventions Source conventions

## Encoding

**Every source file in this repository is UTF-8 without BOM.**

Historically 32 of them were Shift-JIS (CP932): all of `BOOTROM/`, `BOOTROM_FIX/` and
`mml/`, plus `Canvas.h`, `Obj3d.h`, `rp_debug.h` and `rp_sound.h`. They rendered as
mojibake in any UTF-8 editor, which is why the tree was converted.

@warning Configure your editor for UTF-8 and check that it does not re-encode on save.

The conversion was validated by rebuilding every artifact and byte-comparing:

| Artifact | Result |
|---|---|
| `BOOTROM_FIX/PG_main.nes` | identical |
| `BOOTROM/bootrom_fixr.bin` | identical |
| `BOOTROM/rom.NES` | identical (with `dbdate.h` pinned) |
| `mml/sound.nsf` | identical |

Both `nesasm` and `nsc` accept UTF-8 comments without complaint.

### The one code change

`mml/env_set0.mmh` used full-width `｛ ｝` as live MML syntax on the frequency-envelope
line, while every other envelope in the file used ASCII. Those two characters were
normalised:

```
- E(3000)｛ 0 D12 L 1 2 3 2 1 0 -1 -2 -3 -2 -1 0 ｝
+ E(3000){ 0 D12 L 1 2 3 2 1 0 -1 -2 -3 -2 -1 0 }
```

The rebuilt `sound.nsf` is byte-identical, so `nsc` treats the two forms as equivalent.

### Generated files are never transcoded

`BOOTROM*/PG_main.lst` and `nesasm.log` are assembler output. nesasm copies source bytes
into the listing without interpreting them, so a listing simply inherits whatever
encoding its sources had at build time: they were CP932 when the sources were CP932, and
they are **UTF-8 now**. They are tracked, so a build will show them as modified; that is
expected and harmless.

@warning Never include generated files in an encoding conversion. Running a
CP932-to-UTF-8 pass over a tree that has been rebuilt since the last one re-encodes the
already-UTF-8 listings a second time, and the result is **irrecoverable by script** --
half-width katakana collapse during the round trip, so `A を押し` comes back from a
reverse conversion as `A を押ぁE`. Recovery is `git checkout` or a rebuild, nothing else.
Any conversion script must exclude `*.lst`, `*.log` and `nesasm.log`.

`tools/doxygen/check_code_unchanged.py` skips these by extension (`GENERATED`) and
reports the count, so a generated file can no longer slip past the gate unexamined.

## Documentation comments

### C and C++

Standard Doxygen. Japanese authoring comments are left in place; English blocks are
added above them rather than replacing them.

```cpp
/**
 * @brief One line, imperative.
 * @param x What it means.
 * @return What comes back.
 * @warning A way to get this wrong.
 */
```

### 6502, PIO, MML, batch

Doxygen cannot parse these, so `tools/doxygen/doxyfilter.py` shadows them into C-like
declarations. **Only comments with the explicit marker are promoted**; ordinary `;`
comments are dropped, which is what keeps the original Japanese notes out of the English
reference.

| Dialect | Marker | Trailing form |
|---|---|---|
| nesasm, PIO | `;///` | `;///<` |
| batch | `::/` | `::/<` |
| MML | `///` (already C-style) | `///<` |

```asm
;/// @brief Reads eight mailbox bytes into a zero-page block.
;/// @ingroup bootrom
RCV_PICO_BUF	MACRO

SRC_ADR   EQU  $08   ;///< Generic 16-bit source pointer.
```

### What the filter synthesises

| Construct | Becomes |
|---|---|
| `LABEL:` | `void LABEL(void);` |
| `NAME EQU $12` | `#define NAME 0x12` |
| `NAME MACRO` | `#define NAME(...)` |
| `.include "x.asm"` | `#include "x.asm"` |
| `.define public N v` | `#define N v` |
| `.program name` | `void name(void);` |
| `BGM(1)` | `void BGM_1(void);` |
| `$phrase{` | `void phrase(void);` |

Local labels (`.name`) are deliberately skipped -- they are branch targets, not API.

The filter emits **exactly one line per input line**, so declarations keep the line
numbers of the constructs they shadow. `FILTER_SOURCE_FILES = NO` means the source
browser shows the real files, not the shadow.

## Module groups

Declared once in `docs/pages/modules.dox`; sources only `@ingroup` them.

| Group | Covers |
|---|---|
| `fcbus` | Bus interface, PIO, DMA, protocol |
| `platform` | Bring-up, dual-core, watchdog, tracing |
| `graphics` | Canvas, ArduinoGL, Obj3d |
| `audio` | Sound driver, NSF player, 6502 emulator |
| `app` | Sample application |
| `bootrom` | 6502 code |
| `toolchain` | Build scripts and converters |

## House rules

- **Document the mechanism, not the syntax.** `@brief Sets m_step` is worthless;
  say what changing it causes.
- **`@warning` is for things that will actually bite.** Torn mailboxes, the mandatory
  dummy read, the 24-writes-per-frame APU cap, the three copies of the protocol
  constants.
- **Mark vestigial code as vestigial**, so nobody chases it. See below.
- **Cross the language boundary with `@see`.** rp_system::setPF_APU() and the replay
  loop in @ref NMI are one mechanism in two languages.

## Vestigial code

Present, assembled, and doing nothing:

| What | Where | Status |
|---|---|---|
| FC-EXA / ESP32 channel at `$5000` | `SysArduino.asm` | Only `setWRAM_BANK` is still called |
| `BOOT_EXA` probe | `SysNES.asm` | Detects the absent adapter, then proceeds |
| `HIRQ_*`, `EXS_HIRQ_REG` | `macro.h` | Scanline IRQ hardware not present |
| `CHG_BANK_*`, `PUSH_BANK`, `POP_BANK` | `macro.h` | Dead on an NROM cartridge |
| `KEY_RTN2` | `SysKey.asm` | Controller 2, inside `.if 0` |
| `ROM_MIRROR` | `macro.h` | Emits a marker for `nes_mirror.exe`, never run |
| `memcpyDMA`, `memcpyDMA32` | `rp_dma.cpp` | Marked "under verification"; leak a channel per call |
| `initSM_DMA_RX` | `rp_dma.cpp` | Marked "under construction" |
| MP3 music path | `rp_sound.cpp`, `ap_data.cpp` | Reachable at run time, but `setMP3data()` has its body commented out |
| 3D pipeline | `ArduinoGL.cpp` | `glUseCanvas()` never called |
| `bpe_fc`, `inesAlterer`, `nes_mirror`, `spchr_cnv` | `bin/` | Not invoked by any script under `tuto1_hw/` |
| `sound_nsf` | `ap_data.h` | Declared, never defined or referenced |
| `setModelDataObj()` | `ap_data.cpp` | Reduced to `obj->init()`; no model data ships |

## Duplication to keep in sync

- `macro.h` is byte-identical in `BOOTROM/` and `BOOTROM_FIX/`.
- The protocol opcodes exist in three places -- see @ref protocol.
- `SysEqu.h` differs between the two banks; the fix-bank version is deliberately
  reduced, and its header notes that everything except key state may be destroyed after
  boot.
