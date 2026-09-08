# 05 -- Input

Eight buttons, two controllers, one game designed for a keyboard and mouse.

## What arrives

Per console frame the boot ROM sends the controller state (v1: one raw byte for pad 1; v2:
`FP_COM_KEY pad1 pad2`). The `fcbus` ISR latches `pad[2]` (bit layout: A `$80`, B `$40`,
Select `$20`, Start `$10`, Up `$08`, Down `$04`, Left `$02`, Right `$01`) and increments
`fcbus_frame_no`. The console's fix bank already de-glitches pad 1 with a four-sample majority
vote (needed while DPCM plays); the v2 boot ROM does the same for pad 2.

The engine reads it once per tic in `I_StartTic()` (35 Hz) via `fcinput_poll()`, which
computes pressed/released edges against the previous tic and posts `ev_keydown`/`ev_keyup`
events with Doom key codes (`doomkeys.h`), exactly as the USB path does through
`pico_key_down/up`. Because tics are slower than frames, a tap shorter than one tic
(~28 ms) can be missed; the latch therefore ORs presses seen between polls ("sticky press for
one poll"), which is what NES games do with `KEY_TRG`.

## Mapping

### In game

| Button | Tap | Hold | Doom key posted |
|--------|-----|------|-----------------|
| D-pad Up / Down | -- | move forward / back | `KEY_UPARROW` / `KEY_DOWNARROW` |
| D-pad Left / Right | -- | turn (or strafe while B is held) | `KEY_LEFTARROW`/`KEY_RIGHTARROW`, or `,` / `.` (`key_strafeleft`/`key_straferight`) |
| A | -- | fire | `KEY_RCTRL` (`key_fire`) |
| B | **use** (released within 8 frames with no D-pad activity) | strafe modifier | tap -> `' '` (`key_use`) pulse for one tic; hold -> nothing itself, changes the Left/Right mapping |
| Select | next weapon | automap (held >= 20 frames) | `key_nextweapon` pulse; `KEY_TAB` pulse on hold threshold |
| Start | menu | -- | `KEY_ESCAPE` |
| Select + Start (together) | -- | -- | reserved: pause (`KEY_PAUSE`) |

Always-run is a firmware option (default **on**): `fcinput` holds `KEY_RSHIFT` (`key_speed`)
while any movement key is down. Doom's own "always run" via `joyb_speed=29` is not used (the
joystick code is compiled out: `NO_USE_JOYSTICK=1`).

Turn speed: Doom accelerates turning after holding for 6 tics (`slowturn`), which is fine on a
D-pad. If turning feels too fast at 35 Hz, the mapper can alternate press/release on every
other tic ("half-rate turn") as an option.

### Second controller (optional, v2 only)

| Button | Action |
|--------|--------|
| Left / Right | strafe |
| A | use |
| B | run toggle (when always-run is off) |
| Select / Start | previous weapon / automap |

### Menus, intermission, finale, automap

| Button | Menu | Automap |
|--------|------|---------|
| D-pad | navigate / adjust sliders | pan |
| A | `KEY_ENTER` | -- |
| B | `KEY_BACKSPACE` (back) | zoom out (`-`) |
| Start | `KEY_ESCAPE` | exit automap (`KEY_TAB`) |
| Select | -- | zoom in (`=`) / follow toggle on hold (`F`) |

"Press a key" prompts (title, intermission, finale, message boxes with Y/N): A = `y`/enter,
B = `n`/escape. The message-box code checks for `key_menu_confirm` = `'y'`.

### Text entry

Doom prompts for a save-game name. On a controller this is replaced: the fcpico build defines
`FCPICO_AUTO_SAVENAME=1`, and `M_SaveSelect()` in `m_menu.c` (engine fork) accepts the slot
with a generated name (`"E1M3 12:34"`-style using the level and `leveltime`) without entering
`saveStringEnter` mode. Player name for network games is irrelevant (`NO_USE_NET`).

### Cheats

Doom cheats are typed letter sequences checked by `cht_CheckCheat()` against key events with
`data2` set to the character. `fcinput` includes a small sequence recogniser: while **Select
is held**, D-pad and A/B presses are appended to a buffer; the buffer is matched against a table
of button sequences, and on a match the corresponding letters are posted as key events (e.g.
`Up Up Down Down Left Right A` -> `iddqd`). Table in `fcinput_cheats.h`. This is a Phase 3
extra; it must be built so that it cannot interfere with normal Select behaviour (a matched
sequence swallows the Select release).

### Quit

"Quit Game" in the menu cannot exit to DOS. In the fcpico build `I_Quit()` prints a line on the
serial console and calls `watchdog_reboot(0, 0, 0)`. The console keeps sending heartbeats; the
firmware boots into `INIT`, detects the running protocol from the first packet and streams
again once Doom reaches its title screen (a few seconds of black). Alternatively hide the menu
item (`NO_USE_EXIT` is already set in the tiny build and reroutes `M_QuitDOOM`; verify what it
does in the fork and pick whichever gives the cleaner experience).

## Implementation notes (engine fork, `src/fcpico/i_input_fcpico.c`)

- Keep `TranslateKey`/`GetTypedChar` for the UART "SDL event forwarder" path so a developer can
  still type on a keyboard through the serial port during bring-up (`I_GetEventTimeout` is
  unchanged), and add the controller mapper on top.
- All mapping tables are `const` in flash; the state is a handful of bytes.
- Expose `fcinput_set_option(FCINPUT_ALWAYS_RUN, on)` for the options menu (Phase 3 adds an
  "FC PICO" options page or reuses the Doom `M_Options` sliders: mouse sensitivity slot ->
  "turn speed", "Always Run" toggle in place of "Messages").

## Tests

- `tests/input/test_mapper.c`: feed pad sequences per tic, assert the exact event stream
  (edge order, use-tap detection window, strafe modifier, Select tap-vs-hold, cheat sequence).
- Co-simulation: Mesen2 Lua scripts drive controller 1 (`emu.setInput`) to walk the E1M1
  start room, open the first door and change weapon; assert on game state read back through
  the serial log of the host model (`player->mo->x`, `readyweapon`).
