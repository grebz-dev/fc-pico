# Reference-driven HUD layout comparison

Date: 2026-09-30
Status: four static Mesen variants captured; fc-pico transport integration remains open.
Executor: GPT-6-Sol. Work sequentially through the bounded steps below.

## Objective and scope

Produce at least four distinct, NES-feasible HUD outcomes, captured from Mesen,
using the user's edited frame as the authority for placement, scale, styling,
and color intent. Compare sprite/background allocation and palette tradeoffs.
Deliver review images and reproducible fixtures before selecting a production
layout. This pass is a plan only. The subsequent experiment should not build a
UF2 or replace the accepted firmware automatically.

References (paths relative to the repository):

- `doom/assets/hud_edit_reference/full-frame-reference-edited.png`: desired
  256x240 composition; preserve the original file.
- `doom/assets/doomguy_faces.png`: preferred 24x24 face cells, arranged 7x6;
  preserve their expressions and warm coloring.
- `doom/assets/hud_edit_reference/full-frame-reference.png`: existing Mesen
  output, useful for a before/after comparison.
- `doom/StatusBar.png`: original DOOM bar; use to check proportions, outlines,
  compartment borders, weapon boxes, and the black face recess. Do not shrink
  this entire image and call it the native result.

## 1. Identify and measure the target

The HUD occupies y=192..239: 256x48 pixels, six 8-pixel tile rows. The checkerboard
above it is a comparison backdrop, not requested game artwork.

| Element | Approximate target area (inclusive x) | Appearance and behavior |
|---|---|---|
| Current ammo | 0..30 | Red outlined `20`; small white AMMO below; number changes with weapon/ammo |
| Health | 31..73 | Red outlined `100%`; small white HEALTH below |
| ARMS | 74..108 | Two rows of three boxed numbers, 2/3/4 then 5/6/7; yellow selected/acquired emphasis, white and red details; white ARMS below |
| Face | 109..146 | Tall face centered in a black recess, roughly 32x40 visible canvas; preserve idle, injury and pickup expressions |
| Armor | 147..182 | Red outlined `75%`; small white ARMOR below |
| Keys | 183..194 | Three vertically stacked blue/yellow/red icons in small slots |
| Inventory | 195..255 | Four compact rows: BULL, SHEL, RCKT, CELL; current/max values and slash; mostly white with color accents |

Measure exact non-background bounds from the edited PNG before locking the
coordinates. These are starting estimates, not a prescription to move the
user's artwork. Record border, glyph, baseline, face and key bounds separately.
The inventory sample values are artwork examples; actual gameplay capacities
must continue to come from the engine, including backpack changes.

Produce an annotated reference image with these boxes, an 8x8 tile grid and a
16x16 attribute grid. Preserve an unannotated copy for judging the result.

## 2. Normalize assets and respect the actual constraints

- Treat the edited RGB colors as intent. Normalize them to explicit NES palette
  indices and record the mapping. Use the same Mesen palette/settings for all
  captures. Do not silently use an approximate RGB preview as emulator evidence.
- Background: four palettes total, each with the shared backdrop plus three
  colors. Assign one palette per 16x16 attribute region. A border drawn at an
  arbitrary pixel x does not move that palette boundary.
- Sprites: four separate palettes, each three opaque colors plus transparency;
  64 OAM entries total, eight entries on any scanline. Count transparent parts
  of sprites too. Keep Mesen sprite-limit removal disabled.
- The current HUD BG palette is `$0F,$00,$30,$16` (black, gray, white, red).
  Keep this as the first panel candidate. Test a warm face palette drawn from
  NES browns/red-browns/skin colors against the preferred face sheet; avoid
  choosing the old olive palette simply because it is already wired up.
- Plan one sprite palette containing blue/yellow/red for colored keys and
  weapon highlights. Gray/white slots and outlines can be drawn underneath on
  the background. A sprite palette with blue/yellow/red cannot also supply an
  opaque black outline; that outline must come from another layer or replace
  one of those colors.
- Keep major values red with black outlines, small labels white, the face recess
  black, and the panel gray. Start with the edited low-noise panel. Optional
  concrete grain must not erase outlines or compete with the small inventory.
- Align asset canvases to 8 pixels, but allow glyph pixels to start inside tiles.
  Packed background strings can cross tile boundaries. Do not snap every letter
  to an 8-pixel advance: that would destroy the user's compact layout.
- Use nearest-neighbor resampling on indexed face pixels, with a documented
  sampling rule. Compare 24x40 (vertical stretch only) and 32x40 (wider canvas
  matching the target). Use the selected width consistently across variants
  unless width is explicitly the tested difference. Keep black padding around
  the art and no overlap with labels/borders.
- Export the complete normalized face sheet as editable PNG plus palette-index
  data and cell/name metadata. Inspect idle, ouch, pickup, dead and god faces.
  For the static comparison only the chosen face needs to be resident; separately
  report the storage required for the full animation set.
- Deduplicate identical sprite tiles including horizontal/vertical flips.
  Flips save CHR storage, not the number of OAM placements. NES background tiles
  have no per-tile flip attribute: store the transformed pattern when necessary.
