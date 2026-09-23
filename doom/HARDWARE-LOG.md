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

## 2026-09-22 -- HR-1 initial count and display run
- Console: NES-001, NTSC
- Cartridge firmware: `fcpico_testpattern.uf2`, pre-init-handoff fix
- Console ROM: tutorial boot ROM; displayed `MEMORY 2048B OK`
- Results: 5495/5507 measured frames reported exactly 15490 reads; 12 low outliers caused
  DMA stops.  LED and 56.44 Hz heartbeats continued, but no pattern appeared because the
  physical test-pattern path did not react to `FP_COM_INI` by requesting data mode.  The
  palette and attribute buffers were correct.  Three raw traces were incomplete.
- Attachments: `tests/fixtures/hw_trace_ntsc/`
