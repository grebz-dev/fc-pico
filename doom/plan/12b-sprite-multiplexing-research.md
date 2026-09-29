# Sprite multiplexing research for the concrete HUD

Date: 2026-09-28. Planning/research only; no production renderer changes.

## What can actually increase capacity?

The NES PPU selects the first eight OAM entries whose **vertical range** intersects the next scanline. It loads eight output units; later entries cannot render that line. Selection does not consider whether the tile row is transparent or whether another sprite covers it. Transparent padding therefore still consumes capacity. This behavior was investigated through timed hardware reads and confirmed with Visual 2C02. [PPU sprite evaluation](https://www.nesdev.org/wiki/PPU_sprite_evaluation)

| Technique | Useful saving | Does it increase eight-per-line capacity? |
|---|---|---|
| 8×16 sprites | Half the OAM entries for a solid 32×32 face: eight instead of sixteen | No: face still uses four of eight slots on every intersected line |
| Horizontal/vertical flipping | Reuse matching CHR art | No OAM or per-line saving |
| Pack two narrow glyphs into one 8-pixel tile | Fewer slots than one sprite per character | Yes relative to an inefficient glyph layout, but still only eight tiles per line |
| Change OAM order each frame | Share omission fairly | No: trades permanent omission for flicker |
| Different prebuilt OAM pages each frame | More intentional temporal scheduling | No: only one page is visible per frame |
| CHR bank switching / uploads | More animation frames or text art in limited pattern memory | No additional sprite output units |
| Midframe OAM replacement | Reuse 64 entries in separate vertical screen regions | No additional slots on the same line |
| Native background tile composition | Use the separate background renderer | Yes, for total UI pixels; text is then background, not OAM |

The PPU supports 8×8 or 8×16 sprite mode, selected globally. It has no hardware 16×16 or 32×32 sprite mode: those sizes are metasprites made from multiple entries. In 8×16 mode the tile-number low bit selects the pattern table and the remaining index selects a tile pair. Flipping transforms stored art without merging two displayed entries. [PPU OAM](https://www.nesdev.org/wiki/PPU_OAM), [PPUCTRL bit definitions](https://www.nesdev.org/wiki/User:Lidnariq/Microchip-style_PPU_documentation)

## Temporal cycling worth testing

Changing OAM order is the conventional way to replace fixed dropout with controlled flicker. A developer discussion describes both object-order cycling and deliberate multi-frame patterns; this is first-person technique reporting, not evidence of greater hardware capacity. [Controlled sprite flicker](https://forums.nesdev.org/viewtopic.php?t=18010)

For this HUD, explicitly schedule whole fields or 8-pixel columns. Do not blindly rotate individual tiles: a percentage must not temporarily lose a digit or percent sign while the remainder appears authoritative. Keep all pages derived from one coherent semantic snapshot.

Calculated ideal bounds at approximately 60 Hz display refresh:

| Per-line design | Pages | Per-field refresh / duty |
|---|---:|---|
| Sixteen columns, no permanently reserved sprites | 2 | 30 Hz / 50% |
| Four stable face columns plus eight cycling text columns | 2 | 30 Hz / 50% for text |
| Four stable face columns plus twelve cycling text columns | 3 | 20 Hz / 33% for text |
| Four stable face columns plus twenty-eight cycling columns | 7 | 8.6 Hz / 14% for text |

These are mathematical bounds, not measured visual quality. General formula with `F` fixed slots and `C` cycling columns on the busiest line: `pages >= ceil(C / (8-F))`. Uneven spatial distribution, whole-field grouping, and 64-entry page limits may require more pages. PAL rates are lower. Derivation uses hardware capacity above and frame timings from the [cycle reference chart](https://www.nesdev.org/wiki/Clock_rate).

Cycle on each **NES display NMI**, not each approximately 39 ms Doom conversion. Advancing only when a new converted frame arrives would make two-page text roughly 12.8 Hz or worse. Probes must also test stalled/repeated host frames. The 39 ms figure is the user's supplied hardware log, not a new timing measurement.

Four rows of 8×8 sprites provide at most 32 simultaneously displayed entries within a 32-pixel-high band when rows do not overlap. Making sprites 8×16 gives two rows and only sixteen entries in that band, but the same total pixel coverage. A 32×32 face occupies half the horizontal capacity across that entire band. Vertical staggering can redistribute pressure if the visual layout allows it; taller sprites do not create more horizontal bandwidth. These are consequences of the selection rule, not implementation assumptions.

## Priority and raster tricks

Front/behind-background priority changes final compositing, not sprite selection. Lower-index sprites win overlapping opaque pixels even if their background priority subsequently hides them. This can create masks, but cannot make a ninth sprite visible. An outline drawn as a second overlapping sprite makes pressure worse; put the outline in the same tile palette. [PPU sprite priority](https://www.nesdev.org/wiki/PPU_sprite_priority)

OAM writes while rendering is enabled do not provide a reliable live display-list patch: OAMDATA writes fail to replace OAM and disturb its address. Disabling only sprites does not stop evaluation while the background is enabled. Midframe replacement therefore requires forced blanking, carefully controlled restart, and scroll restoration. [PPU programmer reference](https://www.nesdev.org/wiki/PPU_programmer_reference)

A full OAM DMA costs 513 or 514 CPU cycles. NTSC has 113⅔ CPU cycles per scanline, so DMA alone occupies about **4.52 scanlines**, before setup/restart overhead. It cannot fit in horizontal blank. At best, a deliberate blank seam can split two independently populated vertical regions; each remains limited to eight sprites per line. This does not solve dense side-by-side status fields. [DMA](https://www.nesdev.org/wiki/DMA), [cycle reference chart](https://www.nesdev.org/wiki/Clock_rate)

Midframe rendering toggles and OAM address changes also expose hardware-dependent OAM corruption. Treat emulator-only success as insufficient evidence for a portable raster scheme. [PPU errata](https://www.nesdev.org/wiki/Errata)

## Pattern paging and project constraints

MMC3 supplies actual CHR bank controls and scanline IRQs; those are mapper features, not capabilities every NES cartridge inherits. Bank switching can expose more faces or change art by vertical region, but still feeds the existing eight sprite units. [Programming MMC3](https://www.nesdev.org/wiki/Programming_MMC3)

The current fc-pico Mesen mapper selects one 8 KiB CHR-RAM page and its write-register handling models flash commands. The project has **no demonstrated arbitrary CHR banking or scanline IRQ facility** for this HUD. Do not borrow MMC3 capabilities into the plan without a separate hardware/transport investigation. See [FcPico.h](../sim/mesen2/Mesen2/Core/NES/Mappers/Homebrew/FcPico.h) and the assumptions in [12-sprite-ui.md](12-sprite-ui.md).

Dynamic CHR uploads remain a possible residency optimization: load a selected face into a fixed tile region rather than retaining every face. That requires independent byte/timing accounting and atomic publication with its OAM references. It solves art storage, not scanline overflow.

## Proposed bounded experiments before choosing the final renderer

1. Build an exact per-scanline budget for a **32-pixel** status bar as well as the prior 48-pixel proposal. Test actual packed glyph widths, empty-column omission, and 8×16 tile-pair organization. Count sprite bounds, not just opaque pixels.
2. Create isolated Mesen fixtures for stable native background, two-page whole-field cycling, and three-page cycling. Keep the face stable initially; a separate fixture may cycle it to show the visual cost. Do not change the production HUD or produce UF2s.
3. Capture consecutive raw frames, page IDs, OAM/CHR/palette state, and an animation at the real display cadence. Include 0%, 100%, 200%, changing three-digit ammo, full keys, and all weapons. A temporal-average PNG may illustrate placement, but must never stand in for actual flicker validation.
4. Enforce eight-per-line behavior with emulator sprite-limit removal disabled. Require all page entries to fit 64 slots and ensure page cycling continues during host stalls and menu transitions.
5. Compare readability and perceived flicker interactively. Select temporal cycling only if its visible tradeoff is accepted; retain stable native-background digits/labels as the fallback. Investigate midframe DMA only if the blocker is total OAM across disjoint regions, not concurrent scanline demand.

No final renderer decision follows from static PNGs alone. The current research establishes what each technique can save; Mesen motion and eventual display behavior determine whether temporal multiplexing is acceptable.
