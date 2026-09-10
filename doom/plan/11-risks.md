# 11 -- Risks, open questions, decisions for a human

## Risk register

| ID | Risk | Likelihood | Impact | Mitigation / fallback | Trigger to act |
|----|------|-----------|--------|-----------------------|----------------|
| R1 | The PPU-bus model cannot be calibrated because the count discrepancy in 01 has an explanation the trace does not reveal (e.g. board-level CS1 gating). | Medium | Model goldens are regression-only; co-sim may pass while hardware fails | Keep L7 checks per milestone; make the mapper's `cs1_mask` and per-line read counts data-driven from the trace rather than derived. The stream layout is fixed regardless (01, prior art). Fallback sync source: vblank-gap detection as PiPU's FX2 does (09). | P0-T10 cannot fit the trace |
| R2 | NMI does not fit vblank with the full v2 mailbox on real hardware (cycle model wrong, DPCM DMA stalls add cycles). | Medium | Tearing / dropped attribute updates | v2.1 conditional attribute block; half table per frame; APU cap 12; measure with py65 and Mesen before hardware | P2-T2 test > 1900 or S2 NMI scanline >= 261 |
| R3 | Frame rate below 15 fps on RP2350 at 150 MHz with the added conversion. | Low-Medium | Unplayable feel | Overclock (P5-T1, precedent 270-276 MHz); converter optimisation; reduce `RENDER_COL_MAX`; drop `NO_DRAW_*` features | P1-T8 fps < 15 |
| R4 | 4 MB flash assumption wrong (smaller). | Low | WHX does not fit | Measure at P0-T9; if 2 MB, fall back to RP2040 Doom's super-tiny layout (`0x10040000`) and a 256 KB firmware cap | `flash_get_size()` |
| R5 | Text (menus, HUD) unreadable after 5:4 decimation + 2-bit quantisation. | Medium | Menus unusable | Preset B (shared white); Phase 5 custom HUD; larger-font menu patch in the engine (`M_Drawer` at 8x8 nearest-neighbour is already 1:1 at 256 width if drawn after decimation) | P2-T6 legibility metric |
| R6 | Licensing: impact soft's terms for the bus layer are not GPLv2-compatible. | Medium | Cannot publish combined firmware | `fcbus` is a re-implementation from the documented protocol; `fcppu.pio` is Raspberry Pi BSD-3 with small impact soft edits -- rewrite those lines if needed; ask early (P0-T13) | Reply from impact soft |
| R7 | `nesasm.exe` under Wine misbehaves in CI (32-bit packages on GitHub runners). | Low | Boot ROM not buildable in CI | Docker image with `wine32`; native NESASM_X86 2.51 rebuild validated by the MD5 gate; `ca65` port as last resort | `bootrom` job red |
| R8 | Mesen2 build in CI too slow or its Lua API differs from the plan. | Medium | Co-sim delayed | Cache the build by commit; verify API names at P0-T11 against `LuaDocumentation.json` of the pinned commit; keep scenarios small | P0-T11 > 20 min per run |
| R9 | The engine's RP2350 short-pointer scheme (`SHORTPTR_BASE 0x20030000`) conflicts with our static buffers pushing `__end__` past the 256 KB heap window or below the base. | Medium | Boot assert / small zone | Place `fcbus`/`fcvideo` buffers in `.uninitialized_data` above `0x20070000` via a linker section; adjust `memmap_doom.ld` copy for rp2350 | link map shows zone < 200 KB |
| R10 | Clone consoles (and some real ones) do not run the FC PICO technique at all. | Known | Cannot fix in software | Document; test on a genuine Famicom first | -- |
| R11 | DPCM playback corrupts controller reads on pad 2 (pad 1 is protected by the fix bank's majority vote). | Medium | Wrong inputs during gunfire | Same 4x vote for pad 2 in the main bank; Mesen reproduces the conflict so S4/S3 can catch it | S3 with DPCM active |
| R12 | Audio arrangement quality is poor with `--auto`. | Medium (was High) | Sounds bad | Start from PiPU's `DOOM.ftm` (Intro, Inter, E1M1-E1M4, E2M1, E3M1 already arranged for the 2A03, GPL); human-tuned FamiStudio projects only for the remaining tracks | listening session |
| R13 | The converter's IRQ context on core 1 starves the renderer's core 1 share (visplanes) and lowers fps. | Medium | fps loss | Measure; alternative: run conversion in `pd_core1_loop()`'s wait loops instead of an IRQ; or split conversion into half-frames | P1-T9 |
| R14 | `pico-sdk` host platform cannot build the engine without `pico_host_sdl`. | Medium | No host runs / no co-sim | Grow `sim/host_shim/` (it is a few hundred lines: multicore via pthreads, sems, alarms) | P0-T3 |
| R15 | The self-reflash takes the console through erase/program with the mailbox stream stopped; a power loss mid-way leaves `IS_ROM_ERACE` set -- recoverable by design, but users may panic at "ROM UPDATE". | Low | Support burden | Document; the fix bank retries automatically | -- |

## Open questions (answer and record in the relevant document)

| ID | Question | Owner | Where the answer goes |
|----|----------|-------|-----------------------|
| Q1 | Exact per-line qualifying-read count and CS1 decode (see 01). | P0-T9/T10 | 01, `sim/ppubus` defaults |
| Q2 | Flash size and part; does `flash_get_size()` (SFDP) work on it? | P0-T9 | 01, 08 |
| Q3 | Does the engine's host build work with pico-sdk's `host` platform plus a shim, without SDL? Partly answered: the base host platform has no `pico_multicore` implementation (headers only) and no alarms; `sim/host_shim/` supplies them (08). Remaining: confirm by linking. | P0-T3 | 08 |
| Q4 | Mesen2 pinned commit: does `MapperReadVram` see rendering reads with a distinguishing `MemoryOperationType`? (Lua API names are now recorded in 09: `setInput`, `read` with `nesPpuDebug`, `addMemoryCallback`, `stop`.) | P0-T11 | 09 |
| Q5 | FamiStudio CLI: exact option names for MIDI import and VGM export; does it run headless on Linux CI (.NET)? The docs site was unreachable when planning. | P4-T3 | 06 |
| Q6 | nesdev facts assumed in 01 (2-dot access, `/RD` width, pre-render fetches): confirm against the wiki and the trace. | P0-T10 | 01 |
| Q7 | `NO_USE_EXIT` behaviour in the fork: does `M_QuitDOOM` reroute cleanly? | P3-T5 | 05 |
| Q8 | Is `DEMO1_ONLY` needed for size on RP2350 with the fcpico target? | P0-T2 | 08 |
| Q9 | Does the engine's `PICO_RP2350` path already move the `stbar` cache out of scratch X, and is scratch X free for a 4 KB core 1 stack? | P1-T3 | 01, 02 |
| Q10 | Does Mesen2 emulate the DMC DMA / controller-read conflict? nesdev discussions recommend Mesen's development builds precisely for this glitch, so yes in principle; confirm on the pinned commit with a `$4016` read breakpoint conditioned on `IsDma`. Note the hardware rule: no glitch when `(DMA address & 0x401F) == 0x4016`. | P4-T5 | 09 |
| Q11 | Which of PiPU's eight arrangements are complete and loop correctly; are instruments 2A03-only (no expansion chips)? | P4-T4 | 06 |
| Q12 | Which two-tile pair per line is uncounted by `fcppu_rna` (prefetch or off-screen), and is it the same on every console? | P0-T9 | 01 |

## Decisions that need a human

| ID | Decision | Why the agent must not decide alone |
|----|----------|-------------------------------------|
| H1 | Contact impact soft about licensing and about redistributing a modified boot ROM. | External relationship |
| H2 | Whether to publish `doom1.whx` in release artifacts or require users to convert `DOOM1.WAD` with `whd_gen`. | Distribution terms (RP2040 Doom ships it, precedent) |
| H3 | Palette preset choice when the metrics disagree with taste. | Aesthetic |
| H4 | Overclock level for release (thermal/reliability on the cartridge). | Hardware risk |
| H5 | Any change to the fix bank (never planned). | Irreversible in the field |
| H6 | Music arrangements. | Aesthetic |
