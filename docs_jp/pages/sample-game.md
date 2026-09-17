@page sample_game The sample game

`tutorial_project/sample_game/` is the finished product the tutorial is a reduction
of: a complete vertical shooter, *FC-PICO TYPE ZERO*, with three stages, music,
MP3 background tracks, a 3D renderer and an options menu.

It is worth reading for one structural reason. The game's rules are not written in
C++. They are 6502 code, and the cartridge **executes them itself**, on an emulated
CPU, then draws the result in 3D. The same 6502 code is also a complete Famicom
cartridge that runs on unmodified hardware. One codebase, two machines.

## Two ways to run one ROM

`GAMEROM/` assembles to `map0demo.NES`, a plain NROM image -- 32 KB PRG, 8 KB CHR,
mapper 0. It has everything a Famicom game needs: a reset vector at `$FFFC`, an NMI
handler that does sprite DMA and scrolling, a controller reader, and a sound driver.
`GAMEROM/rom.bat` will flash it to a real cartridge, and `g.BAT` will run it in an
emulator.

It also has a second front door. At the top of bank 3 sit two `jsr`/`brk` pairs at
fixed addresses:

| Address | Label | Does |
|---|---|---|
| `$E000` | `jvcFCP_GAME_INIT` | Reset the variables for a new stage |
| `$E004` | `jvcFCP_GAME_MAIN` | Advance the game by exactly one frame |

Those call only `updateMission` and `moveGameObj` -- the simulation, with no drawing
in it at all. `ap_game.cpp` reaches them as `emu.run(0xE000 + 4 * n)`, and the
trailing `brk` is what hands control back to the emulator.

@warning The addresses are the interface, not the labels. `$E000` and `$E004` are
hard-coded on the C++ side. Anything inserted above `jvcFCP_GAME_INIT` in bank 3
moves them and breaks the cartridge build silently -- the ROM will still assemble
and still run on a real Famicom.

So on a Famicom the ROM draws itself with sprites and a nametable; under the
cartridge, roughly half of it is dead code -- `NMI`, `KEY_RTN`, `SOUND_SYSTEM`,
`AplGameDisp.asm`, every `Apl*` screen -- and the RP2350 supplies the equivalent.

## One frame

@dot
digraph frame {
  rankdir=LR;
  node [shape=box, fontname="Helvetica", fontsize=9];
  edge [fontname="Helvetica", fontsize=8];

  key  [label="sys.setKeyUpdate()\ncontroller state"];
  wr   [label="write EM_KEY_NEW,\nEM_KEY_TRG"];
  run  [label="emu.run($E004)\n6502 until brk", style=filled, fillcolor=gray90];
  tab  [label="object tables\nin emu.m_RAM", style=filled, fillcolor=gray90];
  conv [label="conv3DObje()\ntable -> Obj3d"];
  draw [label="ap.draw()\nrasterise"];
  ppu  [label="PIO -> console PPU"];
  snd  [label="REQ_BGM_NO,\nREQ_SE_NO x3"];

  key -> wr -> run -> tab -> conv -> draw -> ppu;
  run -> snd [label="raises"];
  snd -> conv [label="played, then\nzeroed", style=dashed];
}
@enddot

The shared surface is `rp_fcemu::m_RAM`, and traffic runs both ways:

- **C++ writes**: the controller bytes, the demo flag, the starting stage, and a
  cleared score.
- **6502 writes**: every object table, the score, the lives, the mission state.
- **Both**: sound requests are raised by the 6502 and cleared by the C++ once
  played, which is the acknowledgement. The explosion counter `BAKU_EFC_CNT` is
  stranger still -- the table is the 6502's, but the animation is advanced on the
  C++ side.

## How a 2D game becomes a 3D one

`ap_game::conv3Dxy()` is the whole trick, and it is three lines long:

```cpp
obj->m_x = 0.08f * (128 - x);
obj->m_z = 0.1f  * (128 - y);
```

Screen **Y becomes world Z**. The 2D playfield is laid flat on the ground and
viewed in perspective, so an enemy descending the screen in the 6502's world is an
enemy approaching the camera in the rendered one. Nothing else about the game
changes; the collision test in `hitEnemyNTObj` is still the original 2D one.

`conv3DObje()` then chooses a representation per object, keyed on the top bit of
the enemy kind: values below `0x80` are shots and get an animated 8x8 sprite,
values above are craft and get a real model. Meteors get a dithered tumbling cube
instead, and the warp effect a 16x16 sprite.

@see @ref nes_doom for how the resulting frame reaches the television.

## The screens

`ap_main` dispatches to one screen module at a time. Each has an `init()` run on
the first frame and a `main()` run thereafter, and each carves its own object
slots out of the single shared `ap_main::m_obj` pool of 256.

| State | Module | Notes |
|---|---|---|
| `ST_TITLE` | ap_title | Menu; hands over to attract mode after ten seconds |
| `ST_GAME` | ap_game | Interactive play |
| `ST_DEMO0` | ap_game | Attract mode: the same screen, driven by a synthetic key pattern |
| `ST_DEMO1` | ap_demo0 | Attract mode: the model parade |
| `ST_OVER` | ap_over | Game over, with the BCD score |
| `ST_CLEAR` | ap_clear | Stage cleared |
| `ST_OPTION` | ap_option | Seven-row settings menu, saved to EEPROM |
| `ST_LICENSE` | ap_license | Two BPE-compressed nametable pages |

