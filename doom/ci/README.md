# CI workflow templates

These are GitHub Actions workflows for the Doom port. They live here, **inactive**, until task
P0-T12 copies them to `.github/workflows/` so that the repository does not show failing checks
before the code they build exists. Keep the two locations identical afterwards (a `diff` step in
`doom-build.yml` enforces it).

| File | Runs | Level (plan/09) |
|------|------|-----------------|
| `doom-build.yml` | device firmware, host build + unit tests + goldens, boot ROM | L0, L1, L2, L3 |
| `doom-sim.yml` | pioemu PIO tests, optional full-chip simulation | L4, L5 |
| `doom-cosim.yml` | Mesen2 co-simulation scenarios (nightly / manual) | L6 |
| `doom-docs.yml` | Doxygen with the repository's zero-warning rule | docs |

Pinned versions (bump deliberately, in one commit, with a note in `plan/08-build.md`):
pico-sdk 2.1.1, arm-none-eabi-gcc 13.2.Rel1, picotool 2.1.1, Python 3.11, .NET 8.
