---
name: fcpico-cosim
description: Run or test an FC PICO console ROM and cartridge application in the MesenCE emulator with the host cartridge model - build the patched emulator, run the S0/S1/demo scenarios headless, play interactively, add a provider or scenario, and interpret strict versus diagnostic results. Use this whenever someone wants to see FC PICO output without hardware, check a boot ROM or provider change in an emulator, reproduce a reflash, compare screenshots, or debug why a Mesen run fails, even if they just say "try it in the emulator".
---

# FC PICO co-simulation

The emulator runs the real console ROM; the cartridge side is the host model in
`sim/cartmodel` (same bus core as the firmware). It does not emulate the RP2350, PIO
timing or the bus electrically. Reference: `docs/pages/emulator-cartridge.md`.

## Prerequisites

SDL2 development files, the .NET SDK required by `sim/mesen/manifest.json`, `patch`,
CMake. Build with `python3 sim/mesen/mesen.py build`; `check-patches` confirms the
patch series still applies to the pinned source. The build checks SDL2 before
compiling and stops with the reason if the library is missing, static-only or a
broken link. Report that message to the user and stop there: don't search the
system or other projects for an SDL2, and pass `--make-arg SDL2LIB=...` only when
the user names the library to use. A .NET SDK installed by `dotnet-install.sh` lives in
`~/.dotnet` and is not on `PATH`: run `export DOTNET_ROOT=$HOME/.dotnet
PATH=$HOME/.dotnet:$PATH` before building or running, rather than reporting .NET as
missing.

## Steps

1. Choose what to run:
   - a regression check → `python3 sim/mesen/scenario.py run S0` (strict), `S1`
     (reflash, needs NESASM CE) or `demo` (scripted input);
   - an investigation → `run S0 --diagnostic` with `--cs1-mask`, which reports without
     failing;
   - a look by hand → `python3 sim/mesen/scenario.py play --provider demo`.
2. For new application behaviour, add a provider (`sim/cartmodel/providers/`, an
   `fcpico_provider_t`) and select it with `FCPICO_PROVIDER`; the emulator ROM copy
   comes from `tools/nes/set_mapper.py`.
3. Read `build/mesen/scenarios/<name>/report.json`: `passed`, `errors`,
   `determinism_errors`, the steady read counts, and the provenance hashes.
4. Look at `run1/final.png` when the picture matters. Before accepting a new golden,
   check it independently, for example by decoding the provider's picture with
   `fcpico.capture` and `fcpico.ppu_decode` (`docs/pages/python-tools.md`); a
   plausible-looking screenshot can still be wrong.

## Interpreting failures

- Counts other than 15490 after startup: see `fcpico-bus-debug`.
- `determinism_errors`: two identical runs differed; check for uninitialised state
  in the provider before suspecting the emulator.
- Data mode (palette/attribute uploads) makes the link stop and resynchronise; that
  is expected, not a failure, in scenarios that use it.
- Keep `RemoveSpriteLimit` off so screenshots match a console.

## Done when

You can state the scenario, strict or diagnostic mode, the verdict, the settings and
emulator/patch hashes from the report, and point to the screenshots and traces. Say
explicitly that hardware timing was not tested.
