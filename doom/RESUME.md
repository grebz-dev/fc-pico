# Resume notes

Trimmed on 2026-09-11: everything this file used to carry has been absorbed. Read in this
order and you have the full picture.

1. `issues/README.md` -- the remaining work, cut into pieces one worker can take on alone.
   This is the entry point. Lane A is verifiable in a plain Linux sandbox today; lane B needs
   CI (no ARM toolchain, no Wine, no .NET here, and the proxy blocks the downloads); lane C
   needs a person.
2. `PROGRESS.md` -- what has landed, with the command that verifies each piece.
3. `plan/10-workplan.md` -- the status table and the full task list the issues were cut from.
4. `REVIEW-2026-09-11.md` -- ten arithmetic and consistency defects found in `plan/` and
   corrected. Worth reading before trusting any number in the plan.

## Facts worth keeping that live nowhere else

- **Branches.** `claude/doom-fc-pico-nes-2bb1bx` in both `grebz-dev/fc-pico` (this repo) and
  `grebz-dev/rp2040-doom` (submodule `doom/rp2040-doom`, pinned at `d8a20ca`, which only adds
  `FCPICO-PORT.md` and `src/fcpico/README.md`).
- **No hardware session has happened.** Every "(inferred)" and "(empirical)" row in
  `plan/01-constraints.md` is unconfirmed, and the frame-read-count contradiction described
  there is the single largest open question in the project. Issue I-19 is the gate.
- **Prior-art clones used for reading, not preserved:** `axsann/fc_pico_gb` and
  `rasteri/pipu`. Everything extracted from them is already in `plan/01`, `04`, `06`, `09`
  and `11`.
- **Open question Q5** (FamiStudio CLI option names) needs `FamiStudio -help` on a machine
  that can run a .NET binary. See `plan/11-risks.md` for Q1-Q12 and which are now answered.
- **Human-only items:** `HARDWARE-REQUESTS.md` HR-1 (trace capture, needs the test-pattern
  firmware from issue I-10 first) and HA-1 (licensing enquiry) -- issues I-19 and I-18.

## Whole-tree verification

```
cmake -S doom -B build -DFCPICO_HOST_ONLY=ON && cmake --build build && ctest --test-dir build
python3 -m pytest doom/tests doom/sim -q
python3 doom/tools/gen_protocol.py --check
python3 doom/tools/check_md_links.py doom
```
