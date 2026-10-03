# World sprite occlusion: NES methods and fc-pico constraints

Date: 2026-10-02. Research and planning only; no mapper or renderer change.

## Recommendation

Prototype **per-instance CHR transparency masks** first: scale the source patch, compute its visible pixels against nearer scene geometry, then clear both NES bitplanes wherever the instance is occluded. Draw surviving tiles with front-background priority and sort actor OAM near to far. This preserves the streamed background's colors and does not need mapper banking or IRQs. It does require a bounded dynamic-CHR upload path, unique tiles for differently clipped instances, and coherent publication of world, CHR and OAM. It is a proposed use of existing interfaces, not a demonstrated throughput result.

The local PPU implementation makes the reason explicit: it first chooses the lowest-index opaque sprite pixel, then tests background priority; a zero sprite pixel lets the background or another sprite win. [Mesen PPU implementation](../sim/mesen2/Mesen2/Core/NES/NesPpu.cpp), [upstream primary source](https://github.com/SourMesen/Mesen2/blob/9688726c982c57cfbcb81bc6d519decfe82eff91/Core/NES/NesPpu.cpp).

## Traditional masking methods

| Method | How occlusion works | Suitability for streamed Doom geometry |
|---|---|---|
| Behind-background actor | Set OAM attribute bit 5; background pattern index zero reveals the actor, nonzero hides it. | Useful only if the background is deliberately encoded as a visibility mask. Ordinary far walls/floor are also opaque and would hide visible actors. |
| Background holes | Clear BG bitplanes at the exact visible actor silhouette and draw that actor behind BG; retain nearer geometry as nonzero pixels. | Possible experiment, but masks must follow the selected instance, scale, position and final visibility. Rectangular holes expose the backdrop around transparent art. If OAM admission fails after holes are cut, holes remain; naturally zero BG pixels leak the actor in occluded regions. |
| Lower-index masking sprite | A behind-BG mask sprite wins against a higher-index front-BG actor; opaque BG then covers the mask, revealing the scene. | Conventional priority quirk, but costs extra entries and scanline slots. Where BG is zero, the mask sprite itself appears. Poor default for a renderer already limited to eight sprites per line. |
| Per-instance tile clipping | Clear actor pixels hidden by nearer geometry before uploading its CHR tiles; drop wholly invisible tiles. | Most general present-board candidate. Correct for moving vertical edges, floor/ceiling boundaries and arbitrary shapes if visibility is computed correctly. Extra CHR storage/upload work replaces mask-sprite overhead. |

The priority behavior and mask-sprite technique are documented by [NESdev's sprite-priority investigation](https://www.nesdev.org/wiki/PPU_sprite_priority), with the project's [PPU source](../sim/mesen2/Mesen2/Core/NES/NesPpu.cpp) as an independently inspectable implementation. Pattern-index zero controls transparency; choosing a visually black palette color does not. A single back/front bit provides no depth test. Likewise PPUMASK's left-edge clipping is an eight-pixel screen-edge control, not a movable window around a corner. [PPU register documentation](https://www.nesdev.org/wiki/PPU_registers).

Clipping pixels does **not** free a scanline slot for a retained tile: the PPU admits sprites by vertical bounds before inspecting their pattern pixels. Omit wholly hidden entries and tighten metasprite bounds where possible, then recompute occupancy from all remaining OAM rectangles. Mask sprites worsen this budget. [Sprite evaluation](https://www.nesdev.org/wiki/PPU_sprite_evaluation), [existing capacity study](12b-sprite-multiplexing-research.md).

## Mapper methods: distinguish storage from visibility

**MMC3:** real hardware supplies 2 KiB/1 KiB CHR bank windows and a qualified PPU-A12 IRQ counter. These can select art banks or synchronize changes between vertical regions; neither adds a depth comparator, horizontal clipping window, sprite units or additional OAM capacity. Replacing a CHR bank changes the pattern data presented at subsequent fetches, rather than masking an arbitrary 3D edge by itself. [MMC3 hardware specification](https://www.nesdev.org/wiki/MMC3), [local primary implementation](../sim/mesen2/Mesen2/Core/NES/Mappers/Nintendo/MMC3.h).

**MMC5:** extended attributes select a palette and 4 KiB CHR bank for each **background tile**. They do not create a per-pixel depth/priority channel. Its fetch-aware sprite/background banking, with the documented 8×16-mode conditions, increases available art but retains normal PPU compositing and sprite limits. It could support a redesigned cartridge's background mask/art organization; it is not an occlusion feature available just by changing a ROM header. [MMC5 hardware specification](https://www.nesdev.org/wiki/MMC5), [local primary implementation](../sim/mesen2/Mesen2/Core/NES/Mappers/Nintendo/MMC5.h).

**Homebrew fetch substitution:** an address-aware cartridge could return scaled, already-clipped sprite row bytes during sprite pattern fetches, avoiding the CPU's dynamic-CHR upload volume. The cartridge would need explicit instance/slot metadata and reliable fetch identification: a CHR address alone need not distinguish two instances sharing a tile, and OAM X/priority information is not automatically exposed as a sprite pattern address. It must also handle dummy fetches, 8×8/8×16 modes, flips and scroll/timing changes. This is an architecture proposal requiring new electrical/protocol/timing proof. It still cannot bypass the PPU's eight admitted sprites. [PPU fetch sequence](https://www.nesdev.org/wiki/PPU_rendering), [primary PPU implementation](../sim/mesen2/Mesen2/Core/NES/NesPpu.cpp).

## What the present fc-pico board actually supports

The shipped bus design deliberately does **not connect PPU address lines**; it consumes the outgoing FIFO on selected read strobes and tracks position by counting them. PA12/PA13 definitions are commented out. The Mesen FcPico adapter models one 8 KiB CHR-RAM page and selects streamed reads by address decoding, normally `$0800..$0fff`; other reads use local CHR RAM. It has no demonstrated MMC3-style banking/IRQ or MMC5-style fetch-substitution interface. [PIO source](../fcbus/fcppu.pio), [FcPico adapter](../sim/mesen2/Mesen2/Core/NES/Mappers/Homebrew/FcPico.h).

Consequently, use existing nonstreamed CHR with bounded updates for the first experiment. Changing sprite tile/table selection can accidentally consume the BG stream and desynchronize its byte count; audit all sprite fetch addresses, including unused/dummy slots and any global 8×16 switch. A new fetch-count-controlled stream layout might be investigated, but it is a separate transport redesign, not existing arbitrary CHR addressing. An emulator hook that inspects PPU cycle/scanline or OAM and supplies clipped rows without corresponding physical signals is **emulator-only evidence**. New wiring/address capture, IRQ output or conventional mapper logic requires a separately specified board/firmware revision.

## Obtain actual scene visibility, then mask

This fork enables `NO_DRAWSEGS`, `NO_VISSPRITES`, `NO_MASKED_FLOOR_CLIP` and `PD_SCALE_SORT`; `R_ProjectSprite` uses `R_DrawSpriteEarly`. Do not base implementation on stock Doom's retained drawseg clip arrays or assume its normal deferred vissprite pass exists here. Inspect the active PD spans/depth ordering and derive per-instance visibility from nearer ownership; if its data is insufficient, budget a new visibility representation explicitly. [Active build configuration](../rp2040-doom/src/CMakeLists.txt), [sprite projection/drawing](../rp2040-doom/src/doom/r_things.c).

Record visibility at the final projected scale and display coordinates, covering opaque walls, upper/lower portal boundaries, floors/ceilings, masked midtextures and sprite/sprite overlaps. Column top/bottom ranges suffice only where visibility is one contiguous interval; disjoint holes need multiple spans or a bitmask. Apply the same generation to background conversion and sprite publication. Remove an actor's streamed drawing only after its sprite admission, visibility, palette and CHR residency are guaranteed; otherwise keep it in the stream for that complete frame.

## Acceptance experiments

1. Compare front-priority clipped CHR, BG-hole/behind-BG and mask-sprite fixtures using the same source-derived actor. Include naturally zero BG pixels, two instances sharing art with different clips, and two actors at different depths behind one corner.
2. Exercise vertical corners, sloped/stepped boundaries, windows with disjoint visibility, masked midtextures, screen edges, near huge actors, and HUD/menu overlap. Confirm no backdrop holes, leaked actor pixels or duplicated streamed actors.
3. Record dynamic tile bytes, upload time, OAM total and all 240 scanline occupancies. Disable emulator sprite-limit removal. Verify actual fetch selection/count and recovery under repeated/stalled frames.
4. Prefer the cheapest correct present-board method; advance any fetch-substitution/mapper candidate only after writing a concrete bus trace and physical signal/timing design. Emulator images alone cannot validate that redesign.
