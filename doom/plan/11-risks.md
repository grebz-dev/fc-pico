# 11 -- Risks, open questions, decisions for a human

## Risk register

| ID | Risk | Likelihood | Impact | Mitigation / fallback | Trigger to act |
|----|------|-----------|--------|-----------------------|----------------|
| R2 | NMI does not fit vblank with the full v2 mailbox on real hardware (cycle model wrong, DPCM DMA stalls add cycles). | Medium | Tearing / dropped attribute updates | v2.1 conditional attribute block; half table per frame; APU cap 12; measure with py65 and Mesen before hardware | [P2-T2](10-workplan.md#p2-t2-v2-nmi-init-main-loop-controller-packet) test > 1900 or S2 NMI scanline >= 261 |
| R3 | Measured conversion cost limits frame rate: ~35.6 ms average/max on NES-001 (2026-09-24), above the 8 ms target. | Observed | Reduced display cadence and input feel | Profile and optimize stage C/D and core-1 scheduling in [P1-T9](10-workplan.md#p1-t9-converter-performance-pass); remeasure on hardware before considering overclock | Conversion max remains > 8 ms |
| R4 | The physical flash part/size is not yet independently identified. | Low | Future WHX layouts may not fit | Read the part ID and SFDP in a later hardware session; current merged UF2 boots | Flash layout check or device programming fails |
| R5 | Text (menus, HUD) unreadable after 5:4 decimation + 2-bit quantisation. | Medium | Menus unusable | Preset B (shared white); Phase 5 custom HUD; larger-font menu patch in the engine (`M_Drawer` at 8x8 nearest-neighbour is already 1:1 at 256 width if drawn after decimation) | [P2-T6](10-workplan.md#p2-t6-palette-preset-evaluation) legibility metric |
| R6 | Licensing: impact soft's terms for the bus layer are not GPLv2-compatible. | Medium | Cannot publish combined firmware | `fcbus` is a re-implementation from the documented protocol; `fcppu.pio` is Raspberry Pi BSD-3 with small impact soft edits -- rewrite those lines if needed; ask early ([P0-T13](10-workplan.md#p0-t13-licensing-and-attribution)) | Reply from impact soft |
| R8 | MesenCE build or scenario matrix takes too long in remote CI. | Medium | Co-sim gate delayed | Cache by pinned commit; local strict S0 and fixed-frame D0/D1 already run, while dynamic S2 remains | Remote lane exceeds 20 min |
| R9 | The engine short-pointer zone could be crowded by static bus/video buffers. | Reduced | Boot assert / small zone | Device flash/RAM layout gate currently passes with 128 KB zone and >24 KB post-zone margin; rerun after buffer growth | Layout check fails |
| R10 | Clone consoles (and some real ones) do not run the FC PICO technique at all. | Known | Cannot fix in software | Document; test on a genuine Famicom first | -- |
| R11 | DPCM playback corrupts controller reads on pad 2 (pad 1 is protected by the fix bank's majority vote). | Medium | Wrong inputs during gunfire | Same 4x vote for pad 2 in the main bank; Mesen reproduces the conflict so S4/S3 can catch it | S3 with DPCM active |
| R12 | Audio arrangement quality is poor with `--auto`. | Medium (was High) | Sounds bad | Start from PiPU's `DOOM.ftm` (Intro, Inter, E1M1-E1M4, E2M1, E3M1 already arranged for the 2A03, GPL); human-tuned FamiStudio projects only for the remaining tracks | listening session |
| R13 | The converter's IRQ context on core 1 starves the renderer's core 1 share (visplanes) and lowers fps. | Medium | fps loss | Measure; alternative: run conversion in `pd_core1_loop()`'s wait loops instead of an IRQ; or split conversion into half-frames | [P1-T9](10-workplan.md#p1-t9-converter-performance-pass) |
| R15 | The self-reflash takes the console through erase/program with the mailbox stream stopped; a power loss mid-way leaves `IS_ROM_ERACE` set -- recoverable by design, but users may panic at "ROM UPDATE". | Low | Support burden | Document; the fix bank retries automatically | -- |

The former PPU-count calibration, host shim, Mesen mapper API and native NESASM
risks have been resolved by trace 6, strict S0, host engine tests and the tutorial MD5
gate. Keep their historical evidence in [`../PROGRESS.md`](../PROGRESS.md).

## Open questions (answer and record in the relevant document)

| ID | Question | Owner | Where the answer goes |
|----|----------|-------|-----------------------|
| Q2 | Flash size and part; does `flash_get_size()` (SFDP) work on it? The current merged UF2 boots, but no part ID is recorded. | Later hardware session | 01, 08 |
| Q5 | FamiStudio CLI: exact option names for MIDI import and VGM export; does it run headless on Linux CI (.NET)? The docs site was unreachable when planning. | [P4-T3](10-workplan.md#p4-t3-audio-tools----spec-06-pipeline-size-l-depends----acceptance-teststools-round-trips-mus2apuspy---auto-produces-all-13-streams-from-doom1wad-under-the-cap) | 06 |
| Q7 | `NO_USE_EXIT` behaviour in the fork: does `M_QuitDOOM` reroute cleanly? | [P3-T5](10-workplan.md#p3-t5-quit-console-reset-heartbeat-timeout----spec-05-03-state-machine-size-s-depends-p2-t3-acceptance-host-fault-injection-tests-hw-pressing-the-consoles-reset-returns-to-the-title-within-5-s) | 05 |
| Q10 | Does Mesen2 emulate the DMC DMA / controller-read conflict? nesdev discussions recommend Mesen's development builds precisely for this glitch, so yes in principle; confirm on the pinned commit with a `$4016` read breakpoint conditioned on `IsDma`. Note the hardware rule: no glitch when `(DMA address & 0x401F) == 0x4016`. | [P4-T5](10-workplan.md#p4-t5-dpcm-bank-and-boot-rom-v3----spec-06-07-size-m-depends-p2-t2-p4-t3-acceptance-dpcm_packpy-budget-check-stamp-bump-nmi-cycle-test-unchanged-co-sim-controller-input-stays-correct-while-dpcm-plays-mesen-emulates-the-dmccontroller-conflict) | 09 |
| Q11 | Which of PiPU's eight arrangements are complete and loop correctly; are instruments 2A03-only (no expansion chips)? | [P4-T4](10-workplan.md#p4-t4-arrangements----size-l-human-assisted-depends-p4-t3-acceptance-famistudio-projects-for-d_e1m1-d_intro-d_introa-d_inter-committed-vgm-export-reproducible-via-cli-others-auto) | 06 |

Q1, Q3, Q4, Q6, Q8, Q9 and Q12 were answered by the NES-001 trace and
working host, Mesen and device builds. The remaining rows are unresolved.

## Decisions that need a human

| ID | Decision | Why the agent must not decide alone |
|----|----------|-------------------------------------|
| H1 | Contact impact soft about licensing and about redistributing a modified boot ROM. | External relationship |
| H2 | Whether to publish `doom1.whx` in release artifacts or require users to convert `DOOM1.WAD` with `whd_gen`. | Distribution terms (RP2040 Doom ships it, precedent) |
| H3 | Palette preset choice when the metrics disagree with taste. | Aesthetic |
| H4 | Overclock level for release (thermal/reliability on the cartridge). | Hardware risk |
| H5 | Any change to the fix bank (never planned). | Irreversible in the field |
| H6 | Music arrangements. | Aesthetic |
