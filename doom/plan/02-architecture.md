# 02 -- Architecture

## The system

```
 Famicom / NES                              FC PICO cartridge (RP2350)
 +------------------------------+           +----------------------------------------------+
 | 6502  Doom boot ROM (07)     |  $2007 wr | core 1                                        |
 |  NMI: read mailbox, write    |---------->|  fcbus ISR: heartbeat, re-arm DMA, key latch  |
 |       attrs/palette/APU,     |           |  LOW_PRIO_IRQ: compose 320x200x8 -> quantize  |
 |       send controller packet |           |             -> 256x200x2bpp + attrs + palette |
 |  main: controller read,      |  CHR bus  |             -> stream buffer swap             |
 |        commands, data mode   |<==========|  pd_core1_loop: visplanes + columns           |
 | PPU: fetches pattern stream  |  PIO+DMA  |  APU sequencer pump (I_UpdateSound)           |
 | APU: plays register writes   |           +----------------------------------------------+
 +------------------------------+           | core 0                                        |
                                            |  Doom game loop (35 Hz tics), BSP, column     |
                                            |  lists, menus, save/load, input mapper        |
                                            +----------------------------------------------+
                                            | flash: firmware | doom1.whx | assets | saves  |
                                            +----------------------------------------------+
```

Three processors, as in the FC PICO tutorial, but the roles shift: the console's 6502 does
slightly more per frame (attributes, palette, an explicit controller packet) and the RP2350's
core 1 becomes the "video card".

## Modules

| Module | Repo / path | Language | Responsibility |
|--------|-------------|----------|----------------|
| `fcbus` | fc-pico `doom/fcbus/` | C, pico-sdk | PIO programs (`fcppu.pio` verbatim), DMA channel, stream buffers, mailbox builder, receive dispatcher, frame-sync rule, data mode, boot ROM serving (`FP_COM_VER`/`FP_COM_ROM`), protocol v1 and v2. **Two backends**: `device` (PIO/DMA/IRQ) and `host` (byte-stream model, used by every simulation). |
| `fcvideo` | rp2040-doom `src/fcpico/` (engine-facing half) + fc-pico `doom/port/` (console-facing half) | C | 8-bit frame composition from RP2040 Doom's buffers and overlays; horizontal 5:4 decimation; block palette selection; ordered dither; bitplane packing; NES palette sets; letterboxing. |
| `fcinput` | rp2040-doom `src/fcpico/` | C | Controller packet -> Doom key events; mapping tables; menu navigation helpers; cheat combos. |
| `fcapu` | fc-pico `doom/port/apu/` + rp2040-doom `src/fcpico/` (module glue) | C | APU register sequencer: music streams, SFX scripts, DPCM triggers, priority and the per-frame write cap; `sound_fcpico_module`, `music_fcpico_module`. |
| `bootrom` | fc-pico `doom/bootrom/` | 6502 asm (nesasm 2.51+autozp) | Erasable-bank program for the console: v2 NMI, controller packet, data mode, DPCM sample bank, fixed entry points. |
| `port` | fc-pico `doom/port/` | C + CMake | `main()`, clock/voltage setup, flash layout header, resource archive (boot ROM image, music, SFX), watchdog, serial debug CLI. |
| `tools` | fc-pico `doom/tools/` | Python 3 | `ppu_decode.py` (stream -> PNG), `mus2apu.py`, `sfx2apu.py`, `dpcm_pack.py`, `flash_layout_check.py`, `gen_protocol.py` (one header -> 6502 `.inc` + Python constants), `update_goldens.py`. |
| `sim` | fc-pico `doom/sim/` | C, Python, C++ (Mesen2 mapper), TypeScript/Renode scripts | See 09. |

## One frame, end to end

1. **Vblank on the console.** The NMI handler (boot ROM v2) reads the 128-byte mailbox from
   the tail of the stream the PPU just finished pulling, writes the attribute table and (if
   flagged) the palette to VRAM, replays the APU pairs, restores scroll and PPU registers, then
   writes the controller packet `FP_COM_KEY, pad1, pad2` to `$2007`. (v1 boot ROM in Phase 1:
   64-byte mailbox, no attribute/palette blocks, raw controller byte.)
