# World sprite implementation and review gate

Date: 2026-10-02. P3-W1/W2 partial; P3-W3/W4/W5 open.

The session gate is reviewable cosimulation output before building device
firmware. The implementation supplies three scaling candidates using
per-instance CHR transparency masks: resident size bands, a host prototype of
cartridge-side LUT/DDA scaling, and an adapted LUT scaler executed by the NES
6502. These are fixed-generation gameplay probes, not migrated engine actors
or a demonstrated continuous upload protocol.

## Reproduce the samples

Use a legally obtained shareware WAD and an existing host frame. Generated art,
ROMs and images stay outside git under the output directory.

```sh
doom/.venv/bin/python doom/sim/mesen2/run_native_status_probe.py \
  --host-frame /tmp/fcpico-audio-engine-compare/guard-updated/stream/frame000300.bin \
  --output /tmp/fcpico-world-background

doom/.venv/bin/python doom/sim/mesen2/run_world_sprite_probe.py \
  --wad /tmp/fcpico-audio-doom1.wad \
  --background /tmp/fcpico-world-background/frame.bin \
  --output /tmp/fcpico-world-sprites
```

Review `/tmp/fcpico-world-sprites/review.html`, `comparison.png`, `review.md`
and `summary.json`. Each scene/approach directory contains its ROM/source,
stream, visibility masks, independent decoded reference, paired Mesen baseline,
CHR/OAM dumps, bus trace, NMI log, three consecutive captures and report.

## Implemented scope

`tools/build_world_sprite_art.py` validates the WAD directory through the
existing audio reader, retains directory identities, decodes classic patch
posts with signed origins, keeps palette-zero pixels distinct from transparent
posts, and handles paired mirrored rotation names. The reviewed manifest is
`world-sprite-scope.json`: imp (`TROO`) and stimpack (`STIM`). All 54 available
patches and 176 logical frame/view mappings are extracted, including imp death
art. The source and indexed sheets, NES scale-band CHR and hashed manifest
regenerate identically in a separate output directory.

Runtime WHX identities are explicitly unresolved (`null`), rather than treating
sheet order or raw WAD lump order as an engine index. Engine state-table coverage,
lighting/fullbright treatment, translation/shadow effects, wider actor scope,
flipped runtime anchors and cache churn still require integration work.

The LUT implementation packs contiguous destination slivers using global
integer sampling coordinates. It supports scales below half-size without
8-pixel-cell gaps. An independent scalar sampler checks every generated scale
band and each sample's chosen size. The 6502 control evaluates generated LUT
routing and mask recipes on the console CPU; py65 separately executes the same
assembled routine and checks every emitted byte. It adapts the reference
`scaling-wip` byte-LUT concept; it is not the original demo or its cycle schedule.

## Measured samples

All five scenes run for 180 PPU frames for each approach, with sprite-limit
removal disabled. Every sample has 30 stable post-startup heartbeats at 15,122
reads, no DMA stops, exact CHR/OAM, exact changed-pixel geometry against an
independent decoded image and a paired baseline, and identical last three
captures. NMI exits remain on scanline 256. Fixed-bank checks pass for every
assembled probe. The sample mailbox has no active audio sequence; this does
not establish the combined runtime audio/CHR upload gate.

| Scene | Resident OAM / CHR bytes | LUT OAM / CHR bytes | Actual combined peak line | LUT changed pixels | 6502 scaler cycles (py65) |
|---|---:|---:|---:|---:|---:|
| Open | 20 / 320 | 22 / 352 | 6 | 622 | 55,036 |
| Corner | 17 / 272 | 19 / 304 | 6 | 468 | 47,537 |
| Disjoint window | 18 / 288 | 20 / 320 | 6 | 413 | 50,033 |
| Crowded | 24 / 384 | 24 / 384 | 8 | 686 | 60,035 |
| Near view | 8 / 128 | 10 / 160 | 6 | 254 | 25,043 |

The two LUT approaches produce identical images. The far imp requests width 13;
resident bands choose width 12, exposing size quantization for review. Admission
retains the 35-entry UI partition and gives world sprites 29 slots. Crowded
views admit four of seven actors; three remain wholly in the fixture stream.
The near 64-pixel imp stays wholly in the fixture stream. The fixture fallback
uses indexed art and the existing BG palette; it is not Doom's software render
or evidence of restored geometry behind suppressed actors. The recorded Doom
background may contain its original streamed actors. Probe actors have explicit
fixture coordinates and controlled masks, not engine-derived visibility.

The generated imp/stimpack resident bands occupy 29,360 bytes before tile
sharing. Deduplicating exact tiles separately within each frame/band would
reduce that to 27,936 bytes; cross-frame/flip sharing is not implemented. Host
Python timing fields are diagnostics, not RP2350 measurements. The 6502 open
control uses 4,096 LUT bytes and 2,464 recipe bytes, plus routine code. Its setup
cycles include PPU writes with rendering off. Continuous scaling, tile reception
and upload during gameplay are unproven.

