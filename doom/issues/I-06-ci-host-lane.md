<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-06 Activate the CI host lane

| | |
|---|---|
| **Lane** | A -- host only, actionable now |
| **Size** | M |
| **Depends on** | none |
| **Work plan task** | P0-T12 |

## Goal

Nothing in this repository is checked automatically today, and the device, boot-ROM and
co-simulation lanes cannot even be run in the development sandbox (no ARM toolchain, no
Wine, no .NET -- the proxy blocks the downloads). CI is therefore the only place most of the
remaining work can ever be verified, and the host lane is the part that can go green now.

## Specification

`plan/09-testing-ci.md`, the GitHub Actions table. The existing templates in
`ci/workflows/` assume targets that do not exist yet, which is why none of them is active.

## Owns (create or modify only these)

`.github/workflows/doom-host.yml` (new), `ci/workflows/doom-host.yml` (new), `ci/README.md` (update)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Write one workflow that runs exactly what passes today: the host CMake build and ctest,
   the full pytest suite, the pioemu suite, `tools/gen_protocol.py --check`, and
   `tools/check_md_links.py doom`. Add an ASan/UBSan configuration of the C tests.
2. Keep it in both `ci/workflows/` and `.github/workflows/`, and add the step that diffs the
   two so they cannot drift (the existing templates already do this).
3. Pin action versions and the Python version; cache pip.
4. Split the other templates' jobs out of `doom-build.yml` so that activating the device lane
   later (I-08) is a copy, not a rewrite. Say in `ci/README.md` which lanes are active and
   which are waiting on which issue.

## Acceptance

Every command must pass from the repository root.

```
# locally, the same steps the workflow runs:
cmake -S doom -B build-host -G Ninja -DFCPICO_HOST_ONLY=ON && cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 -m pytest doom/tests doom/sim/pioemu -q
python3 doom/tools/gen_protocol.py --check
python3 doom/tools/check_md_links.py doom
# then: the workflow is green on the branch
```

## Traps

Do not add a job that cannot pass. A red lane nobody can fix teaches the team to ignore
CI, which is worse than no CI.
