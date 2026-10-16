---
name: fcpico-upstream-change
description: Prepare a focused change to the FC PICO repository for review - one feature per branch, the smallest complete diff, tests and documentation, honest provenance, no surprises for the tutorial. Use this whenever someone wants to contribute, open or prepare a pull request, split work into reviewable branches, port code from another project into FC PICO, or check a branch before review.
---

# FC PICO upstream change

FC PICO is a teaching project with users who build on Windows with the Arduino IDE.
A change is easy to accept when it is small, explained, tested, and leaves the
tutorial working as before. Reference: `docs/pages/conventions.md` and `AGENTS.md`.

## Steps

1. Fetch upstream and branch from its `main` (or from the branch this change depends
   on, if the reviewer has agreed to a stack). Name the branch for the feature.
2. Write down the one thing the change does. Move anything else to another branch.
3. Port deliberately: copy only the files the feature needs, adapt paths and names,
   and keep provenance (where the code came from, under which licence) in the commit
   message. Do not cherry-pick broad commits from other projects.
4. Keep the tutorial behaviour unless the change is about it: wire values stay those
   of `rp_system.h` and `SysPico.asm`, the fix bank stays byte-identical, generated
   files are regenerated, sources stay UTF-8, and `docs_jp/` is left to the author.
5. Dependencies are tracked files or pinned, checksum-verified downloads. Do not add
   submodules or forks to deliver a feature.
6. Add tests that fail without the change, and documentation in `docs/pages/`.
7. Run the existing checks (`python3 tools/build/fcpico_build.py host rom resources`,
   and the emulator scenarios if the change touches the bus or ROM).
8. Write the description: the problem, what changes, how it was tested, what was not
   tested (hardware in particular), and anything the reviewer must decide.

## Done when

`git diff upstream/main...HEAD` (or against the base branch) contains only this
feature, the checks pass, and the description names provenance, tests and remaining
gaps.