## Storage and palette findings

The probe borrows `$1380..$15FF`: 40 tiles between arms and logo data. **This
includes inherited menu font glyphs, so it is gameplay-only borrowing, not a
permanently free atlas.** Menu transitions must restore those glyphs, or a
future allocator must select different storage. World OAM is hidden during
menus by the generated routine, but these samples do not validate restoration.
Face art at `$1800..$1FFF`, face overflow at `$1280..$12DF`, keys at
`$12E0..$130F`, arms at `$1320..$137F`, logo at `$1600..$17FF`, small menu logo
at `$1130..$127F`, and menu font ownership must all participate in a mode-aware
CHR map. The held weapon still lives in the background stream.

The enemy shares face subpalette 1 (`07 18 26`); the pickup shares arms/key
subpalette 3 (`12 38 16`). No fifth palette is assumed. Gameplay masks clip at
Y=192, keeping world tiles away from the status display. The allocator reserves
a conservative fully active HUD occupancy of eight; observed occupancy with the
sample UI packet is six except in the crowded scene. Every retained tile costs
its entire eight-line rectangle even when partially transparent.

At the crowded sample's 384 unique bytes, two dynamic tile buffers require
768 bytes before metadata. Reserving two full 40-tile regions requires 1,280
bytes, which the present probe has not allocated. Replacing all 384 bytes at
20 Hz would be 7,680 bytes/s plus OAM/descriptors/palette work. These calculated
numbers do not prove that receive/write bursts fit vblank. Current sample
uploads occur before rendering starts and add no runtime CHR work to NMI.

## Alternatives excluded from general present-board migration

| Alternative | Reason samples are not advanced |
|---|---|
| Ordinary behind-BG priority | Far walls and floors also have opaque BG pixels, so visible actors disappear. |
| BG holes / behind-BG | Nearer geometry's naturally zero BG pixels leak hidden actor art; enforcing nonzero geometry changes the four-color stream contract. |
| Lower-index occluder sprites | Same zero-BG leak; additional OAM and line capacity, with crowded output already at eight. |
| Tile-aligned cropping | Loses visible portions of partially clipped tiles at thin corners and disjoint windows. |
| Traditional banking / raster IRQ | Helps art storage but supplies no pixel depth; the current board does not expose this mapper interface. |
| Fetch substitution | No demonstrated physical fetch/address/instance signals or deadline proof; an emulator hook would not establish board viability. |

The geometry predicates above are the exclusions documented in plan 14a, not
new electrical experiments. Special-purpose constrained-color BG masks or
extra OAM can still be separate experiments; these exclusions concern arbitrary
streamed Doom geometry on the current transport.

## Renderer integration seam and remaining work

Source inspection confirms `R_ProjectSprite` already resolves patch, flip,
projection and light before `R_DrawSpriteEarly`. `pd_column` is 12 bytes and
has no actor-owner field. `push_down_x_guts` destructively frees, clips and
splits obscured columns. In this representation a smaller stored scale is in
front. Simply suppressing final actor rasterization cannot reconstruct discarded
geometry. A sidecar owner ID must also propagate through splits and allocation
reuse; an ownership tag alone still cannot restore underlying geometry.

Budget a geometry/streamed-actor pass that omits provisionally admitted actors
before destructive insertion, plus a visibility query/pass retaining the
geometry used to clip their art. Resolve admission before final background
publication; a failed admission must rerender that whole actor/background
generation through the existing path. Visplanes, masked texture holes, nearer
streamed actors and the held weapon must enter the visibility query. Compare
this with preserving underlying spans before selecting the production seam.
The RP2350 configuration has a 7,200-column pool: a two-byte owner sidecar
costs 14,400 bytes and a second 12-byte column pool costs 86,400 bytes before
other state. The non-RP2350 3,600-column pool costs half those amounts. Neither
extra representation has been reserved or measured on RP2350.

Masks must be mapped through the same converter sampling as the background:
source X `(x / 4) * 5 + (x & 3)`, source Y `floor(d * 200 / 224)` with an
eight-line top margin. The present fixtures start in native coordinates and
therefore do not validate a source-to-native visibility seam.

After visual review: choose the scaling/palette treatment, resolve WHX identities
and renderer ownership, measure bounded cache/mask costs and zone headroom,
implement coherent runtime transport with tile double buffering and menu font
restoration, then exercise animation/views, moving geometry, actual software
fallback, transitions/stalls and combined audio/NMI. Device firmware and
physical acceptance follow that review; no P3-W3/W4/W5 completion is claimed.
