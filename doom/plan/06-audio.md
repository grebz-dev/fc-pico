# 06 -- Audio

> **Dependency update (2026-09-25):** Required [sprite UI](12-sprite-ui.md)
> precedes audio integration in execution priority. Coordinate P4-T5's proposed
> boot-ROM v3 with P3-U3 before assigning a protocol version. Budget APU replay
> and DMC stalls together with UI reception, sprite palettes and OAM DMA, and
> revalidate controller reads under their combined worst case.


The host `fcapu` sequencer and audio conversion tools are implemented and tested,
including automatic MUS/MIDI music and synthesised pulse/noise effects. Engine
sound/music adapters are implemented and host-tested. Asset-bank installation,
firmware asset placement, arrangements and console audio remain open; see
[`../issues/I-05-audio-tools.md`](../issues/I-05-audio-tools.md) and
[`../issues/README.md`](../issues/README.md).

The cartridge cannot make sound (the PWM pin drives nothing the console amplifies for the
Doom use case). Everything is played by the console's APU from register writes the cartridge
ships in the mailbox and the boot ROM replays during vblank ([`docs/pages/audio.md`](../../docs/pages/audio.md)).

## The instrument

| Channel | Registers | Use |
|---------|-----------|-----|
| Pulse 1 | `$4000-$4003` | music lead |
| Pulse 2 | `$4004-$4007` | music harmony; **stolen by synthesised SFX** |
| Triangle | `$4008-$400B` | music bass |
| Noise | `$400C-$400F` | music percussion; **stolen by noise SFX** |
| DPCM | `$4010-$4013` | sampled SFX from the boot ROM bank |
| Control | `$4015` | channel enables / DPCM start |

Budget: **15 `(reg,value)` pairs per frame** (v2 mailbox; `APU_PAIRS_MAX_V2`), 23 in v1. Latency: writes made
during frame N are replayed in the NMI that follows frame N+1's picture -- about two frames.

## Decision D5: a register sequencer, not a 6502 emulator

