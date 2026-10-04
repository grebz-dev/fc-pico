# 13 — Upstream FC PICO contribution plan

Date: 2026-10-01. Status: proposed; this document does not start extraction or
create feature branches. Intended implementation size: one bounded feature and
its acceptance checks per pull request.

## Goal and source baseline

Contribute the development platform proven on the `doom` branch to
`impact-soft/fc-pico`, so a project can develop and inspect its console ROM and
cartridge application alongside MesenCE without depending on Doom. Preserve the
existing tutorial, firmware build and physical cartridge behavior.

This plan was checked against these local snapshots:

| Repository | Source revision | Contribution destination |
| --- | --- | --- |
| FC PICO fork | `ce221d44639c89c50951f3d509015dbef63368ff`, branch `doom` | `impact-soft/fc-pico`; separate feature branches in `grebz-dev/fc-pico` |
| MesenCE fork | `9688726c982c57cfbcb81bc6d519decfe82eff91` | Extract mapper source and integration patches into an FC PICO feature branch |
| RP2040 Doom fork | `84ecd1de89ebbb1b63f51f18b0e28e4c829d8d1d` | Extract reusable source into an FC PICO feature branch; omit engine/UI changes |

These are source snapshots, not a claim about current upstream HEADs. Before
starting a contribution, fetch each relevant upstream and record its actual base
SHA. Check its existing build conventions and licensing before choosing final
destination paths. Every contribution branch belongs to this repository and
contains its implementation as ordinary tracked files. Submodule revisions above
identify extraction provenance only. Feature branches must not add, update or
depend on gitlinks to supply contribution content. Existing Doom submodules may
remain on the source branch, but the contributed features must work without them.

Mesen integration is delivered here as tracked mapper source, patch files,
configuration, tests and a manifest. Building may fetch a checksum-verified
pristine upstream emulator archive into an ignored cache and apply those local
patches. An upstream dependency revision in that manifest is allowed; a submodule
commit pointer carrying our changes is not. A later submission to MesenCE itself
is optional follow-up and is not required to deliver any feature in this plan.

Evidence entry points: [Doom README](../README.md),
[build notes](08-build.md), [constraints](01-constraints.md),
[simulation/testing design](09-testing-ci.md),
[Mesen scenario guide](../sim/mesen2/DOOM-FRAME.md),
[progress](../PROGRESS.md), and [physical log](../HARDWARE-LOG.md).
The latest physical report confirms the 9016 application build works as intended;
it does not independently validate every proposed extracted platform component.
README summaries and some memories still describe older milestones: resolve
claims against dated traces and current code before copying them upstream.

## Architectural contract: one implementation of platform behavior

The emulator executes NES CPU/PPU code and calls the native host C cartridge
backend. It does not emulate ARM instructions or electrically accurate PIO/DMA
timing. Python orchestrates runs and inspects artifacts; it must not become a
second implementation of the production cartridge state machine.

Extraction must also prevent *C* implementations from drifting. Today
`doom/fcbus/` is a port of tutorial `sys/rp_system` behavior, rather than a shared
implementation used by the original tutorial. Introduce a production/host seam
and use the same reusable protocol/state-machine sources in both paths. Keep
device PIO/DMA/GPIO responsibilities behind a device backend and the existing
tutorial application interface behind a compatibility wrapper. Prove tutorial
v1 compatibility before replacing production behavior. If that migration is too
large for one review, split it into a host-neutral library PR and a separately
named `platform/tutorial-bus-adapter` PR; do not claim that duplication is solved
until the tutorial consumes the library.

Protocol identifiers and stream geometry must have one authored source. Export
them to assembler/Python, and export stream layout from the same native
implementation or a descriptor generated from it. Future upstream firmware
changes may require schema/API evolution and new tests, but must not require
rewriting equivalent behavior in Python. Consumers should reject unknown
protocol/layout versions explicitly.

Proposed upstream organization, adjusted to maintainer preferences:

```text
platform/fcbus/          shared core, host/device backends, protocol definitions
platform/host/          optional Pico SDK application shims
sim/cartmodel/          native cartridge adapter and application-provider ABI
sim/mesen/              tracked mapper/patches/manifest, settings, runners, Lua probes
tools/fcpico/           Python inspection package and native bindings
tools/build/            tool bootstrap, ROM assembly, reproducible build entry points
examples/cosim/         redistributable patterns and tiny application example
tests/                  platform contracts and scenario fixtures
docs/development/       setup, emulator, hardware, native graphics and evidence guides
AGENTS.md               concise development constraints and task routing
.agents/skills/         platform workflows with explicit triggers and completion gates
```

## Feature branches and PR sequence

Each row is its own feature branch and atomic PR. Names are proposed branch names
to create later, not existing branches. Dependencies describe required accepted
interfaces; independent branches start from upstream rather than from `doom`.
For dependent work, use a temporary stack, then rebase onto upstream after its
prerequisites merge so the PR diff contains only that feature.

| ID | Repository and feature branch | Feature | Depends on |
| --- | --- | --- | --- |
| U01 | FC PICO: `platform/shared-bus-core` | Shared firmware/host bus implementation and tutorial compatibility | None |
| U02 | FC PICO: `platform/generated-protocol` | Protocol and layout exports for C/assembler/Python | U01 |
| U03 | FC PICO: `tools/native-rom-build` | Native ROM assembly and verification chain | None |
| U04 | FC PICO: `sim/native-cartmodel` | Application-independent native cartridge adapter | U01, U02 |
| U05 | FC PICO: `mesen/fcpico-mapper` | Tracked optional mapper, PRG flash model and makefile patches | U04 ABI |
| U06 | FC PICO: `sim/mesen-workflow` | Mesen configuration, build/run integration, captures and CI | U03, U04, U05 tracked integration |
| U07 | FC PICO: `tools/python-inspection` | Thin native bindings and reusable stream/trace/PPU inspection | U02, U04 |
| U08 | FC PICO: `tools/reproducible-platform-build` | Unified host/emulator/device buildchain and manifests | U03, U06, U07 |
| U09 | FC PICO: `firmware/serial-bootsel` | Nonblocking USB serial reboot into BOOTSEL | None |
| U10 | FC PICO: `sim/pico-host-shim` | Optional multicore/timer shim for applications using Pico APIs | U01; separate from basic cosimulation |
| U11 | FC PICO: `docs/platform-findings` | Verified, application-independent development findings | Can start now; finalize commands after U06–U09 |
| U12 | FC PICO: `agents/development-guide` | Platform `AGENTS.md` | U11; accepted tool interfaces |
| U13 | FC PICO: `agents/platform-skills` | Codex skill-creator workflow; Agent Skills standard and validation | U12; accepted tool interfaces |

Suggested delivery: U09 and U03 first for immediate usability, then U01/U02/U04,
then U05/U06/U07/U08. Draft U11 alongside extraction. U10 is optional for the
minimal platform demo and must not block it. Finish U12/U13 against real upstream
commands instead of promising commands that only exist on `doom`.

### U01 — shared bus core

Source: `doom/fcbus/fcbus_core.{c,h}`, `fcbus_host.{c,h}`,
`fcbus_device.{c,h}`, `fcppu.pio`, associated CMake and C tests. Compare with
`tutorial_project/tuto1_hw/sys/rp_system.{cpp,h}` and tutorial PIO sources.

1. Inventory the mapping between original tutorial functions and ported core
   functions. Distinguish compatibility corrections from new v2–v4 behavior.
2. Extract v1-capable bounded core and host backend with explicit buffer sizes,
   initialization, frame ownership, watchdog and stopped/open-bus semantics.
3. Add a tutorial adapter without changing its application API or its normal
   build route. Keep hardware/backend configuration and board pins explicit.
4. Preserve RAM placement for flash-sensitive paths, ISR behavior and existing
   allocation constraints; do not move logging into bus ISRs.
5. Keep new protocol extensions out of the required tutorial path. Versioned
   application providers may opt into them later.