- In 8x16 mode the mode applies to every sprite. Pack legal tile pairs, account
  for the pattern-bank bit in the tile number, and deduplicate whole pairs.
  It saves vertical OAM entries, not scanline slots.

Store coordinates, palette choices and layer ownership in one small JSON
manifest. Avoid a generalized graphics framework. A Python generator using the
existing Pillow/NumPy tooling and a small ROM fixture is enough.

## 3. Four required variants

All variants use identical values, expression, viewport, panel height and
capture settings. Prefer full target color where feasible; label every
compromise explicitly. Each variant must have its own actual Mesen capture.

| ID | Allocation | What it tests | Expected constraint |
|---|---|---|---|
| A — faithful hybrid | Panel, outlined values, labels, inventory and weapon/key box outlines in BG palette 3; 32x40 face in 8x8 sprites; colored weapon/key pixels as sprites | Closest match while retaining current three world BG palettes | Face uses 20 entries and four slots per line. Six weapon sprites in two rows plus three nonoverlapping key sprites give a provisional 29 entries and peak eight per line (4+3+1). Verify generated bounds, not just this estimate. |
| B — tall sprite pairs | Same visual target and palettes as A, but all sprites use 8x16 mode | Whether the face can retain the same appearance with lower OAM usage | A 32x40 face padded to 32x48 uses 12 entries. Repack weapon/key pixels carefully: 16-pixel coverage can make neighboring rows overlap in sprite evaluation, even when pixels are transparent. Move rows or combine them in one pair as needed and report shifts. |
| C — background face | Panel in one BG palette; black face recess in a second warm BG palette; weapon/key color accents remain sprites | Remove the face from OAM while retaining warm skin and colored accents | Reserve face attribute columns x112..143 for y192..239; fit art inside. At least two BG palettes now serve the HUD, leaving at most two unrestricted world palettes without a split. Face-region borders must also use the face palette. |
| D — all background | No HUD sprites; dedicate BG palettes to panel, face, weapon accent and key/inventory treatment | Static UI entirely in BG with attribute-aligned compartments | Four BG palettes are shared with the world. A narrow three-color key stack cannot independently select a palette for every 8-pixel row; use a documented common key palette or expand/reposition slots to 16x16 regions. Show the resulting loss of panel colors/space honestly. |

For D, first try black/blue/yellow/red for the entire key attribute column,
with black slot backgrounds. Its neighboring armor/inventory pixels must move
outside that column or accept those colors. A useful fallback layout gives
keys x176..191, inventory x192..255, face x112..143, and ARMS x80..111.
Reflow armor into x144..175 and health/ammo into the space left of ARMS. Check
`200%` fits before accepting this fallback. Use this variant to expose the
cost of strict attribute alignment, not to redefine the target for A–C.

For B, do not claim a successful result by removing sprites from the scene
that A displays. If legal 8x16 packing requires a small shift, record it and
include the overlay of original target bounds.

Palette availability statements assume no mid-frame palette upload. Do not
introduce raster palette swaps, cycling, or disabled sprite limits to make a
variant pass. Those require a separate timing experiment and are outside this
bounded visual comparison. World pixels may reuse HUD palettes; “remaining
world palettes” means independently chosen world palettes, not inaccessible
colors.

## 4. Implementation sequence for the later experiment

1. **Read the current seams.** Inspect `doom/port/video/fcvideo.c`,
   `doom/port/video/fcui.c`, `doom/tools/native_status_art.py`,
   `doom/tools/build_large_face_art.py`, `doom/tools/build_native_status_probe.py`,
   and `doom/sim/mesen2/run_native_status_probe.py`. Read the face-name metadata
   alongside `doomguy_faces.png`. Preserve source assets and existing captures.
2. **Create isolated experiment files.** Suggested generator:
   `doom/tools/prototypes/hud_reference_variants.py`; manifest and notes under
   `doom/assets/hud_layout_variants/`. Write generated runs to
   `/tmp/fcpico-hud-layout-variants/`. Do not modify the production renderer just
   to obtain a static screenshot.
3. **Measure and extract assets.** Produce annotated target bounds, indexed
   glyph/box/key artwork, and stretched face sheets. Reuse the user's drawn
   shapes rather than replacing them with a convenient but differently sized
   font. Separate fixed art from numeric sample values.
4. **Build A first.** Generate CHR, nametable, attribute bytes, palettes and OAM.
   Check the exact CHR count, OAM count, every scanline and all sprite bounds.
   Capture and visually inspect A before deriving B–D. Avoid four independent
   generators: branch on a small variant configuration.
5. **Build B, C and D.** Keep the target source and metrics shared; document only
   intended allocation/alignment changes. A variant that violates limits must
   be corrected, not presented as one of the four feasible outcomes.
6. **Capture in Mesen.** Start with a conventional static NES fixture if needed
   to isolate layout/PPU behavior. Use repository NESASM and Mesen; upload assets
   through normal ROM code, set the correct sprite mode, and render normally.
   Capture through the existing Lua screenshot APIs after startup settles.
   A host-composited preview alone is not an acceptable result.