The demo "AI" is worth calling out for how little there is of it: attract mode
replaces the controller byte with a square wave that alternates left and right
every 64 frames. That is all.

## Missions are bytecode

A stage is not a function. `cfg/cfgStage.h` lists the missions a stage runs, and
each mission is a **script** assembled from the `MC_*` macros in `defMission.h`:

```
MC_ZAKO   NTK_NOMAL, 3, 128    ; spawn a wave
MC_LOOP_CNT 4                  ; four times
MC_JMP    label, cond
MC_END
```

`mainMissionControl` is the interpreter. `MISSON_PC` is its program counter,
`MISSON_WAIT` suspends it for a number of frames, `MISSON_STACK` and
`MISSON_PC_SP` give it a call stack, and `MISSON_TYPE` set to `$FF` halts it.

That halt value is load-bearing across the language boundary: `ap_game::main()`
watches `MISSON_TYPE` for `0xff` and treats it as stage-cleared. The C++ has no
other notion of what finishing a stage means.

@note A called script may not itself call. The restriction is recorded only in the
comment at the head of `cfg/cfgMissonHara.h`, and the interpreter does not enforce
it.

## Resources

Two archives, both built by `res/conv.bat` and both indexed by `binlink`:

| Archive | Built with | Holds | Reaches the board via |
|---|---|---|---|
| `res.bin` -> `resdata.c` | id base 0 | boot ROM, game ROM, NSF music, sprite sheet, font, two licence pages | linked into the firmware |
| `res2.bin` | id base 10000 | four MP3 tracks | `res/upload.bat`, flashed to `0x10200000` |

The id base is the archive selector: `getResHead()` divides the id by 10000, so
`MP3_007_MONO` at 10000 resolves against `RES_DATA_ADR` instead of the linked-in
blob. This is the mechanism the tutorial firmware has but never exercises.

`binlink2.lst` also shows the `:LABEL` form, which emits a bracketing constant
rather than an archive entry -- `MP3_RES_ID` and `MP3_RES_ID_MAX` mark where the
tracks begin and end.

@warning Nothing in the build checks that the firmware image has not grown past
`0x10200000`. @see @ref generated_resources

## Build order

```
GAMEROM/mml/m.bat      :: game ROM's own music -> sound.bin
GAMEROM/chr/conv.bat   :: sprite sheet + licence nametables
GAMEROM/m.BAT          :: assemble -> map0demo.NES
res/conv.bat           :: models, licence pages, then both archives
                       :: (also reads BOOTROM/rom.NES and mml/sound.nsf)
Arduino IDE            :: sample_game.ino -> UF2
res/upload.bat         :: flash res2.bin (only when the MP3s change)
```

@see @ref build_pipeline for the tutorial firmware's equivalent, and for the shared
boot-ROM steps this depends on.

## Duplication to keep in sync

This tree carries four separate hand-maintained copies of things. None of them is
checked by the build.

- **The RAM map.** `sample_game/ap_game.h` mirrors 51 addresses from
  `GAMEROM/defRAM.h`. They all agree at this revision -- that was verified when
  this page was written -- but the two files are edited independently. `PLY_STAGE`
  is the fragile one: on the 6502 side it is an alias for a debug slot, `DEBUG_DT0`.
- **The sound slots.** The `BGM_*` and `SE_*` numbering exists in `ap_main.h` and
  again in `GAMEROM/defGame.h`. The 6502 raises a number, the C++ plays it, and
  neither validates it.
- **The key bits.** `KEY_A` and friends are declared on both sides.
- **The platform layer.** `sample_game/sys/` is a byte-for-byte copy of
  `tuto1_hw/sys/`, differing only by two blank lines in `rp_system.h`. A fix
  applied to one does not reach the other.

## Vestigial code

Present, compiled, and doing nothing:

| What | Where | Status |
|---|---|---|
| `ap_game::drawRader()` | ap_game.cpp | Complete 2D radar; its call site is commented out |
| `ap_game::getMission()` | ap_game.h | Declared, never defined |
| `ap_game::initMission()` | ap_game.h | Empty stub |
| `sound_nsf` | ap_data.h | Declared, never defined or referenced |
| `DAM_BG_FLASH` handling | ap_game.cpp | Commented out; the note says the background stuck red |
| `ap_license::op_dt`, `op_cfg[]` | ap_license.cpp | Inherited from ap_option and unused. The table is six pairs against a seven-entry array |
| Console-side screens | `AplTitle.asm` and friends | Reachable only when the ROM runs on real hardware |

@note The per-screen `OBJ_*` enumerations overlap by design, and several screens
reach for another's names -- ap_over and ap_clear both use ap_demo0's
`OBJ_DEM_STAR` and `OBJ_DEM_MODEL`. The values coincide, so it works. It is a trap
for anyone who renumbers one screen's slots.

@warning `ap_license::main()` writes all 1024 bytes of the decompressed page to the
serial port on every frame. That is left-over debug output and it is slow enough to
see.

## What is not in this reference

`sample_game/sys/` is excluded from the Doxygen input. It is the same platform
layer already documented under @ref platform and @ref fcbus, and including the copy
would duplicate every class and make each cross-reference ambiguous. Read those
pages; they describe this tree's `sys/` exactly.

The generated assets -- `res/resdata.c`, `res/res_id.h`, `res/res_id2.h`,
`res/fcpico.c` and the `res/mdl_*.c` model arrays -- are excluded as machine
output, along with `GAMEROM/PG_main.lst`.