Done when host and tutorial device builds use the shared behavior; v1 mailbox,
stream, init/reflash and frame ownership tests pass; original ROM and permanent
fix-bank checks pass; an original tutorial physical run is recorded. A host-only
pass is an extraction milestone, not a physical compatibility claim. If split,
the separately named adapter PR carries these final production gates.

### U02 — protocol generation and native layout export

Source: `doom/fcbus/fcbus_protocol.h`, `doom/tools/gen_protocol.py`,
`doom/bootrom/gen/`, `doom/tools/fcpico/protocol.py`, protocol tests.

1. Choose a canonical upstream definition file with preserved v1 wire values.
   The current restricted header parser is usable if its accepted syntax is
   documented and unsupported expressions fail rather than silently disappear.
2. Make input and output paths configurable; remove `DOOM_ROOT` assumptions.
   Emit assembler constants, Python constants, and a machine-readable layout
   descriptor/native query API without manually copied geometry.
3. Add generation and `--check` targets to the relevant builds. Keep generated
   artifacts clearly labeled, deterministic and checked for freshness in CI.
4. Leave Doom UI field meanings and default v3/v4 adoption in the Doom adapter.
   If extended protocols are desired upstream, review them in an additional
   `platform/versioned-mailbox` branch with separate compatibility/timing gates.

Done when modifying a supported definition regenerates all consumers, stale
outputs fail CI, v1 bytes match the tutorial, and Python inspection selects its
layout using native/generated metadata rather than new hand-written offsets.

### U03 — native ROM assembly toolchain

Source: `doom/bootrom/build.sh`, `doom/bootrom/ci/tutorial_md5_gate.sh`,
`doom/tools/nes/{normalize_ines,bincut,bin2c,check_fixbank,set_mapper}.py`,
bootstrap portions of `doom/tools/build_shadow_detail.sh`.

Provide an opt-in Linux native NESASM CE path alongside the original vendor
build. Pin assembler source revision `6fc41cda37b934aa29aa2639d0baa74424268e31`
as the known baseline, document normalization of the two header bytes, and
preserve listings and source encodings. Use configurable input/output paths and
do not overwrite the committed vendor ROM. Keep permanent-bank comparison and
the tutorial MD5 gate. An emulator-only mapper-header copy must leave PRG bytes
unchanged and must never become the device ROM artifact.

Done when a clean build reproduces the normalized tutorial ROM, validates the
unchanged permanent bank, and emits symbols/listings usable by timing probes.
No ARM SDK or Doom engine checkout is required for this feature.

### U04 — reusable cartridge adapter and application provider

Source: `doom/sim/cartmodel/fcpico_cart.{c,h}`, its CMake/tests,
`doom/testpattern/`, and frame/ROM publication interfaces in the host backend.

The current adapter is process-global, links the test-pattern app, and detects
Doom protocol/stream variants. Generalize it with a small versioned C ABI:
create/destroy/reset, qualified PPU read/write, monotonic tick, stats, application
init/frame publication and configured served ROM. Prefer instance ownership so
emulator load/reset sequences do not inherit a previous cartridge. Move the
pattern app and fixed-frame playback into provider implementations. Keep a
bounded deterministic dynamic provider for a simple non-Doom demo.

Done when patterns and the tiny dynamic demo run through the same adapter;
invalid inputs, stopped-bus reads, reset/watchdog and frame handoff pass native
tests; host builds do not initialize the engine submodule or load WAD/WHX assets.
Record the ABI revision needed by U05. Save-state support must be implemented
against all adapter/provider state or explicitly disabled for this first ABI.

### U05 — repository-owned MesenCE mapper and makefile patches

Source changes are only three files relative to local MesenCE baseline
`20f497c9`: `Core/NES/Mappers/Homebrew/FcPico.h`,
`Core/NES/MapperFactory.cpp`, and `makefile`. Source commits:
`5c2de02e`, `7d87b7cb`, `af34ead8`, `9688726c`.

