@page doom_port Doom port (plan)

A development plan for running Doom on the FC PICO -- the RP2040 Doom engine on the RP2350,
displayed and played through the console -- lives outside the Doxygen tree, in the `doom/`
directory of the `claude/doom-fc-pico-nes-2bb1bx` branch:

- `doom/README.md` -- entry point and reading order
- `doom/plan/00-overview.md` .. `12-agent-playbook.md` -- goals, constraints, architecture,
  protocol v2, video, input, audio, boot ROM, build, testing/CI, work plan, risks, and the
  operating rules for an AI agent executing the plan
- `doom/rp2040-doom/` -- the engine, as a git submodule of the `grebz-dev/rp2040-doom` fork

The plan was derived from @ref architecture, @ref nes_doom, @ref protocol, @ref hardware,
@ref graphics_page, @ref boot_reflash and @ref build_pipeline; where it extends the wire format
it does so as a superset (protocol v2) and never alters a v1 constant. Nothing under
`tutorial_project/` is modified by it.
