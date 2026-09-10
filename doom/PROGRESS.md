# Progress log

One entry per task from `plan/10-workplan.md`, newest first. Format:

```
## <task id> -- <title>
- Commits: <range or list>
- Verified: <command> -> <result summary>
- Measurements: <fps / cycles / bytes / ms, with the command>
- Left out: <what and why>
- Plan changes: <documents touched>
```

## P2-T2 (harness part) -- NMI cycle-count harness, measured on the tutorial ROM
- Commits: see `git log -- doom/tests/bootrom`
- Verified: `python3 -m pytest doom/tests/bootrom -q` -> 17 passed
- Measurements (`doom/tests/bootrom/tutorial_nmi_cycles.json`, py65 1.2.0, NTSC vblank 2273):
  tutorial NMI critical section 577 cycles; totals 1149 / 1341 / 1533 / 1724 for 0 / 8 / 16 / 24
  APU pairs with sprite DMA (+513 modelled); 65 `$2007` reads; 23.7 cycles per APU pair.
- Left out: the Doom ROM does not exist yet; the harness is parameterised for its 128-byte
  mailbox (`$20`-`$5F` + `$C0`-`$FF`) but RAM presets for it are TBD (see the README).
- Plan changes: 01 (measured NMI row, APU cost 24 cycles/pair), 07 (validation note).

## Planning

Planning completed; implementation tasks proceed per `plan/10-workplan.md`.
