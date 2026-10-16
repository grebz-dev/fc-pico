---
name: fcpico-hardware-validation
description: Prepare and record tests of FC PICO builds on a real Famicom or NES - hand over the exact UF2 and its hash, give flashing steps and the observations to make, and record what the person running the console reports. Use this whenever a build is about to go onto a cartridge, someone reports what they saw on a console, a change needs hardware confirmation, or results from the emulator are about to be described as working on hardware.
---

# FC PICO hardware validation

Only a person with the console can run these tests. Your part is to make the test
exact and to record its result honestly. Reference: `docs/pages/flashing.md` and
`docs/pages/boot-and-reflash.md`.

## Preparing a test

1. Identify the artifact: the UF2 path and full SHA-256 (from `build/manifest.json`),
   the boot ROM stamp it carries, and the source commit and whether it was dirty. If
   no UF2 exists yet, build it first (`fcpico-build`); a test request without an exact
   artifact cannot be recorded against anything.
2. Give the flashing steps: console off; BOOTSEL by the button or by
   a 1200-baud open of the serial port (`bin/arduino_picoboot.exe` on Windows,
   `stty -F /dev/ttyACM0 1200` on Linux); copy the UF2 or use `upload.bat` or
   `picotool load -x`.
3. Say what the console should do on first power-up. A new boot ROM stamp means the
   console reflashes itself (ROM UPDATE) before running; `PICO NOT FOUND` means the
   cartridge did not answer.
4. List the observations to report, each specific: what is on screen, how the
   controller behaves, serial statistics if a terminal is attached (read counts,
   stops, resyncs), and anything that looks wrong. Ask for a photo when the picture
   matters.

## Recording a result

- Record what was reported, in the reporter's terms, against the exact artifact hash.
- Separate what was observed from what you infer, and list the requested checks that
  were not done.
- Never describe a test as run, or a build as working on hardware, unless the person
  with the console said so. Emulator results are not hardware results.
- When emulator results are part of the evidence, record the scenario, its
  `report.json` verdict and the commit, labelled emulator-only. Rerun the scenario if
  the tools are available (`fcpico-cosim`) rather than relying on a summary.

## Done when

The test request names the artifact hash, the steps and the observations wanted, or
the result is recorded against that hash with any unrun checks named.
