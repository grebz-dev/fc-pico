# CI workflow templates

These are GitHub Actions workflow templates for the Doom port. The host-only lane is active at
`.github/workflows/doom-host.yml`; its own first job verifies that the active file and this
template remain identical. The other templates remain inactive until their build targets exist.

| File | Runs | Level (plan/09) |
|------|------|-----------------|
| `doom-host.yml` | host C tests, sanitizers, Python tests, pioemu, generated-file and link checks | L0, L1, L4 (active) |
| `doom-build.yml` | device firmware, host build + unit tests + goldens, boot ROM | L0, L1, L2, L3 |
| `doom-sim.yml` | pioemu PIO tests, optional full-chip simulation | L4, L5 |
| `doom-cosim.yml` | Mesen2 co-simulation scenarios (nightly / manual) | L6 |
| `doom-docs.yml` | Doxygen with the repository's zero-warning rule | docs |

Pinned versions (bump deliberately, in one commit, with a note in `plan/08-build.md`):
pico-sdk 2.1.1, arm-none-eabi-gcc 13.2.Rel1, picotool 2.1.1, Python 3.11, .NET 8.

## Lane status

- Host: active in `doom-host.yml` (issue I-06).
- Device firmware: waiting for the device superbuild in issue I-08.
- Boot ROM: waiting for the source tree and Linux build in issue I-13.
- Full-chip simulation: waiting for the test-pattern firmware in issue I-10.
- Mesen2 co-simulation: waiting for the co-simulation skeleton in issue I-15.
- Documentation: remains a template until it can be activated without also activating the
  unfinished jobs that previously shared `doom-build.yml`.