2. **Heartbeat on the cartridge.** `fcppu_w` captures each written byte; the RX-not-empty
   interrupt (`PIO0_IRQ_0`, core 1) runs `fcbus_isr()`: it reads the read counter
   (`fcppu_rna`), compares with the expected count, re-arms the DMA from the *current display
   buffer* if in phase (or stops it), copies the *next* mailbox onto the tail of that buffer,
   latches the controller bytes, increments `fcbus_frame_no`, and pends `LOW_PRIO_IRQ`.
   Elapsed: a few microseconds. It must never print or wait.
3. **Conversion, core 1, low-priority IRQ.** If RP2040 Doom has published a new frame
   (`render_frame_ready` available) since the last conversion, take it (`new_frame_stuff()`
   equivalent), compose 200 lines of 8-bit indices (main view from `frame_buffer`, overlays
   from the vpatch lists, or the single-screen/wipe/text variants), decimate to 256 columns,
   choose a sub-palette per 16x16 block, dither and pack into the *back* stream buffer,
   write the attribute table and palette into the *back* mailbox, then mark the back buffer
   as "display next". If no new frame, do nothing: the DMA keeps streaming the same buffer.
   Elapsed: 3-6 ms estimated; measured in P1-T8.
4. **Game logic, core 0.** Unchanged RP2040 Doom loop: `TryRunTics()`, `D_Display()`,
   `pd_begin_frame()`/`pd_end_frame()`; the input mapper feeds `D_PostEvent` from the latched
   controller bytes once per tic; `I_UpdateSound()` pumps the APU sequencer.
5. **Audio.** The sequencer emits at most 16 `(reg,value)` pairs into the next mailbox; the
   console replays them in the next NMI. Latency: one to two frames.

### Buffer ownership

| Buffer | Writer | Reader | Swap point |
|--------|--------|--------|------------|
| `frame_buffer[2]` (320x168x8) | core 0/1 renderer | converter (core 1, IRQ) | `render_frame_ready` / `display_frame_freed` semaphores (existing) |
| `line8[2]` (320 B) | converter | converter | internal |
| `stream[2]` (17,344 + 64 B each) | converter (back) | DMA (front) | `fcbus_publish()` marks back as next; the ISR swaps at the heartbeat |
| `mailbox[2]` (128 B) | converter (attrs, palette, flags), sequencer (APU pairs), command queue | ISR memcpy onto stream tail | with the stream |
| `pad_state` (2 B) | ISR | input mapper (core 0) | atomic byte reads |

The rule that keeps the picture torn-free: **the DMA source is only ever changed inside the
ISR, at a heartbeat, and the ISR only ever selects a buffer the converter has finished.**

## Core assignment and interrupt priorities

| Core | Context | Priority | Work | Max duration |
|------|---------|----------|------|--------------|
| 1 | `PIO0_IRQ_0` | highest of the app (0x40) | `fcbus_isr()` | < 20 us |
| 1 | `LOW_PRIO_IRQ` (31) | lowest (0xC0) | frame conversion | < 8 ms |
| 1 | thread | -- | `pd_core1_loop()`: visplanes, columns, `I_UpdateSound()` polling | per frame |
| 0 | thread | -- | game loop | per tic |
| 0 | timer alarm (SDK) | default | `time_us_64` only | -- |

Why the bus ISR on core 1 and not core 0 as in the tutorial: core 0 runs the game loop, which
must not take microsecond-scale jitter on flash-heavy code paths; core 1 is already the
display side in RP2040 Doom and already saves/restores the interpolator around IRQ work.
Consequence: `pio_set_irq0_source_enabled` and `irq_set_exclusive_handler` are called from
core 1 during its launch, before `scanvideo` would have been set up in the original.

The RP2040 Doom watchdog behaviour (`watchdog_reboot` in `restart()`) is kept; the FC PICO
dual-core watchdog handover is not needed because the heartbeat itself is the liveness signal:
if no heartbeat arrives for 2 s the firmware logs, stops the DMA and waits (the console is off
or in bulk mode).

## Memory map (RP2350, 150 MHz)