Keep the mapper and the minimal optional build linkage in one FC PICO PR:
neither is useful alone. Store the mapper as an ordinary source file under
`sim/mesen/integration/`, with ordered patches for `MapperFactory.cpp` and
`makefile` under `sim/mesen/patches/`. A tracked manifest names the pristine
upstream base archive, checksum, patch order and backend ABI. Copy the mapper
into the temporary emulator source tree during the build; avoid keeping two
hand-edited copies in the repository. Replace makefile references to
`/doom/sim/cartmodel`, `/doom/fcbus`
and fixed static-library lists with configurable include/library or package
inputs from U04. Default emulator builds must remain independent of FC PICO.
Fix dependency tracking for mapper headers and backend libraries; current
`build.sh` forces `MapperFactory.cpp` to rebuild with `make -W` because normal
header tracking is insufficient. Avoid stale objects when optional flags change.

Retain physical address selection, CHR RAM for unselected reads, watchdog ticks,
and the JEDEC PRG flash erase/program/status behavior required by reflash.
Debugger peeks must not consume bytes. The mapper currently uses emulator-only
NES 2.0 ID 4093; confirm an acceptable opt-in registration with MesenCE
maintainers and do not present this private value as an assigned mapper number.
An invalid backend configuration must report an actionable startup error.

The mapper defaults to mask `0xf800` selecting `$0800–$0fff`, whereas some
runner/CI descriptions call `0xf000` the calibrated mask. Resolve that mismatch
against the measured address trace; retain wider masks as explicit diagnostics,
not alternate silent production defaults. Do not compensate by skipping fetches.

Done when optional and stock MesenCE builds both pass; real selected reads reach
the backend once; debugger peeks do not; masks/flash/reload/reset are checked;
and a mapper-header/backend change reliably rebuilds and relinks. Check that the
local patch bundle applies to the verified pristine source without fuzz or
offsets, and that all FC PICO modifications come from tracked repository files.
No emulator submodule pointer or separate feature branch is required.

### U06 — Mesen configuration, scenarios, development loop and CI

Source: `doom/sim/mesen2/{build.sh,run_scenario.py,run_scenario.sh}`, Lua probes,
selected generic portions of `run_doom_frame.py`/UI runners, and
`doom/ci/workflows/doom-cosim.yml`.

1. Build U04 independently of the Doom superbuild, then obtain the verified
   pristine emulator archive and apply U05's repository-owned integration.
   Support an offline archive/cache override; do not initialize Doom submodules.
2. Supply isolated settings and paths for every run. Baseline: NTSC, game database
   off, deterministic RAM/CPU–PPU alignment, random mapper power state off,
   `RemoveSpriteLimit=false`, rewind/autosave/update checks off. Allow Lua I/O
   only in the controlled runner profile. Do not overwrite a desktop install.
3. Accept project ROM, provider, input script and output directory. Expose both
   deterministic headless testing and an interactive launch with real key/pad
   controls. Interactive mode must retain hardware sprite limits too.
4. Separate strict S0 from diagnostic runs. Strict mode checks post-startup
   heartbeats, mailbox validity, no new stops and reviewed output. Run debugger
   peek/no-peek variants and compare outputs/traces.
5. Add S1 reflash as a separate scenario using two served/console ROM versions.
   A tiny moving-pattern/input-response provider supplies a continuous dynamic
   example; do not describe a repeated still frame as live application execution.
6. Save full frames, useful crops, Lua results, PPU fetch/mailbox traces, counts,
   logs and a manifest containing scenario/provider/ROM/emulator/source hashes.
   Add CI with timeout, failure artifacts, generator checks and content-based
   cache keys covering archive checksum, tracked mapper/patch hashes,
   configuration and backend dependencies.

Done when a fresh checkout builds and runs the non-Doom demo interactively and
headlessly, strict/diagnostic results are distinguishable, S1 verifies permanent
bank preservation, and two deterministic runs agree. The example requires no
Doom assets and does not imply cycle-accurate ARM or electrical simulation.

### U07 — Python development/inspection tools without duplicated firmware

Source disposition:

| Existing source | Contribution decision |
| --- | --- |
| `tools/fcpico/protocol.py` | Generated export only; U02 owns constants |
| `tools/fcpico/stream.py` | Keep generic packing/attribute inspection; delegate firmware layout/encoding behavior to native APIs or generated descriptors |
| `tools/fcpico/nes_palette.py` | Reusable color visualization with documented palette provenance |
| `tools/ppu_decode.py` | Keep standalone stream/sprite inspection; draw from generated/native metadata and make geometry explicit |
| `tools/trace_decode.py` | Keep hardware trace parsing and count/address summaries |
| `sim/ppubus/ppubus.py` | Keep only an independent diagnostic oracle if needed; do not use `SimpleCart` as the production cosimulation backend |
| `tools/fcvideo_ref.py` | Exclude Doom palette-selection, quantization and conversion reimplementation from the required platform chain |
| Native HUD prototypes/generators | Exclude Doom composition; generic PPU/OAM export may be adapted separately |

Package Python tooling with an isolated environment and portable imports. Use a
small C ABI with `ctypes` or a subprocess artifact API rather than requiring a
hand-maintained Python port of firmware functions. Make native errors and version
incompatibility visible. The independent decoder/oracle can retain NES format
logic to detect errors, provided the operational pipeline never depends on it
to reproduce evolving firmware algorithms.

Done when a native protocol/layout change updates Python consumers through
generated/bound data alone; packing is compared with native fixtures; decoding
matches Mesen captures; trace parsing rejects incomplete/corrupt records; and
the package installs/runs without Doom paths or data. Audit imports transitively
so a supposedly portable tool does not pull in Doom constants indirectly.

### U08 — reproducible platform buildchain

Source: reusable portions of `doom/tools/build_shadow_detail.sh`,
`reproduce_native_hud.sh`, `setup_env.sh`, build CMake/toolchain files and
flash-layout checks. Reuse the approach, not the hard-coded HUD/WHX pipeline.

Create separate opt-in setup, host, emulator and device entry points (or clearly
named presets) plus a single documented demo reproduction command. Pin known
source/tool versions and record compiler versions; verify downloads/checksums.
Baseline pins observed here include Pico SDK/picotool 2.1.1, Arm GNU 13.2.Rel1,
NESASM CE above, and .NET 10 for the current MesenCE build. Confirm version
requirements against the selected upstream revisions when implementing.

Use a project-local venv/cache and configurable build/output directories. Do not
copy the current setup script's system `pip --break-system-packages`, unconditional
APT/Wine steps or assumption that an existing checkout has the requested version.
Native assembly removes Wine from the default Linux path. Keep firmware's
original Arduino workflow supported; a new SDK example is opt-in.

Device outputs must use SDK/picotool firmware UF2 generation, preserve the
permanent console bank, validate flash/RAM boundaries and distinguish device ROMs
from emulator-header copies. Generic resource merging accepts explicit format
and placement; no default Doom WHX address or asset requirement. Record root and
dependency archive checksums, patch/source hashes, dirty-source status, tool
versions, configuration and artifact
hashes. `--verify-twice` uses independent trees and reports byte differences;
eliminate path/time leakage or document remaining reproducibility boundaries.

Done when a clean machine can reproduce the generic host/demo/ROM/UF2 chain;
missing dependencies have actionable errors; outputs have a verified manifest;
two clean builds match under the documented environment; and an upstream tutorial
device smoke run passes. Do not promote the Doom diagnostic startup countdown
to a universal platform requirement: its necessity remains an unresolved
application/build-profile observation.

### U09 — serial BOOTSEL command

Sources: `doom/port/cli.c` and
`doom/rp2040-doom/src/fcpico/device_video_fcpico.c`, with polling call sites in
the engine's `i_main_fcpico.c`. The actual operation is SDK
`reset_usb_boot(0, 0)` after a complete `bootsel` line.

