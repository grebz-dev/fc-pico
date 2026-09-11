<!-- SPDX-License-Identifier: BSD-3-Clause -->
# Issues

The remaining work, cut into pieces that can be handed out one at a time. Each file is
self-contained: what to build, which documents specify it, which files it owns, the exact
commands that decide whether it is done, and the traps that have already bitten someone.

## How to use these

- **One issue, one worker.** The **Owns** list exists so two workers never edit the same
  file. If an issue needs to touch something another issue owns, it is the wrong size --
  split it or wait.
- **Acceptance is a command, not an opinion.** Paste the command and its output into the
  commit message or `../PROGRESS.md`.
- **Lanes** say where an issue can be verified. **A** runs entirely on a host with gcc,
  cmake and Python, which is what the development sandbox has. **B** cannot be verified
  locally at all -- there is no ARM toolchain, no Wine and no .NET, and the proxy blocks the
  downloads -- so its deliverable includes the CI job that proves it. **C** needs a person.
- **Fix the plan as you go.** `plan/` is a specification and it has been wrong before; see
  `../REVIEW-2026-09-11.md` for ten examples. Correct it in the same commit as the code that
  revealed the error, and say so in the commit message.

## The set

| Issue | Title | Lane | Size | Depends on |
|-------|-------|------|------|------------|
| [I-01](I-01-ppubus-tests.md) | Tests for the PPU-bus model | A -- host only, actionable now | S | none |
| [I-02](I-02-ppu-decode-tests.md) | Tests for the stream decoder | A -- host only, actionable now | S | none |
| [I-03](I-03-fcvideo-ref-tests.md) | Tests for the reference video pipeline | A -- host only, actionable now | M | I-02 (soft) |
| [I-04](I-04-fcapu-core.md) | APU register sequencer | A -- host only, actionable now | L | none |
| [I-05](I-05-audio-tools.md) | Music and effect conversion tools | A -- host only, actionable now | L | I-04 (soft) |
| [I-06](I-06-ci-host-lane.md) | Activate the CI host lane | A -- host only, actionable now | M | none |
| [I-07](I-07-input-mapper.md) | Controller mapping as a host-testable module | A -- host only, actionable now | M | none |
| [I-08](I-08-device-superbuild.md) | Device configuration of the superbuild | B -- verified in CI only | M | I-06 |
| [I-09](I-09-fcbus-device.md) | Device backend for the bus | B -- verified in CI only | L | I-08 |
| [I-10](I-10-testpattern-firmware.md) | Test-pattern firmware and serial CLI | B -- verified in CI only | M | I-09 |
| [I-11](I-11-engine-skeleton.md) | Engine fork: superbuild guard and platform skeleton | B -- partly local | M | I-08 (soft) |
| [I-12](I-12-engine-host-build.md) | Engine host build without SDL | A/B -- try locally first | L | I-11 |
| [I-13](I-13-bootrom-tree.md) | Doom boot ROM source tree and Linux build | B -- verified in CI only | M | I-06 |
| [I-14](I-14-bootrom-nmi.md) | The v2 vertical-blank handler | B -- verified in CI only | L | I-13 |
| [I-15](I-15-mesen2-cosim.md) | Mesen2 co-simulation skeleton | B -- verified in CI only | L | I-06, I-12 |
| [I-16](I-16-stage-a-composition.md) | 8-bit frame composition in the engine | B -- depends on the host build | L | I-12 |
| [I-17](I-17-fcvideo-impl.md) | The converter, on the host and on the device | B -- depends on I-16 | L | I-03, I-16 |
| [I-18](I-18-licensing.md) | Licensing enquiry and LICENSES.md | C -- human | S | none |
| [I-19](I-19-hw-trace.md) | Hardware trace capture and model calibration | C -- human, hardware | M | I-10 |

## Suggested order

Lane A first and in parallel: I-01, I-02, I-03 are small and independent; I-04 and I-07 are
self-contained C modules; I-05 follows I-04's data format; **I-06 unblocks every lane B
issue** and should be done early by whoever is comfortable with Actions.

Then I-08 (device configuration), which unblocks I-09 and I-10; I-11 and I-12 (the engine),
which unblock I-16 and I-17; I-13 and I-14 (the console); I-15 (co-simulation).

I-18 can happen at any time and should happen soon. I-19 is the gate on everything the bus
model claims, and it needs I-10 plus a person with a Famicom.