7. **Check integration feasibility.** Port the most promising compatible hybrid
   fixture into the existing fc-pico Mesen probe without promoting it to default.
   If it needs a transport change, record that blocker and leave the static
   comparison intact. Clearly distinguish NROM visual validation from fc-pico
   cosimulation and eventual hardware validation.
8. **Package evidence and recommendation.** Generate the comparison images and
   report described below. Stop for visual selection before production migration
   or firmware packaging.

## 5. Integration constraints to carry into the report

The current v4 stream omits 28 tiles at y224..231 (columns 2..29), leaving a
resident native row; the calibrated read count is 15122. A background face or
new labels crossing that row cannot simply be painted into the streamed image.
List which pixels in each variant intersect it and whether they require native
resident tile updates or a later protocol revision. Static NROM success does
not establish that the current transport can display the variant unchanged.

Preserve the permanent fix bank. Do not widen mapper selection, change the
calibrated read schedule, or suppress validation to get a capture. Report CHR
residency separately from deduplicated art size; streaming and resident address
ownership matter. Dynamic face updates need a generation-matched snapshot and
atomic visible result. An all-background face does not require every animation
frame to be resident simultaneously, but streaming/upload cost must be measured
before claiming the complete animation solution fits.

Changing 8x16 mode also affects menus, logos and keys. Include a follow-up cost
estimate for those assets; do not describe B as a production-ready mode switch
based only on its static status screenshot. Current transparent-menu behavior
and the accepted paused HUD policy remain integration requirements.

## 6. Validation and required deliverables

For each A–D produce:

- Original-resolution 256x240 Mesen PNG, unfiltered 4x HUD crop, and labeled
  side-by-side comparison against the edited reference and original DOOM bar.
- The runnable ROM, generation command, manifest, CHR/nametable/attribute/OAM
  data and palettes; optionally a NAW PPU/OAM dump from that same run.
- Metrics JSON: sprite mode, used BG/sprite palettes, unique tiles/bytes by
  pattern region, total OAM, peak sprite count and affected scanlines, element
  bounds, changed pixels/coordinates, and remaining integration work.
- Mesen settings proving sprite-limit removal is off, screenshot frame number,
  fixture identity and whether the capture uses static NROM or fc-pico mapper.

Checks before presenting the set:

- No missing face corners, residual logo tiles, cut-off digits or border overlap.
- Red values include percent signs; labels are white; keys retain recognizable
  distinct colors wherever claimed; selected weapon and unowned weapons differ.
- Confirm 0/100/200 health and armor, three-digit ammo, 0/max inventory values,
  and all keys fit using supplementary fixtures. Don't change the main comparison
  values between variants. Record any sample inventory ambiguity from the PNG.
- Capture a short sequence for healthy idle, ouch and pickup if animation is
  wired into the experiment; otherwise explicitly mark animation as untested.
- Validate every generated scanline with normal sprite evaluation, including
  B's transparent padding. Verify attribute assignments by decoding PPU data.
- Pixel-compare the generated NES reference decode with the Mesen HUD using
  the same palette. Investigate mismatches; visual similarity alone can hide
  missing sprites.
- For the integration candidate, inspect a captured Doom world as well as the
  checkerboard. It must demonstrate the impact of any world palette reduction.
  Keep a separate result if integration cannot yet support a static variant.

Final report: a compact A–D table of visual fidelity, alignment changes, world
palette cost, sprite/CHR cost, and transport work, with a recommendation and
links to all four actual captures. Distinguish measured results from estimates.
The user chooses the preferred appearance before the selected layout becomes
production work. No UF2 is required for this comparison.

## Prototype outcome (2026-09-30)

The A–D NROM fixtures, Mesen captures, measured budgets, normalized face sheet,
and comparison are in `doom/assets/hud_layout_variants/README.md`. The fixture
generator is `doom/tools/prototypes/hud_reference_variants.py`. A preserves
the target layout most closely with one HUD BG palette and a measured peak of
eight sprites per line. C removes the face from OAM with a near-identical
image, at the cost of a second HUD BG palette. B uses 8x16 sprite pairing and
shifts its lower ARMS row. D needs a wider attribute-aligned key band. These
are real Mesen captures from static ROM fixtures; fc-pico stream integration,
dynamic values/expressions, moving-world palette impact and hardware checks
remain open for the selected design.

## ARMS/key refinement and playback fixtures (2026-10-01)

The first A fixture's ARMS accents were copied from color fragments in the
edited PNG. `doom/tools/prototypes/hud_playback_candidates.py` replaces these
with uniform 9x10 weapon wells and three candidate selected-number treatments:
`digit`, `plate`, and `border`. All three keys now use one 8x8 mask with blue,
yellow or red sprite palette indices. Each candidate ROM plays three distinct
health/ammo/armor/inventory, selected-weapon, key and face states, captured in
Mesen at frames 30/90/150. See `doom/assets/hud_playback_candidates/README.md`
and its nine original-resolution Mesen PNGs. Generated CHR reserves all digits,
percent, slash and selection glyphs 2–7; the three active face expressions fit,
while all 42 stretched expressions do not fit in one sprite pattern table.
This remains a visual playback fixture; fc-pico protocol integration is open.
