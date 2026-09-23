# Hardware log

Appended by whoever runs a console session. Newest first.

```
## <date> -- <milestone / request id>
- Console: <Famicom / AV Famicom / NES-001 + adapter ...>, region
- Cartridge firmware: <uf2 name, git commit>
- Console ROM: <stamp string shown by `stats` or from the ROM UPDATE screen>
- Results: ...
- Attachments: <paths>
```

## 2026-09-22 -- I-10 replacement firmware validation
- Console: NES-001, NTSC
- Cartridge firmware: `fcpico_testpattern.uf2` from parent commit `73a6df2`, SHA-256
  `be283708ba21e4ffa75ad4dec665461cd52db9220888d86888096f8051389cbf`
- Console ROM: tutorial boot ROM
- Results: success; the test patterns appear after the replacement firmware added the
  post-`FP_COM_INI` data-mode handoff. This confirms the previous blank screen was the
  physical adapter's lost init event, not bad pattern, palette, or attribute data.
- Attachments: `tests/fixtures/hw_trace_ntsc/`; replacement short traces pending

## 2026-09-22 -- HR-1 initial count and display run
- Console: NES-001, NTSC
- Cartridge firmware: `fcpico_testpattern.uf2`, pre-init-handoff fix
- Console ROM: tutorial boot ROM; displayed `MEMORY 2048B OK`
- Results: 5495/5507 measured frames reported exactly 15490 reads; 12 low outliers caused
  DMA stops.  LED and 56.44 Hz heartbeats continued, but no pattern appeared because the
  physical test-pattern path did not react to `FP_COM_INI` by requesting data mode.  The
  palette and attribute buffers were correct.  Three raw traces were incomplete.
- Attachments: `tests/fixtures/hw_trace_ntsc/`