| Region | Contents |
|--------|----------|
| `0x20000000` - `__end__` (~230 KB) | RP2040 Doom statics (frame buffers, list buffer, decoders), `fcbus` buffers (~35 KB), converter tables (~20 KB), boot ROM shadow (none; served from flash) |
| `__end__` - `0x20070000` | zone heap (`I_ZoneBase`), capped by the 16-bit "short pointer" scheme at 256 KB |
| `0x20070000` - `0x20080000` | free; candidate for the core 1 stack and the stream buffers if statics grow past the short-pointer base check (see `AutoAllocMemory`) |
| `0x20080000`, `0x20081000` | scratch X / Y: core 1 / core 0 stacks per `memmap_doom.ld` (core 1 stack raised to 4 KB) |
| flash `0x10000000` | firmware image incl. boot ROM image, APU music/SFX data |
| flash `0x10080000` | `doom1.whx` (`TINY_WAD_ADDR`) |
| flash `0x10300000` | asset archive (DPCM originals for tooling, optional extra music) |
| flash top | save slots |

A single header `doom/port/flash_layout.h` defines these; `tools/flash_layout_check.py` fails
the build if the ELF's flash end crosses `TINY_WAD_ADDR` or the WHX crosses the archive.

## Boot sequence

1. Console power-on: fix bank `BR_INIT` -> sends `FP_COM_RST` -> `CHK_ROMVER` sends
   `FP_COM_VER` and compares 14 bytes. The firmware answers from the **Doom boot ROM image**
   compiled into the firmware, so a console still carrying the tutorial ROM (or an older Doom
   ROM) mismatches and reflashes itself through the unchanged fix-bank `ROM_UPDATE` path
   (`FP_COM_ROM` pages `$80`-`$EF`). This is the field-upgrade mechanism; the firmware must
   answer `FP_COM_VER` and `FP_COM_ROM` exactly as `rp_system::ver_dma()`/`rom_dma()` do.
2. Fix bank jumps to `MAIN_SETUP` (`$EF00`) -> Doom boot ROM init: clear VRAM, load a
   boot palette, send `FP_COM_INI, 0`, enable rendering, enter the main loop.
3. Firmware: on `FP_COM_INI` it resets `fcbus` state, publishes a "loading" test pattern,
   and releases the game to start (Doom shows its title screen).

The tutorial's "hold Start at power-on to force a reflash" behaviour is preserved because it
lives in the fix bank.

## What is deliberately not modelled

- The PPU address lines. The whole design assumes the fixed fetch order; the model in 09 does
  too.
- NES sprites and OAM. `$4014` is never written; `$2001` still enables sprites (so that the
  fetch pattern is the one the count was calibrated for -- **do not change PPUMASK bits
  without recalibrating**).
- Scrolling. Scroll is always (0,0); `$2005` is written every NMI to keep the address latch
  deterministic.

## API sketches

The seams between modules, as C prototypes. These are the contracts the tasks in 10 implement;
names are binding, signatures may grow.

### `fcbus/fcbus.h`

