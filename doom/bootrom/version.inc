;/// @file version.inc
;/// @brief Build stamp of the Doom boot ROM, assembled at $EFF0 (14 characters + NUL).
;///
;/// Replaces the tutorial's wall-clock dbdate.h so builds are reproducible.
;/// Format: DOOM-<protocol version, 2 digits>-<build number, 6 digits>.
;/// Bump the build number on purpose only: the cartridge compares these 14 bytes
;/// with its own copy at every boot and a mismatch makes the console reflash itself
;/// (docs/pages/boot-and-reflash.md, doom/plan/03-protocol-v2.md "Version stamp").
;/// SPDX-License-Identifier: BSD-3-Clause
	DB "DOOM-02-000001",0