Extract one bounded, nonblocking parser/platform helper; integrate it into the
upstream tutorial's normal loop and initialization wait paths. Accept CR/LF and
backspace, discard oversized lines, and reject partial or prefixed commands.
Give serial input one owner so a diagnostics CLI and a second poller cannot
consume each other's bytes. Do not introduce USB-host support or engine
dependencies. Support the original framework's compatible boot-ROM API rather
than requiring a full SDK migration for this small feature.

Document sending the command over the configured CDC port, waiting for BOOTSEL
enumeration, and using existing UF2/picotool loading. Preserve the established
instruction to keep the NES off during flashing. BOOTSEL only enters the MCU ROM
loader; it does not by itself load a new image or update the console PRG.

Done when parser tests cover line endings/overflow/partial commands, a physical
MCU enters BOOTSEL during startup and normal application execution, firmware can
be loaded without the button, and the tutorial boots afterward. If shared helper
adoption in another application is useful, include an ordinary tracked adapter
or example on a separately named FC PICO branch `firmware/serial-bootsel-adapter`.
The helper itself and the tutorial integration must not depend on an engine
submodule update.

### U10 — optional Pico SDK host application shim

Source: `doom/sim/host_shim/` and its detailed README/tests. Extract only APIs
actually required by non-Doom Pico applications: thread-local core identity,
two bounded inter-core FIFOs, alarm/repeating-timer support and runtime assertion
handling. It is not needed for U04's pure C host bus/provider path.

Do not ship the current shim as hardware-equivalent: host SDK dummy spinlocks
and sticky WFE event behavior make genuine pthread use of SDK synchronization
unsafe or inefficient; SDK cancellation can enter unsupported timer hooks.
Either provide a tested concurrency-safe adapter for the advertised API subset
or restrict/document it and ensure example applications use safe primitives.
Name supported SDK versions, timer cancellation requirements, callback ownership
and teardown behavior. Preserve deadlock-relevant FIFO capacity and direction.

Done when two-way FIFO saturation/ordering, core identity, alarm/cancel/reentrant
callbacks and shutdown pass under concurrency checks; unsupported APIs fail
clearly; an example demonstrates the supported subset without misleading claims.
No unrelated Chocolate Doom portability changes belong in this PR.

### U11 — platform findings and portable memory knowledge

Promote the following findings into human documentation with sources and limits:

| Finding | Evidence to consult | Upstream treatment |
| --- | --- | --- |
| CS1 address selection and buffered `$2007` reads determine byte consumption | `01-constraints.md`, trace 6 fixture, mapper | Explain selection; retain raw/calculated trace summaries |
| 66 selected pre-render reads, 64 per visible line on measured tutorial configuration | Hardware fixture/log and strict S0 | Label console/configuration; do not generalize to every ROM/region |
| Tutorial count is 15490: 15426 picture reads + dummy/mailbox reads, zero-based report | `fcbus_protocol.h`, `bus_protocol` memory | Show accounting; generate constants and retain count tolerance |
| Converter batch width 34 words is not consumed scanline stride 32 | Stream code and `bus_protocol` memory | Prevent buffer/offset mistakes with a layout diagram and fixture |
| Boot stamp must start with `20`; fix bank is permanent and version-coupled | ROM build, `bootrom_build` memory, S1 | Document original entry points/stamp/reflash verification |
| Debugger VRAM inspection must not advance cartridge data | Mapper plus diagnostic S0 | Make a contract/test, not an emulator workaround |
| Four BG and four sprite palettes, attribute and OAM/scanline constraints | Native UI plan, PPU/OAM captures | Generic native graphics guide; 8×16 still has eight-sprite scanline limit |
| Switching menu/game OAM requires complete state restoration | Menu-close trace and HUD fixes | Generic state-ownership lesson; no Doom asset inclusion |
| MCU host model, pioemu, Mesen and hardware prove different properties | Testing plan, `host_simulation` memory | Publish confidence boundaries and known simulator gaps |
| Native toolchain normalization, fix-bank gates and picotool UF2 route | Build notes and device memory | Include exact reproduction commands and evidence, not inferred fixes |

