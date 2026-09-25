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
  commit message or [`../PROGRESS.md`](../PROGRESS.md).
- **Lanes** say what an issue needs for verification. **A** uses host C and Python tools;
  **B** needs additional device or emulator tooling and a CI acceptance job; **C** needs a
  person. Recheck the current environment instead of assuming old sandbox limitations
  still apply; the latest findings are in [`../PROGRESS.md`](../PROGRESS.md).
- **Fix the plan as you go.** `plan/` is a specification and it has been wrong before; see
  the September 11 entry in [`../PROGRESS.md`](../PROGRESS.md). Correct it in the same commit as the code
  that revealed the error, and say so in the commit message.

## Adding a C test suite

`tests/CMakeLists.txt` discovers suites by glob: any immediate subdirectory of `tests/`
holding its own `CMakeLists.txt` is added automatically, in sorted order. So adding a suite
is adding a directory, and no issue needs to edit the shared file.

That was not the original design, and the reason it changed is worth keeping. Two issues
([I-04](I-04-fcapu-core.md) and [I-07](I-07-input-mapper.md)) both listed `tests/CMakeLists.txt` under **Owns** with the note "add one
`add_subdirectory`". Handed out in parallel, as the whole set is meant to be, they would have
clobbered each other on that one line, and the promise at the top of this file would have
been false on its first real test. A shared file that every new suite must touch is a design
error, not a coordination problem to be managed. The glob removes it. If a future issue finds
itself needing to edit a file that several other issues also need, that is the same error
again: fix the structure rather than the schedule.

## The set

| Issue | Title | Lane | Size | Depends on | State |
|-------|-------|------|------|------------|-------|
| [I-01](I-01-ppubus-tests.md) | Tests for the PPU-bus model | A -- host only, actionable now | S | none | **done** |
| [I-02](I-02-ppu-decode-tests.md) | Tests for the stream decoder | A -- host only, actionable now | S | none | **done** |
| [I-03](I-03-fcvideo-ref-tests.md) | Tests for the reference video pipeline | A -- host only, actionable now | M | [I-02](I-02-ppu-decode-tests.md) (soft) | **done** |
| [I-04](I-04-fcapu-core.md) | APU register sequencer | A -- host only | L | none | **done** |
| [I-05](I-05-audio-tools.md) | Music and effect conversion tools | A -- host only, actionable now | L | [I-04](I-04-fcapu-core.md) (soft) | open |
| [I-06](I-06-ci-host-lane.md) | Activate the CI host lane | A -- host only, actionable now | M | none | **done** |
| [I-07](I-07-input-mapper.md) | Controller mapping as a host-testable module | A -- host only, actionable now | M | none | **done** |
| [I-08](I-08-device-superbuild.md) | Device configuration of the superbuild | B -- device tooling + CI | M | [I-06](I-06-ci-host-lane.md) | **done**: builds clean on 13.2.Rel1; lane active |
| [I-09](I-09-fcbus-device.md) | Device backend for the bus | B -- device tooling + CI | L | [I-08](I-08-device-superbuild.md) | **done**: test patterns display on NES-001; trace-calibrated count and stream model |
| [I-10](I-10-testpattern-firmware.md) | Test-pattern firmware and serial CLI | B -- device tooling + CI | M | [I-09](I-09-fcbus-device.md) | **done**: replacement firmware displays patterns on an NTSC NES-001 |
| [I-11](I-11-engine-skeleton.md) | Engine fork: superbuild guard and platform skeleton | B -- partly local | M | [I-08](I-08-device-superbuild.md) (soft) | partial: GCC 13.2 device ELF links and fits; hardware boot check remains |
| [I-12](I-12-engine-host-build.md) | Engine host build without SDL | A/B -- try locally first | L | [I-11](I-11-engine-skeleton.md) | partial: SDL-free engine runs 600 deterministic indexed view frames; stream/pads await adapters |
| [I-13](I-13-bootrom-tree.md) | Doom boot ROM source tree and Linux build | B -- verified in CI only | M | [I-06](I-06-ci-host-lane.md) | open |
| [I-14](I-14-bootrom-nmi.md) | The v2 vertical-blank handler | B -- verified in CI only | L | [I-13](I-13-bootrom-tree.md) | open |
| [I-15](I-15-mesen2-cosim.md) | MesenCE co-simulation skeleton | B -- local host + CI | L | [I-06](I-06-ci-host-lane.md); [I-12](I-12-engine-host-build.md) for Doom scenarios only | **S0 done**: strict count, mailbox and screenshot golden pass; Doom scenarios await [I-12](I-12-engine-host-build.md) |
| [I-16](I-16-stage-a-composition.md) | 8-bit frame composition in the engine | B -- depends on the host build | L | [I-12](I-12-engine-host-build.md) | partial: 320x200 host goldens pass; device sink and independent picture check remain |
| [I-17](I-17-fcvideo-impl.md) | The converter, on the host and on the device | A/B -- host core now, device after [I-16](I-16-stage-a-composition.md) | L | [I-03](I-03-fcvideo-ref-tests.md); [I-16](I-16-stage-a-composition.md) for device integration | partial: host core passes differential tests; tables and device integration remain |
| [I-18](I-18-licensing.md) | Licensing enquiry and LICENSES.md | C -- human | S | none | open |
| [I-19](I-19-hw-trace.md) | Hardware trace capture and model calibration | C -- human, hardware | M | [I-10](I-10-testpattern-firmware.md) | **done**: complete trace 6 calibrates 66/64 reads and zero-based count |

## Suggested order

The current priority is the display path. Hardware trace calibration, strict S0 and the
host converter are complete. [I-11](I-11-engine-skeleton.md)/[I-12](I-12-engine-host-build.md)/[I-16](I-16-stage-a-composition.md) now establish the engine's 8-bit frame source;
then finish device publication and validate Doom pictures in Mesen S2. See the execution
order in [`../plan/10-workplan.md`](../plan/10-workplan.md).

[I-01](I-01-ppubus-tests.md) through [I-04](I-04-fcapu-core.md), [I-06](I-06-ci-host-lane.md) and [I-07](I-07-input-mapper.md) are done. [I-05](I-05-audio-tools.md)'s remaining audio conversion work follows
the display gate. The active host workflow from [I-06](I-06-ci-host-lane.md) provides the CI foundation for lane B.

Device build acceptance and test-pattern display on an NES-001 are complete. The pinned
13.2.Rel1 toolchain is in the repository root (the root `.gitignore` hides it from a tree
listing). [I-11](I-11-engine-skeleton.md) and [I-12](I-12-engine-host-build.md) unblock [I-16](I-16-stage-a-composition.md) and the device half of [I-17](I-17-fcvideo-impl.md); [I-13](I-13-bootrom-tree.md) and [I-14](I-14-bootrom-nmi.md) cover the
console; [I-15](I-15-mesen2-cosim.md) carries the later Doom co-simulation scenarios.

[I-15](I-15-mesen2-cosim.md)'s tutorial-ROM/test-pattern S0 now passes with the hardware-calibrated PPU selection,
valid mailbox, stable count and reviewed picture golden. Later Doom scenarios still need
[I-12](I-12-engine-host-build.md) and the corresponding engine/video features. [I-18](I-18-licensing.md)'s licensing enquiry remains open.