`fcapu` (C, host-testable, no allocation) runs on the cartridge, called from `I_UpdateSound()`
(already polled from the game loop and from core 1's wait loops) but doing its real work once
per heartbeat (`fcbus_frame_no` changed): it advances the music stream one frame, advances the
active SFX scripts one frame, resolves channel ownership, and emits writes into the back
mailbox through `fcbus_apu_write(reg, val)` in priority order until the cap is reached.

Why not run a 6502 sound driver on the cartridge as the tutorial does: it drags in the GPL-v3
`rp_fcemu`, it is not host-testable without that emulator, and its write count per frame is
not controllable. The sequencer's output is deterministic and testable to the byte.

### Music stream format (`.apus`)

Produced offline (below), stored in flash as `const uint8_t[]`.

```
header: magic "APUS", version, NTSC frame rate flag, loop point (frame index), length (frames)
body:   sequence of frames; each frame = count byte (0..15) then count x (reg, value) pairs;
        a count byte with bit 7 set means "N silent frames" (run-length)
```

Register semantics are raw APU (`reg` in 0..0x13). The offline tool guarantees `count <= 12`
so that 3 pairs remain for SFX in every frame under the v2 cap of 15.

### SFX

Two kinds:

1. **DPCM** samples in the boot ROM bank (`$C000`-`$ECFF`, see [07](07-bootrom.md)). Trigger = 4 writes:
   `$4010 = rate | 0x00`, `$4012 = (addr - 0xC000) >> 6`, `$4013 = (len - 1) >> 4`,
   `$4015 &= ~0x10` then `$4015 |= 0x10` (that is 5 writes; the clear can be skipped if the
   channel is known idle). One DPCM voice.
2. **Synthesised** scripts: per-frame tables of `(pulse2 or noise) period/volume/duty`
   generated offline from the Doom sample (pitch and envelope tracking) or hand-authored.
   Up to two concurrent (one pulse, one noise).

Priority follows Doom's `S_sfx[].priority` (lower is more important) with a channel-type
preference table (`fcapu_sfx_table.h`): each of Doom's 60-ish sounds is assigned
`{DPCM id | PULSE script | NOISE script | NONE}`. A new sound of higher priority steals the
voice; equal priority replaces if the current one is more than half done.

Initial DPCM set (rate `$9` = 11.2 kHz unless stated; sizes from 1-bit encoding = rate/8 bytes
per second):

| Doom sfx | Approx length | Bytes |
|----------|---------------|-------|
| `dspistol` | 0.25 s | 350 |
| `dsshotgn` | 0.6 s | 850 |
| `dsdoropn` | 0.9 s | 1250 |
| `dsdorcls` | 0.8 s | 1120 |
| `dspunch` | 0.2 s | 280 |
| `dsplpain` | 0.4 s | 560 |
| `dsoof` | 0.3 s | 420 |
| `dsbgdth1` (imp death) | 0.8 s | 1120 |
| `dspopain` (imp pain) | 0.4 s | 560 |
| `dsslop` | 0.5 s | 700 |
| `dsswtchn` | 0.2 s | 280 |
| `dsitemup` (as DPCM if the pulse version sounds worse) | 0.15 s | 210 |
| total | | ~7.7 KB of the ~11 KB budget |

Everything else is synthesised (`dsbarexp` and `dsrlaunc` = noise sweeps, `dsfirsht` = pulse
sweep, `dspstop`/`dsstnmov` = short noise, monster sights = pulse chirps) or dropped.

### Arbitration per frame (in this order, until 15 pairs)

1. DPCM trigger/stop for this frame (never deferred).
2. Voice steals: silence writes for a channel changing owner.
3. SFX script writes.
4. Music note-on writes (`$4003`/`$4007`/`$400B`/`$400F` and their period low bytes).
5. Music timbre/volume updates.

Deferred writes are re-attempted next frame from a 64-entry queue; the queue has a drop counter
that the serial CLI reports (`apu drops`). In practice music streams are pre-thinned so that
step 4-5 rarely exceed 10.

### Doom sound API mapping (engine fork, `src/fcpico/i_sound_fcpico.c`)

| `sound_module_t` / `music_module_t` entry | fcpico |
|---|---|
| `Init` | Initialize the configured `fcapu` bank once across both modules |
| `GetSfxLumpNum` | table lookup by name (no lump needed unless synthesising at runtime) |
| `StartSound(sfx, ch, vol, sep, pitch)` | `fcapu_sfx_start(id, vol)`; `sep` and `pitch` ignored (mono, fixed pitch) |
| `StopSound`, `SoundIsPlaying`, `UpdateSoundParams` | sequencer state |
| `PlaySong(handle, looping)` | `fcapu_music_play(id, looping)`; stable song handle from configured bank (`USE_DIRECT_MIDI_LUMP`/`USE_MUSX` paths bypassed) |
| `StopSong`, `PauseSong`, `ResumeSong`, `SetMusicVolume` | sequencer; volume scales envelope nibbles from 0 to 15 |
| `UpdateSound` | `fcapu_pump()` -- no-op unless a new heartbeat has happened |

`I_PicoSoundSetMusicGenerator`, the ADPCM decoder, `emu8950` and `pico_audio_i2s` are not linked
in the fcpico target.

### Implemented engine adapter (2026-10-02)

`src/fcpico/i_audio_fcpico.h` defines `I_FCPicoAudioSetBank()`: install a
caller-owned, persistent `fcpico_audio_bank_t` before either module initializes.
It supplies sequencer streams, full music lump names (`D_E1M1`) and bare SFX names
(`pistol`). Doom music registration resolves WHX lump numbers against these names
without caching raw MUSX bytes. Effects resolve linked aliases and use opaque
sequencer handles; stale handles cannot stop replacement voices. Synthesized
volume changes apply even during silent script frames; DPCM amplitude is fixed.

Device updates run on core 0 with local interrupts disabled, advancing once per
`bus.core.frame_no` and writing through `fcbus_core_apu_write()`. Core 1 polls do
not touch sequencer state. The host runner uses its rendered-frame counter and
discards APU writes; it does not provide audible playback or a real 60 Hz clock.
No bank is installed by the firmware yet, so default builds remain silent.

`tests/audio_engine` exercises the real engine module with a small synthetic bank
and an empty bank. Physical playback, combined UI/APU timing and generated bank
placement remain acceptance gates.

## Offline pipeline (`doom/tools/audio/`)

```
 doom1.wad --(wadextract.py)--> D_E1M1.mus ... --(mus2mid, built from the engine tree)--> .mid
     .mid --(FamiStudio: midi import, arrange)--> song.fms --(FamiStudio CLI export)--> .vgm
     .vgm --(vgm2apus.py)--> D_E1M1.apus                                     (primary path)
     .mid --(mus2apus.py --auto)--> D_E1M1.apus                                (fallback path)

 doom1.wad --> dspistol.lmp (8-bit, 11025 Hz) --(sfx2dpcm.py)--> .dmc + table entry
                                              --(sfx2apu.py --auto / hand .sfx)--> script
```

- **FamiStudio** (MIT, cross-platform; command-line form `FamiStudio <input> <command>
  <output> [-options]`, full option list via its `-help`; the docs site was unreachable when
  this was written, so [P4-T3](10-workplan.md#p4-t3-audio-tools----spec-06-pipeline-size-l-depends----acceptance-teststools-round-trips-mus2apuspy---auto-produces-all-13-streams-from-doom1wad-under-the-cap) records the exact `midi-import`, `.ftm` import and `vgm-export`
  option names in this section) is the arranging tool. Its project files
  (`.fms`) are checked in under `doom/assets/music/` so arrangements are reproducible; a human
  can improve them in the GUI without touching code.
- `vgm2apus.py` converts the VGM register log (frame-accurate for NES) into the stream format,
  merging same-frame writes, thinning redundant writes (a register rewritten with the value it
  already holds is dropped), and asserting the `<= 12` per-frame cap (it fails loudly with the
  frame number if an arrangement is too busy).
- `mus2apus.py --auto` is the no-human path: melody = highest-pitched active note ->
  pulse 1, second voice -> pulse 2, bass = lowest note folded into the triangle's range,
  GM drums -> noise presets. Quality is "recognisable", which is enough for M4; arrangements
  in FamiStudio are the polish.
- `sfx2dpcm.py` implements the standard 1-bit delta encoder (level starts at 64, step +/-2,
  clamp 0..127; input resampled to the chosen APU rate; output length padded to 16n+1) and
  writes `dpcm_bank.bin` plus `dpcm_table.inc`/`.h` (rate, `$4012`, `$4013` per sample).
  Validated by decoding back and comparing RMS error against the source.
- `respack.py` packs `.apus` streams and SFX scripts into one `const` archive with an index
  (replacing the Windows-only `binlink`/`Bin2C`).

Music needed for the shareware episode: `D_E1M1`-`D_E1M9`, `D_INTER`, `D_INTRO`,
`D_INTROA`, `D_VICTOR` (13 streams). `D_BUNNY` is episode-3 finale music and is
absent from shareware `DOOM1.WAD`; it can be converted separately from a
registered WAD. The original 2-6 KB per-song estimate is a polish target,
not a measured bound. The automatic converter produces 318-36,794 bytes per
song and 213,777 bytes total on the v1.9 shareware WAD. Firmware placement
must be checked before embedding this bank; see [the progress log](../PROGRESS.md).

### Implemented extraction and VGM commands (2026-10-01)

```sh
python3 doom/tools/audio/wadextract.py /path/to/doom1.wad 'D_*' 'DS*' -o /tmp/doom-lumps
python3 doom/tools/audio/vgm2apus.py /path/to/song.vgm -o /tmp/song.apus
doom/.venv/bin/python doom/tools/audio/mus2apus.py /path/to/doom1.wad --auto -o /tmp/doom-music
doom/.venv/bin/python doom/tools/audio/sfx2apu.py /tmp/doom-lumps/DSPISTOL.lmp --auto --kind noise -o /tmp/pistol.apus
```

The extractor validates IWAD/PWAD directories and lump ranges, accepts
case-insensitive names/globs, and selects the last entry for duplicate names.
Music lumps use `.mus`; other lumps use `.lmp`. Unmatched patterns fail before
writing files.

The VGM converter follows the [VGM 1.71 specification](https://github.com/vgmrips/vgmplay-legacy/blob/master/VGMPlay/vgmspec171.txt)
for single-2A03 writes, sample waits, wait overrides and relative offsets.
It accepts raw VGM or gzip-compressed VGZ, uses 60 Hz NTSC or 50 Hz PAL timing
from the recording-rate header (zero defaults to NTSC), merges same-frame
writes and applies `apus.thin_register_writes`. It reports the frame number
if the thinned result exceeds 12 pairs. Loop offsets must identify commands
on frame boundaries, without preceding intro writes in that same frame;
thinning restarts at the loop point to retain writes needed on later passes.
DPCM writes/data blocks, FDS, a second APU and other chip commands are rejected.
This path produces music register streams; DPCM effects still require the
separate bank pipeline. Real FamiStudio export and listening checks remain open.

`mus2apus.py --auto` reads MUS directly at 140 ticks/second or MIDI type 0/1
with PPQN timing, running status and tempo changes. It tracks note velocity,
channel volume, expression, sustain and channel-off controllers. Highest and
second-highest active notes use pulse 1/2; the lowest uses octave-folded triangle;
percussion uses kick/snare/hat noise presets. It emits NTSC streams with a setup
frame, one empty frame for loop retriggering, and a final mute frame. Looping
defaults to frame zero; `--no-loop` produces a finite stream. Program changes,
pan, pitch bends and pressure are parsed but ignored in this fixed-timbre
fallback; MIDI type 2 and SMPTE timing are rejected. This is a host-tested
automatic arrangement, with listening quality still to be assessed.

`sfx2apu.py --auto` tracks the PCM RMS envelope and pulse pitch or noise
brightness, strips DMX's 16 padding samples at each end, and writes a non-looping
APUS stream restricted to pulse 2 or noise. `--kind pulse` is the default;
`--kind noise` selects noise. It never writes `$4015`. A `.sfx` override uses one
frame per non-comment line with hexadecimal `reg=value` pairs, such as
`04=bf 05=08 06=fd 07=08`; `-` represents a silent frame and `#` starts a comment.
A final mute frame ends the effect. WAV input is also accepted. These generated
effects are approximations, with human overrides available for important sounds.

## Existing arrangements to start from

`rasteri/PiPU` (GPL) ships `music/DOOM.ftm`, a FamiTracker module with eight Doom songs
arranged for the 2A03: **Intro, Inter, E1M1, E1M2, E1M3, E1M4, E2M1, E3M1** (plus `DOOM.nsf`
and a `famitone`-style driver on the NES side). FamiStudio imports `.ftm`, so [P4-T4](10-workplan.md#p4-t4-arrangements----size-l-human-assisted-depends-p4-t3-acceptance-famistudio-projects-for-d_e1m1-d_intro-d_introa-d_inter-committed-vgm-export-reproducible-via-cli-others-auto) starts by
importing that module, exporting VGM per song and converting with `vgm2apus.py`; only
`D_E1M5`-`D_E1M9` and `D_VICTOR` need new shareware arrangements (auto or human).
`D_BUNNY` is outside shareware scope. Licensing:
the module is distributed under the repository's GPL; reusing it in this GPLv2/GPLv3-mixed
firmware is compatible, with attribution to Andrew Tait in [`LICENSES.md`](../LICENSES.md) (decision H6 still
applies to taste). Verify the songs' completeness by listening once in FamiStudio.

## Note-retrigger rule (from FC PICO GB)

When the sequencer emits register writes from a per-frame register image (the VGM path
produces one), rewriting `$4003/$4007/$400B/$400F` restarts the envelope and the length
counter and produces an audible click. FC PICO GB's rule, adopted here: write the
high-period register only when (a) the channel was not written in the previous frame -- a
new note -- or (b) the period bits (`$4003` bits 0-2 and `$4002`) changed. Volume/duty
(`$4000`) and sweep (`$4001`) are written whenever they change. `vgm2apus.py` applies the rule
offline so the stream already contains only necessary writes.

## `.apus` stream format

```
offset  size  field
0       4     magic "APUS"
4       1     version = 1
5       1     flags: bit0 PAL timing (period table), bit1 reserved
6       2     loop_frame (little endian; 0xFFFF = no loop)
8       4     frame_count
12      4     body_len
16      ...   body
body:   frame := count(u8: 0..15) then count x (reg u8 in 0..0x17, value u8)
        | silence(u8: 0x80 | k, k in 1..127 frames with no writes)
```

`count <= 12` is enforced by the tool so that 3 pairs per frame remain for effects under the
v2 cap of 15 (`APU_PAIRS_MAX_V2`). Streams are stored in flash as `const uint8_t[]`
via `respack.py`; the sequencer reads them with a byte cursor and no decompression.

## Tests

- `tests/apu/test_sequencer.c`: never more than the cap per frame; DPCM trigger ordering;
  steal/restore of pulse 2 and noise around a sound effect; pause/resume leaves no stuck
  notes (`$4015` and volume registers verified); the deferred queue never laps.
- `tests/tools/test_audio_convert.py`: synthetic WAD/VGM/MUS/MIDI/PCM round
  trips, timing, retriggering, caps, malformed inputs and byte-identical CLI
  outputs. Set `FCPICO_AUDIO_WAD=/path/to/doom1.wad` to also verify all shareware
  music and DMX effects. `test_apus.py` and `test_dpcm.py` cover support tools.
- Co-simulation: a Mesen2 Lua script logs every `$40xx` write with its frame number and
  compares against the sequencer's own log for a scripted 10-second scenario (title music,
  one pistol shot, one door).
- Hardware listening checklist (M4): music audible on each channel, no stuck notes after 10
  minutes, SFX audible under music, DPCM does not corrupt controller input (fix bank majority
  vote in effect).