Review the durable portions of `.serena/memories/doom/{bus_protocol,
bootrom_build,host_simulation,video_tools,device_uf2_build,native_ui}.md`, plus
`conventions`, `tech_stack`, `suggested_commands`, `task_completion` and
`memory_maintenance`. Promote stable mechanisms to versioned platform docs.
Keep local user paths, stale status, particular UF2 hashes, Doom health/face
schemas, WHX layout and project-only branch rules out of upstream agent policy.
Separate measured findings, inferred explanations and unresolved failures.

The public docs are authoritative; a Serena project may have a small optional
`platform/core` memory routing to them and domain memories for bus/build/sim/UI.
Do not require Serena to develop the platform or maintain parallel copies of
long documents. Machine-local Serena project settings are not a contribution.

Done when each promoted assertion has a source/configuration/limit, generic
commands work without Doom, stale README claims have been reconciled, and no
unexplained fixture or application asset is copied into the platform docs.

### U12 — platform AGENTS.md

Create a concise root `AGENTS.md` with an accurate repository map, supported
build/check commands, generated-source rules, firmware/ROM coupling, permanent
bank protection, concurrency/ISR constraints, and links triggered by task type.
Preserve the original tutorial by default and scope opt-in platform changes
explicitly. Document repository-owned dependency integration: extracted feature
code and patches are tracked here; pristine dependency archives are verified;
feature PRs must not carry their implementation through submodule gitlinks.

Use explicit routing: bus/PIO or heartbeat changes → protocol/timing guide;
assembly/reflash → ROM guide; emulator/mapper → cosimulation guide; native
graphics → palette/OAM guide; device packaging → build/flash guide. Keep dated
results in evidence logs. Do not copy Codex-specific tool instructions, session
authorization rules, the Doom-only ban on all tutorial edits, or external skills
that upstream contributors cannot access.

Done when an agent can locate canonical sources and run the generic build from
a fresh checkout using only the file and its links. Validate all commands/links,
ensure nested guidance does not conflict, and keep skill details out of the
always-loaded root guide.

### U13 — organized platform agent skills

Create and revise these skills using Codex's built-in `skill-creator` skill and
its bundled initializer/validator. Locate it through the active Codex skill
catalog rather than committing a machine-specific installation path. Read its
`SKILL.md` before authoring and follow its guidance on focused scope, progressive
disclosure and behavioral validation. This is the required authoring workflow;
the resulting skills remain usable by other compatible agents.

