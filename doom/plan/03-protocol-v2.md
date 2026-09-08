# 03 -- Protocol v2

The FC PICO wire protocol (`docs/pages/protocol.md`) is kept as **v1** and used unchanged for
milestone M1. **v2** is a superset introduced with the Doom boot ROM for M2. The firmware speaks
both; which one is in use is decided per console by the boot ROM that is actually running there,
and the firmware learns it from the first controller message it receives.

## Single source of truth (decision D9)

`doom/fcbus/fcbus_protocol.h` is the only hand-edited definition. `tools/gen_protocol.py`
renders it into:

- `doom/bootrom/gen/protocol.inc` (nesasm `EQU` lines and the mailbox offsets), and
- `doom/tools/fcpico/protocol.py` (constants for the tools and tests).

CI fails if the generated files are stale (`gen_protocol.py --check`). The three tutorial
copies (`rp_system.h`, `SysPico.asm`, `BOOTROM_FIX/PG_main.asm`) are not touched; the Doom boot
ROM includes the generated file instead of `SysPico.asm`'s definitions, and a unit test asserts
the v1 values still equal the tutorial's.

## v1 recap (unchanged, still honoured)

| Direction | Item | Value |
|-----------|------|-------|
| cart -> console | mailbox on stream tail | 64 bytes: `[0]` unused, `[1]` `$FC`, `[2..15]` commands, `[16..63]` APU pairs, `$FF` terminated |
| cart -> console | commands | `PF_COM_NONE 0`, `DMOD 1`, `FDIN 2`, `FDOT 3`, `SE $80|n`, `BGM $A0|n`, `VRAM $C0|adrH adrL data` |
| console -> cart | opcodes | `FP_COM_VER $2F`, `ROM $3F`, `LOG $BF`, `DRQ $CF`, `DLD $DF`, `RST $EF`, `INI $FF` |
| console -> cart | heartbeat | any other byte = controller 1 state |
| data mode | `PF_DAT_VRAM $80`, `PF_DAT_RAM $81`, `PF_DAT_STEP $82`; 6-byte header `com, $FC, adrL, adrH, sizeL, sizeH` | |

## v2 additions

### Mailbox: 128 bytes, fixed length

`FC_COM_BUF_SIZE_V2 = 128`. The first 64 bytes keep the v1 layout so the v1 command
executor (`jobPICO`) and APU replay loop can be reused in the new ROM without change.

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | `flags` | bit 0 `ATTR_VALID`, bit 1 `PAL_VALID`, bit 2 `APU_VALID`, bit 7 `V2` (always 1 in v2 frames). Byte 0 is "unused but not reserved" in v1; FC_PICO_GB uses bytes 0-1 for its own purpose. This is a *different* firmware, so the collision is only a documentation note. |
| 1 | 1 | magic `$FC` | as v1 |
| 2 | 14 | commands | as v1; the Doom firmware uses `PF_COM_DMOD`, `FDIN`, `FDOT` only |
| 16 | 32 | APU pairs | as v1 layout but **capped at 16 pairs** (`$FF` terminator within the first 33 bytes); offsets 48..63 are always `$FF` |
| 48 | 16 | BG palette | 16 bytes for `$3F00`-`$3F0F`, applied when `PAL_VALID` |
| 64 | 64 | attribute table | 64 bytes for `$23C0`-`$23FF`, applied when `ATTR_VALID` |

`PPU_COUNT_VAL_V2 = 15426 + 128 = 15554`. The firmware selects the expected count from the
protocol version it detected.

Why fixed length: the read counter is the only sync mechanism, so the number of `$2007` reads
per frame must be a compile-time constant on both sides. A variable-length mailbox (skip the
attribute block when unchanged) is a documented v2.1 option -- the cartridge always knows what
it sent, so it could adjust the expected count per frame -- but it is not needed to fit the
NMI budget (see 07) and it complicates the model.

### Console -> cartridge: explicit controller packet

| Opcode | Value | Follow-up bytes | Meaning |
|--------|-------|-----------------|---------|
| `FP_COM_KEY` | `$4F` | `pad1`, `pad2` | Controller 1 and 2 state (same bit layout as v1). **This is the v2 heartbeat.** |
| `FP_COM_HELLO` | `$5F` | `ver` | Sent once by the boot ROM after `FP_COM_INI`; `ver = 2`. Lets the firmware select v2 before the first heartbeat. |

