@page audio_page Audio pipeline

The music is composed as MML, compiled to an NSF, executed by a 6502 emulator on the
cartridge, and played by the **console's own APU**. The cartridge synthesises nothing
for those channels; it only decides what the registers should contain.

## The chain

@dot
digraph audio {
  rankdir=LR;
  node [shape=box, fontname="Helvetica", fontsize=9];
  edge [fontname="Helvetica", fontsize=8];
  mml  [label="mml/*.mml, *.mmh\n(MML source)"];
  nsc  [label="nsc.exe\n+ nsd.bin driver"];
  nsf  [label="sound.nsf\n7332 bytes"];
  c    [label="res/resdata.c\n_resdata[], id NSF_SOUND"];
  emu  [label="rp_fcemu\n6502 interpreter\n(core 1)"];
  trap [label="sys.setAPU()\nring buffer"];
  pack [label="setPF_APU()\nmailbox $30..$5F"];
  apu  [label="console APU\n$4000-$4017", style=filled, fillcolor=gray90];
  mml -> nsc -> nsf -> c -> emu -> trap -> pack -> apu [minlen=1];
}
@enddot

## Authoring

Sources live in `tutorial_project/mml/`, using NSD.Lib (NES Sound Driver Library) MML.

| File | Contents |
|---|---|
| `sound.mml` | Master file: directives, slot counts, include list |
| `env_set0.mmh` | Envelope definitions and drum macros |
| `bgm_stage.mmh`, `bgm_boss.mmh`, `bgm_clear.mmh`, `bgm_over.mmh` | The four songs |
| `bgm_stage_sub.mmh` | Shared drum pattern |
| `eft.mmh` | Sound-effect bank |

`sound.mml` declares four BGM slots and 21 SE slots. Tracks are `TR1`-`TR4`; the DPCM
track is present but commented out.

Build and audition:

```
cd tutorial_project/mml
m.bat      :: nsc -N sound.mml  -> sound.nsf
g.bat      :: audition in VirtuaNES
```

@note `env_set0.mmh` originally used full-width `｛ ｝` on one envelope line. These were
normalised to ASCII braces when the tree was converted to UTF-8; the rebuilt
`sound.nsf` is byte-identical. See @ref conventions.

## Execution

rp_nsfplayer extends rp_fcemu, a 6502 interpreter derived from FabGL. Rather than
implementing NSD.Lib's behaviour, the cartridge simply **runs the driver's own 6502
code**.

The driver's entry points are hard-coded, because the addresses are fixed by
`nsd.bin`:

| Symbol | Address | Purpose |
|---|---|---|
| `_nsf_init` | `$8010` | NSF init |
| `_nmi_main` | `$8084` | NSF play, once per frame |
| `_nsd_play_bgm` | `$8137` | Start music; A/X = data pointer |
| `_nsd_stop_bgm` | `$8219` | Stop music |
| `_nsd_play_se` | `$8239` | Start sound effect |
| `_nsd_stop_se` | `$82B7` | Stop sound effect |
| `_nsd_table_idx` | `$8F6E` | Song pointer table |

@warning These addresses belong to the specific `nsd.bin` shipped here. Upgrading NSD.Lib
will move them and the constants in rp_nsfplayer.h must be updated to match.

The `BGM_*` and `SE_*` values in `ap_main.h` are indices into the table at
`_nsd_table_idx`, which is why they share one numbering space.

## The emulator

rp_fcemu is a cycle-counting 6502 interpreter with a deliberately minimal memory map:

| Range | Read | Write |
|---|---|---|
| `$0000`-`$1FFF` | 2 KB RAM, mirrored | same |
| `$4000`-`$4017` | `0xFF` | **`sys.setAPU()`** |
| `$8000`-`$BFFF` | `m_ROM8000` -- music data, 16 KB | — |
| `$C000`-`$DFFF` | `m_ROMC000` -- DPCM, 8 KB | — |
| `$E000`-`$FFFF` | `m_ROME000` -- driver, 8 KB | — |

Everything else reads `0xFF` and ignores writes. The APU range is the only outlet, and
it is what makes the whole scheme work.

`run()` is a *call a subroutine and return* emulator, not a free-running CPU. It
terminates on `BRK`, or on an `RTS` that pops past the initial stack frame
(`m_SP == 0xfd`). An unrecognised opcode prints a diagnostic and stops.

@note Decimal mode and the common undocumented opcodes (LAX, SAX, DCP, ISC) are
implemented, so a driver relying on them will still work.

## Relay to the console

rp_system::setAPU() pushes each write into a 64-entry ring. Once per frame,
rp_system::setPF_APU() drains it into the mailbox from offset #PICO_SNDREG, as
`(index, value)` pairs terminated by `$FF`. @ref NMI replays them to `$4000 + index`.

Two different caps apply, and they are one apart. rp_system::setPF_APU() fills from
#PICO_SNDREG up to `FC_COM_BUF_SIZE-2`, so it ships at most **23 pairs per frame**; the
6502 replay loop in @ref NMI stops at `$30` bytes, **24 pairs**. The mailbox runs out
first.

A frame's surplus is *not* lost. setPF_APU() stops filling once the mailbox is full but
leaves `m_APU_R_IDX` where it is, so the remainder goes out on the next frame.

@warning There is a loss path, and it is a different one. rp_system::setAPU() advances
the write index with no full-check, so a driver sustaining more than 23 writes per frame
outruns the drain and eventually laps the 64-entry ring, overwriting pairs that were
never sent. The drop is silent -- nothing counts it and nothing reports it.

The 6502-side sound entry points (`PLAY_BGM`, `PLAY_SE`, `SOUND_SYSTEM`) exist in
`BOOTROM/` but are all commented out. There is no sound driver running on the console;
do not go looking for one.

## MP3 playback

A second, independent path exists for streamed audio, using `BackgroundAudio` and
`PWMAudio` on GP28. rp_sound::setMP3() and rp_sound::jobMP3() manage it, with gain
taken from the save data.

@note MP3 as a *music* source is selected at run time, on the `SDT_MP3_ENA` save-data
flag, not at build time; the old `LINK_MP3` build option was retired once resources
could be uploaded separately with `picotool`. The path is still inert in this tree,
because rp_sound::jobSound() routes through `setMP3data()`, whose track lookup is
commented out -- so every request falls back to the NSF player.