Follow the open [Agent Skills standard](https://agentskills.io/home) and its
[format specification](https://agentskills.io/specification). Use
`.agents/skills/<name>/SKILL.md` as the versioned, tool-neutral source.
Each skill has `name`/`description`, precise triggers, prerequisites, ordered
steps, artifact requirements, failure handling and an observable completion
criterion. Shared technical reference stays in `docs/development/`; skills link
to it rather than repeating constants. Include a compact catalog indexed by
task and avoid relying on an installed Codex/Claude skill registry.

Use YAML frontmatter with a name matching its directory (1–64 lowercase
letters/digits/hyphens; no leading/trailing or repeated hyphens) and a nonempty
description of at most 1024 characters. Add optional metadata only when needed.
Keep conditional details in referenced resources and keep `SKILL.md` under the
standard's recommended 500-line limit. Use relative resource links; optional
`scripts/`, `references/` and `assets/` directories must serve a concrete need.
Codex-specific `agents/openai.yaml` may accompany the portable skill, with
automatic discovery preserved unless the user explicitly requests otherwise.

| Skill directory | Trigger and completion gate |
| --- | --- |
| `fcpico-build` | Reproduce ROM/host/device artifacts; finish with manifest and required fix-bank/flash checks |
| `fcpico-cosim` | Launch or test a project in Mesen; finish with settings, captures, traces and strict/diagnostic verdict |
| `fcpico-bus-debug` | Diagnose count/heartbeat/PIO failures; finish with address/count evidence, hypothesis and bounded next check |
| `fcpico-native-graphics` | Plan/edit tile/palette/OAM layouts; finish with hardware limits enabled and native frame/PPU/OAM artifacts |
| `fcpico-hardware-validation` | Prepare or record physical checks; finish with exact artifact, requested observations and honest unresolved checks |
| `fcpico-upstream-change` | Extract/review one platform contribution; finish with atomic scope, source/patch provenance, no feature gitlinks and regression evidence |

Skills must respect authorization already present in the conversation. A
physical-flash step can provide the concrete artifact and instructions when only
the user has the console; it must never claim that a requested hardware test ran.
Branch creation, pushes, publication and communication follow the agent runtime
and user's instructions rather than new blanket permission rituals.

For each skill, run the built-in creator's `scripts/quick_validate.py` and the
standard reference validator `skills-ref validate <skill-directory>`. Record
validator versions and results. Frontmatter validation does not establish
behavior: exercise realistic sample tasks, check resource links and run any
bundled scripts. Do not create implementation skills during this planning pass.

Done when both validation routes pass, sample tasks exercise each skill's route
and artifact checks, missing tools trigger actionable instructions, commands are
repository-relative, and model/runtime-specific wrappers (if provided) point to
the same skill source.
No generic Git/review/research skill collection is bundled unnecessarily.

## Repository-owned integration and exclusions

All feature branches and PRs in this plan target FC PICO. Extract only necessary
content from the referenced submodules into tracked source, tests or patch
bundles. Keep Mesen-specific files under `sim/mesen/`, separate from core firmware.
U06 consumes U05's tracked integration and checksum-verified pristine base;
updating that base requires rechecking patch application and regression gates.
Do not use a fork's feature commit as a substitute for the local source/patches
and do not claim support in an unmodified upstream Mesen build.

Inspect the engine fork for the BOOTSEL polling helper and application callback
ideas. Production renderer composition, menus, Doomguy animation, WAD/WHX
packing, input mappings and engine host determinism tests stay in the Doom
project. The generic demonstration must build with the engine submodule absent.
Any reusable engine-derived helper needed by a platform feature is extracted
into ordinary FC PICO files with its provenance, license and focused tests.
Application-only follow-ups remain outside this contribution plan.

Preserve source licenses: mapper code is GPL-3.0-or-later; newly authored
platform code commonly carries BSD-3-Clause; engine-derived code has its own GPL
obligations. Audit copied files and upstream/vendor attribution rather than
relabeling them. Use original redistributable test patterns and small trace
fixtures. Do not contribute Doom WAD/WHX files, extracted copyrighted face/logo
art or application screenshots as generic platform examples.

## Execution checklist for each future PR

1. Fetch upstream, record base SHA, and create the feature's named branch in an
   isolated checkout/worktree. Verify source snapshots and pristine dependency
   archive checksums. Every feature branch is in this repository.
2. Write a short file/symbol inventory and select only the feature's sources.
   Adapt paths/interfaces deliberately; do not cherry-pick broad Doom commits.
3. Implement the smallest complete feature plus its documented usage and
   meaningful regression checks. Keep compatibility changes visible in review.
4. Run the row's acceptance gates and the existing upstream tutorial build.
   Distinguish model/ROM assertions from physical results. Record remaining gates.
5. Audit imports, build dependencies, licenses, generated artifacts and fixtures.
   Prove no hidden Doom data/engine dependency. Confirm that the feature adds no
   gitlinks or submodule pin changes; all feature implementation is tracked here.
6. Produce a manifest/evidence bundle and an atomic PR description naming the
   problem, behavior, supported configurations, tests and limitations.
7. Rebase dependent FC PICO feature branches after prerequisites merge. Re-run
   patch application and affected compatibility checks against the final
   upstream base. Publish each atomic PR from its branch in this repository.

Overall completion: an upstream contributor can clone FC PICO, build and run a
non-Doom application alongside Mesen with strict hardware limits, inspect its
native artifacts using Python without porting its firmware logic, reproduce a
flashable build, enter BOOTSEL over serial, and follow platform docs/agent skills.
Every contribution is independently reviewable on its own feature branch.