Raw controller bytes (any value not in the opcode tables) remain the v1 heartbeat. In v2 the
6502 never writes a raw byte, so no controller combination can alias an opcode. The firmware
treats a raw byte as "v1 heartbeat" only until it has seen `FP_COM_HELLO 2`; after that a raw
byte is logged as a protocol error and ignored.

`$4F` and `$5F` were chosen because they sit in the gaps of the existing `$xF` pattern and do
not collide with the unused `FP_COM_ACK $0F` / `FP_COM_NAK $1F`.

### Console -> cartridge: nothing else changes

`FP_COM_VER`, `FP_COM_ROM`, `FP_COM_RST`, `FP_COM_INI`, `FP_COM_LOG`, `FP_COM_DRQ`,
`FP_COM_DLD` keep their values and semantics because the **fix bank** issues the first three
and they cannot change (D6).

### Version stamp

The 14-byte build stamp at `$EFF0` becomes `"DOOM-vv-nnnnnn"` (protocol version, build number,
e.g. `DOOM-02-000001`). `CHK_ROMVER` in the fix bank does not parse it: it skips reply bytes
until it sees `'C'` (the firmware streams the sync word `"!#FC"` first, as `0x21212121,
0x43462321`), then compares 14 bytes verbatim against `DB_ROM_VER`. Any 14-byte string works,
and the firmware must stream the sync word before the stamp exactly as `rp_system::ver_dma()`
does. (The "must begin with 20" description in `docs/pages/boot-and-reflash.md` refers to the
tutorial's date-based stamps, not to a check in the code.)

**Reproducible builds:** `dbdate.h` is replaced by a stamp derived from the protocol version
and a version number in `bootrom/version.inc`, not the wall clock. Bumping the stamp is a
deliberate act (it forces every console to reflash).

### Data mode

Unchanged. The Doom ROM keeps `xPF_COM_DMOD` and the firmware keeps `startDataMode()`
semantics for the two uses Doom has: uploading the initial nametable/attribute/palette state
at boot, and (optional) uploading a splash nametable while Doom initialises.

## Firmware-side state machine

```
            FP_COM_RST / power-on
                    |
                    v
  +------------- IDLE ----------------+
  |  serve FP_COM_VER / FP_COM_ROM    |   <- fix bank talking; DMA stopped
  +-----------------------------------+
                    | FP_COM_INI stage
                    v
  +------------- INIT -----------------+
  |  reset fcbus, publish test pattern |
  |  expect FP_COM_HELLO (v2) or raw   |
  |  key (v1) as first heartbeat       |
  +------------------------------------+
        | v2 hello          | raw key
        v                   v
   RUN_V2 (128 B)      RUN_V1 (64 B)
   count = 15554       count = 15490
        \                   /
         \  PF_COM_DMOD    /
          v               v
          DATA_MODE (DMA stopped, DRQ/DLD service) --> back to RUN_*
```

Timeouts: no heartbeat for 2 s in `RUN_*` -> log once, stop DMA, go to `INIT` (the console may
have been reset; the next `FP_COM_INI` re-enters).

## Compatibility matrix

| Console ROM | Firmware | Result |
|-------------|----------|--------|
| tutorial (v1) | Doom | `CHK_ROMVER` mismatch -> console reflashes to the Doom ROM (v2). One-time, automatic. |
| Doom (v2) | tutorial firmware | mismatch -> console reflashes back to the tutorial ROM. Reversible. |
| Doom (v2) | Doom, same stamp | runs |
| Doom (v2) | Doom, newer stamp | reflash, then runs |
| FC_PICO_GB ROM | Doom | mismatch -> reflash to Doom ROM |

Note that a reflash takes the console through `ROM_ERACE`/`ROM_UPDATE` (a few seconds, with
"ROM UPDATE" on screen). Document this in the user guide.

## Test obligations (see 09)

- `tests/protocol/test_mailbox_layout.c`: offsets, sizes, terminator invariants, `PPU_COUNT_VAL`
  for both versions.
- `tests/protocol/test_rx_dispatch.c`: every opcode and every raw byte value 0..255 through
  `fcbus_rx_dispatch()` in v1 and v2 states, asserting heartbeat/no-heartbeat.
- `sim/mesen2`: boot the v1 tutorial ROM against the Doom firmware model and observe the
  reflash to v2, then the v2 handshake.
