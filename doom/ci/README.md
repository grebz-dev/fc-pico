# CI workflow templates

These are GitHub Actions workflow templates for the Doom port. The host and device lanes are
active at `.github/workflows/doom-host.yml` and `.github/workflows/doom-device.yml`; each
verifies that its active file and template remain identical. The remaining templates are
inactive for the reasons listed below.

| File | Runs | Level (plan/09) |
|------|------|-----------------|
| `doom-host.yml` | host C tests, sanitizers, Python tests, pioemu, generated-file and link checks | L0, L1, L4 (active) |
| `doom-device.yml` | RP2350 test-pattern firmware, layout check and build artifacts | L0 (active) |
| `doom-build.yml` | device firmware, host build + unit tests + goldens, boot ROM | L0, L1, L2, L3 |
| `doom-sim.yml` | pioemu PIO tests, optional full-chip simulation | L4, L5 |
| `doom-cosim.yml` | MesenCE co-simulation scenarios (nightly / manual) | L6 (template; strict S0 passes locally) |
| `doom-docs.yml` | Doxygen with the repository's zero-warning rule | docs |

Pinned versions (bump deliberately, in one commit, with a note in `plan/08-build.md`):
pico-sdk 2.1.1, arm-none-eabi-gcc 13.2.Rel1, picotool 2.1.1, Python 3.11, .NET 10 for MesenCE.

## Lane status

- Host: active in `doom-host.yml` (issue I-06).
- Device firmware: active in `doom-device.yml`, which is byte-identical to this template and
  checks that itself. The same commands were run locally against arm-none-eabi-gcc 13.2.Rel1
  and pico-sdk 2.1.1: clean build, `fcpico_testpattern.uf2`, 87.6% of the flash budget free
  (I-08 through I-10). What remains for I-09 and I-10 is hardware, not CI.
- Boot ROM: waiting for the source tree and Linux build in issue I-13.
- Full-chip simulation: waiting for a validated test-pattern build and simulation harness.
- MesenCE co-simulation: the skeleton builds and strict S0 passes locally against the
  hardware-calibrated mapper and reviewed picture golden. The template remains inactive
  until the workflow is enabled and exercised in CI.
- Documentation: remains a template until it can be activated without also activating the
  unfinished jobs that previously shared `doom-build.yml`.
