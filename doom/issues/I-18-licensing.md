<!-- SPDX-License-Identifier: BSD-3-Clause -->
# I-18 Licensing enquiry and LICENSES.md

| | |
|---|---|
| **Lane** | C -- human |
| **Size** | S |
| **Depends on** | none |
| **Work plan task** | P0-T13 |

## Goal

The firmware will combine GPLv2 engine code with sample code whose terms are not stated,
and a modified boot ROM will be installed on other people's consoles. This needs a person,
not an agent, and it should be asked early enough that the answer can still change the
design.

## Specification

`plan/11-risks.md` R6 and H1; `docs/pages/references.md` for the third-party
components and how to reach impact soft.

## Owns (create or modify only these)

`LICENSES.md`, `HARDWARE-REQUESTS.md` (the HA-1 entry)

## Must not touch

`tutorial_project/**` (read it, copy from it, never edit it), `plan/**` unless the
issue says otherwise, `fcbus/fcbus_protocol.h` (regenerate through `tools/gen_protocol.py`),
and any file another open issue lists under **Owns**.

## Steps

1. Complete `LICENSES.md`: every component, its origin, its licence, and what it obliges.
2. Ask impact soft (product page, or X as `docs/pages/references.md` records) two questions:
   may the sample bus layer and boot ROM be redistributed inside a GPLv2 combined work, and
   do they object to a third-party firmware installing a modified boot ROM.
3. Record the answer in `LICENSES.md` and, if it constrains the design, in
   `plan/11-risks.md`.

## Acceptance

Every command must pass from the repository root.

```
# no automated check; the acceptance is a recorded answer in LICENSES.md
```

## Traps

`fcbus` was written from the protocol documentation rather than copied, which is the
position worth preserving. `fcppu.pio` is the exception: it is copied verbatim, and it
carries Raspberry Pi's BSD-3 header with impact soft modifications.
