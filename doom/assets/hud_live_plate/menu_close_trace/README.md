# Menu-close regression trace

Mesen v4 cosimulation, NTSC, sprite limit enabled. The host picture and UI
snapshot are held constant while the Lua runner supplies a checked gameplay
snapshot to `UI_APPLY` after frame 165. Start is pressed at frame 155; the
initial paused-menu screenshot and OAM are recorded before the transition.
The final frame is 180.

| File | Evidence |
|---|---|
| `before.png`, `before-oam.bin` | Revision `20DOOM-04-9014`: closing the menu left logo X positions in the face and the logo tile in the selected ARMS slot. |
| `menu-open.png`, `menu-open-oam.bin` | Revised menu: full 64×32 logo, dedicated blue/blue/yellow palette. |
| `restored.png`, `restored-oam.bin` | Revised gameplay: 4×4 face grid restored at X 112/120/128/136; selected weapon tile `$33` at X 88 and Y 200. |
| `trace.csv` | Bus and DMA events through the revised Mesen run. |

Before the fix, face row X values after closing began `100,108,116,124`,
then continued `132,140,148,100`; ARMS slot 17 still held logo tile `$24`
at X 124. After restoring all four OAM fields from `native_oam_data`, each
face row has X `112,120,128,136`, and slot 17 holds the yellow plate tile
`$33` at X 88. The open logo has 823 yellow, 466 dark-blue and 262
bright-blue opaque pixels in its 64×32 area; no red logo pixels remain.

The final trace has 30 stable sampled heartbeats at count **15122**, no new
DMA stops during the steady interval, and all sampled NMI exits at scanline
256. It is an emulator trace, not a physical NES bus capture.

Recreate these captures with:

```sh
doom/.venv/bin/python doom/sim/mesen2/run_native_status_probe.py \
  --output /tmp/fcpico-menu-close-repro --menu --close-menu-frame 165
```