```c
typedef enum { FCBUS_PROTO_UNKNOWN = 0, FCBUS_PROTO_V1 = 1, FCBUS_PROTO_V2 = 2 } fcbus_proto_t;
typedef enum { FCBUS_ST_IDLE, FCBUS_ST_INIT, FCBUS_ST_RUN, FCBUS_ST_DATA } fcbus_state_t;

typedef struct {
    const uint8_t *rom_image;      // 32 KB PRG served to FP_COM_VER / FP_COM_ROM (no iNES header)
    fcbus_proto_t  proto_default;  // what to assume before the first packet (V1)
    void (*on_init)(uint8_t stage);          // FP_COM_INI received (thread context deferred)
    void (*on_heartbeat)(void);              // called from the ISR after the DMA decision
} fcbus_config_t;

typedef struct {
    uint32_t frames, resyncs, dma_stops, hb_timeouts, torn;
    uint32_t last_count;           // qualifying reads in the last frame
    uint16_t count_hist[9];        // VAL-4 .. VAL+4
    uint32_t isr_max_us;
} fcbus_stats_t;

void      fcbus_init(const fcbus_config_t *cfg);
void      fcbus_attach_irq(void);            // on the core that services PIO0_IRQ_0 (core 1)
fcbus_state_t fcbus_state(void);
fcbus_proto_t fcbus_proto(void);

// stream and mailbox: the converter writes the back buffer, then publishes
uint16_t *fcbus_stream_back(void);           // VRAM_BUF_BYTES_V2 bytes, word-addressed
uint8_t  *fcbus_mailbox_back(void);          // FC_COM_BUF_SIZE_V2 bytes, v1 layout in the first 64
static inline uint16_t *fcbus_stream_word(uint16_t *buf, int line, int tile)
    { return buf + VRAM_HEAD_WORDS + line * VRAM_LINE_WORDS + tile; }
bool      fcbus_back_is_free(void);          // false while a publish is pending
void      fcbus_publish(void);               // back becomes "next"; the ISR swaps at the heartbeat

// per-frame contributions from other modules (into the back mailbox)
bool      fcbus_apu_write(uint8_t reg, uint8_t val);   // false when the frame's pair cap is reached
bool      fcbus_cmd(uint8_t pf_com);                   // PF_COM_DMOD / FDIN / FDOT
bool      fcbus_cmd_vram(uint16_t addr, uint8_t val);  // PF_COM_VRAM poke (v1 path for attributes)
void      fcbus_attr_table(const uint8_t attr[64]);    // v2: whole table + ATTR_VALID
void      fcbus_palette(const uint8_t pal[16]);        // v2: BG palette + PAL_VALID

// inputs latched by the ISR
uint16_t  fcbus_pads(void);                  // (pad2 << 8) | pad1
uint32_t  fcbus_frame_no(void);

// bulk data mode (thread context only; blocks; DMA stopped meanwhile)
bool      fcbus_data_upload(uint16_t vram_addr, const uint8_t *data, uint16_t len);

const fcbus_stats_t *fcbus_stats(void);

// core (backend-independent), used by both backends and by tests
typedef enum { FCBUS_ARM, FCBUS_ARM_NUDGE1, FCBUS_ARM_NUDGE2, FCBUS_STOP } fcbus_sync_t;
fcbus_sync_t fcbus_sync_decide(uint32_t count, uint32_t expected);
void      fcbus_rx_byte(uint8_t b);          // the dispatcher: opcodes, packets, raw keys

// host backend only
int       fcbus_host_ppu_read(void);         // next byte the PPU would get; -1 when the DMA is stopped
void      fcbus_host_ppu_write(uint8_t b);   // a $2007 write; heartbeats run the ISR-equivalent inline
```

### `fcvideo.h` (engine side declares, fc-pico side implements)

```c
void fcvideo_init(const uint8_t playpal[14 * 768], int preset);
void fcvideo_frame_begin(int video_type, int next_pal);   // called on core 1 before stage A
void fcvideo_line_sink(int y, const uint8_t line320[320]); // stage A output, y in 0..199
void fcvideo_frame_end(void);                              // stages B-E, fcbus_publish()
// host and tests: the same pipeline over a whole frame
void fcvideo_convert_frame8(const uint8_t frame[320 * 200], int pal_no,
                            uint16_t *stream, uint8_t attr[64], uint8_t pal[16]);
uint32_t fcvideo_last_convert_us(void);
```

### `fcapu.h`

```c
typedef struct { const uint8_t *streams[16]; const uint8_t *sfx_scripts; const uint8_t *dpcm_table; } fcapu_bank_t;
void fcapu_init(const fcapu_bank_t *bank);
void fcapu_music_play(int mus_id, bool loop);
void fcapu_music_stop(void);
void fcapu_music_pause(bool paused);
void fcapu_music_volume(int vol_0_15);
int  fcapu_sfx_start(int sfx_id, int vol_0_127);   // voice handle or -1
void fcapu_sfx_stop(int handle);
bool fcapu_sfx_playing(int handle);
void fcapu_pump(void);                              // once per heartbeat; emits via fcbus_apu_write
const fcapu_stats_t *fcapu_stats(void);            // pairs per frame max, deferred, dropped
```
