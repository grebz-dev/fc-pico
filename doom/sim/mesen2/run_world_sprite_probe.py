#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""World sprite review samples on the current HUD ROM and FcPico cosimulator.

Controlled native-coordinate masks are fixtures, not engine depth extraction.
The 6502 control executes LUT routing on the NES CPU while rendering is off.
All three candidates use the same whole-actor admission and immutable setup.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'doom/tools'))
from build_native_status_probe import native_status_source
from build_sprite_probe import SOURCE, ANCHOR, TABLE_ANCHOR, table
from build_world_sprite_art import (build, pack_tile, row_recipe, routing_table,
                                    scale_lut, scale_nearest)
from fcpico import protocol, stream
from ppu_decode import decode_with_sprites

WORLD_CHR = 0x1380  # Gameplay borrowing of menu font tiles; restore needed before menus
WORLD_TILES = 40
WORLD_OAM = 35
WORLD_ENTRIES = 29


def run(command: list[str], log: Path, cwd: Path | None = None, env: dict | None = None) -> None:
    with log.open('w') as output:
        subprocess.run(command, check=True, stdout=output, stderr=subprocess.STDOUT, cwd=cwd, env=env)


def upload(label: str, data: bytes, address: int) -> str:
    lines = [f'        lda #${address >> 8:02X}', '        sta $2006',
             f'        lda #${address & 255:02X}', '        sta $2006']
    for page in range((len(data) + 255) // 256):
        size = min(256, len(data) - 256 * page)
        lines += ['        ldx #0', f'.{label}_{page}:',
                  f'        lda {label}+${page * 256:04X},x', '        sta $2007', '        inx']
        if size < 256:
            lines += [f'        cpx #{size}']
        lines += [f'        bne .{label}_{page}']
    return '\n'.join(lines) + '\n'


def control_source(recipes: list, expected: bytes) -> tuple[str, dict]:
    """Generalized reference byte LUT; per-byte recipes preserve sliver phase.

    Plane rows are emitted in NES order. Integer DDA row selection is encoded
    in recipe inputs; the CPU evaluates the two bitplanes and visibility mask.
    This is a bounded setup control, not the unmodified reference demo.
    """
    routes = {tuple([-1] * 8): 0}
    for parts, mask in recipes:
        for value, route in parts:
            routes.setdefault(route, len(routes))
    if len(routes) > 20:
        raise ValueError('control LUT budget exceeded')
    max_parts = max(len(parts) for parts, mask in recipes)
    encoded = bytearray()
    for parts, mask in recipes:
        parts = parts + [(0, tuple([-1] * 8))] * (max_parts - len(parts))
        for value, route in parts:
            encoded += bytes((value, 0xC1 + routes[route]))
        encoded.append(mask)
    lookup = ''.join(f'''        ldy #{part * 2 + 1}
        lda [$00],y
        sta $03
        dey
        lda [$00],y
        tay
        lda [$02],y
        ora $04
        sta $04
''' for part in range(max_parts))
    routine = f'''WORLD_SCALE:
        lda #LOW(world_recipes)
        sta $00
        lda #HIGH(world_recipes)
        sta $01
        lda #0
        sta $02
        lda #LOW({len(expected)})
        sta $05
        lda #HIGH({len(expected)})
        sta $06
        lda #$13
        sta $2006
        lda #$80
        sta $2006
.world_byte:
        lda #0
        sta $04
{lookup}        ldy #{max_parts * 2}
        lda [$00],y
        and $04
        sta $2007
        clc
        lda $00
        adc #{max_parts * 2 + 1}
        sta $00
        bcc .world_no_carry
        inc $01
.world_no_carry:
        lda $05
        bne .world_dec_low
        dec $06
.world_dec_low:
        dec $05
        lda $05
        ora $06
        bne .world_byte
        rts
'''
    tables = '        org $C100\n' + ''.join(table(f'world_lut_{idx}', routing_table(route))
                                            for route, idx in routes.items())
    tables += table('world_recipes', encoded)
    if 256 + len(routes) * 256 + len(encoded) > 8192:
        raise ValueError('6502 control exceeds isolated bank')
    return routine + tables, {'lut_bytes': len(routes) * 256, 'recipe_bytes': len(encoded),
        'setup_only': True, 'reference': 'reference/scaling-wip byte LUT concept; global DDA routing adaptation'}


def measure_control(rom: Path, expected: bytes) -> int:
    """Cycle count of the assembled setup scaler, independently executed by py65."""
    from py65.devices.mpu6502 import MPU

    class Memory(list):
        def __init__(self):
            super().__init__([0] * 65536)
            self.ppu_bytes = bytearray()

        def __setitem__(self, address, value):
            if address == 0x2007:
                self.ppu_bytes.append(value)
            super().__setitem__(address, value)

    memory = Memory()
    memory[0x8000:] = rom.read_bytes()[16:16 + 32768]
    cpu = MPU(memory=memory, pc=0xC000)
    for _ in range(1000000):
        if memory[cpu.pc] == 0x60:
            if memory.ppu_bytes != expected:
                raise AssertionError('independent 6502 execution differs from reference CHR')
            return cpu.processorCycles + 6
        cpu.step()
    raise AssertionError('6502 scaler did not terminate')


def make_rom(output: Path, chr_bytes: bytes, oam: bytes, recipes: list, candidate: str) -> dict:
    source = native_status_source()
    setup = upload('world_chr', chr_bytes, WORLD_CHR)
    control = {}
    extra = table('world_chr', chr_bytes)
    if candidate == '6502-lut':
        extra, control = control_source(recipes, chr_bytes)
        setup = '        jsr WORLD_SCALE\n'
    setup += '        jsr WORLD_APPLY\n'
    if source.count(ANCHOR) != 1:
        raise ValueError('world setup anchor changed')
    source = source.replace(ANCHOR, setup + ANCHOR)
    # UI hides menu slots every packet. Restore the world only for gameplay.
    point = '        jsr MENU_APPLY\n        lda #1\n        sta $0310'
    source = source.replace(point, '        jsr MENU_APPLY\n        jsr WORLD_APPLY\n        lda #1\n        sta $0310')
    apply = f'''WORLD_APPLY:
        lda $0301
        and #5
        cmp #1
        bne .world_hidden
        ldx #0
.world_oam:
        lda world_oam,x
        sta $028C,x
        inx
        cpx #{len(oam)}
        bne .world_oam
.world_hidden:
        rts
'''
    extra = '        .bank 2\n        org $C000\n' + extra + apply + table('world_oam', oam)
    source = source.replace(TABLE_ANCHOR, extra + TABLE_ANCHOR)
    source = source.replace('20DOOM-04-9016', {'resident-bands':'20DOOM-04-9020',
                           'rp-lut':'20DOOM-04-9021', '6502-lut':'20DOOM-04-9022'}[candidate])
    (output / 'probe.asm').write_text(source)
    temporary = SOURCE.parent / 'PG_world_probe_tmp.asm'
    try:
        temporary.write_text(source)
        run([str(ROOT / '.build/tools/nesasm/nesasm'), '-s', '-f', '-o',
             str(output / 'probe.raw.nes'), temporary.name], output / 'assembly.log', SOURCE.parent)
    finally:
        temporary.unlink(missing_ok=True)
    run([sys.executable, str(ROOT / 'doom/tools/nes/normalize_ines.py'),
         str(output / 'probe.raw.nes'), str(output / 'probe.nes')], output / 'normalize.log')
    run([sys.executable, str(ROOT / 'doom/tools/nes/check_fixbank.py'),
         str(output / 'probe.nes'), str(ROOT / 'tutorial_project/BOOTROM/bootrom_fixr.bin')],
        output / 'fixbank.log')
    return control


def actors(art: Path, manifest: dict, scene: str, candidate: str) -> list[dict]:
    patches = {patch['lump']: patch for patch in manifest['patches']}
    requests = [('TROOA1', 24 if scene != 'near' else 64, 65, 164),
                ('TROOA1', 13, 133, 148), ('STIMA0', 12, 180, 172)]
    if scene == 'crowded':
        requests = [('TROOA1', 16, 24 + i * 27, 160) for i in range(7)]
    result = []
    for instance, (name, requested, anchor_x, foot_y) in enumerate(requests):
        source = np.load(art / f'{name}.npy', allow_pickle=False)
        patch = patches[name]
        width = requested
        if candidate == 'resident-bands':
            widths = manifest['scope']['families'][name[:4]]['scale_widths']
            # Do not clamp enlargement to a small resident silhouette.
            if width <= max(widths):
                width = min(widths, key=lambda band: (abs(band - width), band))
        height = max(1, source.shape[0] * width // source.shape[1])
        started = time.perf_counter_ns()
        if candidate == 'resident-bands' and requested <= 24:
            packed = (art / f'{name}-{width}.chr').read_bytes()
            padded = np.zeros(((height + 7) // 8 * 8, (width + 7) // 8 * 8), np.uint8)
            offset = 0
            for ty in range(0, height, 8):
                for tx in range(0, width, 8):
                    planes = np.unpackbits(np.frombuffer(packed[offset:offset + 16], np.uint8)).reshape(2, 8, 8)
                    padded[ty:ty + 8, tx:tx + 8] = planes[0] + 2 * planes[1]
                    offset += 16
            pixels = padded[:height, :width]
        else:
            pixels = scale_lut(source, width, height)
        elapsed = time.perf_counter_ns() - started
        if not np.array_equal(pixels, scale_nearest(source, width, height)):
            raise AssertionError('scaled candidate differs from independent scalar sampling')
        # Original origin defines world position; use a stable foot reference.
        x = anchor_x - patch['left'] * width // source.shape[1]
        y = foot_y - patch['top'] * height // source.shape[0]
        visible = np.ones(pixels.shape, bool)
        yy, xx = np.indices(pixels.shape)
        nx, ny = xx + x, yy + y
        if scene == 'corner':
            visible &= ~((nx >= 68) & (nx < 80) & (ny < 164))
        if scene == 'window':
            visible &= ~((nx >= 54) & (nx < 86) & (((ny >= 137) & (ny < 144)) |
                         ((ny >= 152) & (ny < 156)) | ((nx >= 65) & (nx < 69))))
        visible &= (nx >= 0) & (nx < 256) & (ny >= 8) & (ny < 192)
        result.append(dict(id=instance, lump=name, source=source, pixels=pixels,
                           visible=visible, x=x, y=y, width=width, requested_width=requested,
                           height=height, scale_host_ns=elapsed,
                           palette=1 if name.startswith('TROO') else 3))
    return result


def allocate(instances: list[dict]) -> tuple[bytes, bytes, list, dict]:
    """Atomic whole-actor admission; CHR and scanline counts include empty rows."""
    entries, art, recipes, reports = bytearray(), bytearray(), [], []
    occupancy = np.zeros(240, np.int16)
    # Current gameplay HUD (16 face, six arms, three keys); reserve exact lines.
    for y, columns in ((200, 4), (208, 4), (216, 4), (224, 4), (201, 3), (213, 3),
                       (201, 1), (213, 1), (225, 1)):
        occupancy[y:y + 8] += columns
    for actor in instances:
        tiles, tile_recipes, trial_oam = bytearray(), [], bytearray()
        trial_lines = occupancy.copy()
        pix, mask = actor['pixels'], actor['visible']
        for ty in range(0, actor['height'], 8):
            for tx in range(0, actor['width'], 8):
                cropped = pix[ty:ty + 8, tx:tx + 8] * mask[ty:ty + 8, tx:tx + 8]
                if not cropped.any():
                    continue
                sx, sy = actor['x'] + tx, actor['y'] + ty
                # A partial tile outside the OAM placement range cannot wrap.
                if sx < 0 or sx > 255 or sy < 1 or sy > 231:
                    trial_lines[:] = 100  # whole actor fallback
                else:
                    trial_lines[sy:sy + 8] += 1
                tile_id = (WORLD_CHR - 0x1000 + len(art) + len(tiles)) // 16
                trial_oam.extend((max(0, min(255, sy - 1)), tile_id & 255, actor['palette'],
                                  max(0, min(255, sx))))
                tiles += pack_tile(cropped)
                for plane in (0, 1):
                    for row in range(8):
                        visible_bits = sum(int(mask[ty + row, tx + bit]) << (7 - bit)
                            for bit in range(8) if ty + row < mask.shape[0] and tx + bit < mask.shape[1])
                        tile_recipes.append((row_recipe(actor['source'], actor['width'],
                            actor['height'], tx, ty, row, plane), visible_bits))
        reason = None
        if actor['requested_width'] > 24:
            reason = 'near-view enlargement outside resident/cache probe'
        elif len(entries) + len(trial_oam) > WORLD_ENTRIES * 4:
            reason = 'world OAM capacity'
        elif len(art) + len(tiles) > WORLD_TILES * 16:
            reason = 'world CHR capacity'
        elif trial_lines.max() > 8:
            reason = 'combined scanline capacity or offscreen tile placement'
        if reason is None:
            entries += trial_oam
            art += tiles
            recipes += tile_recipes
            occupancy = trial_lines
        reports.append({key: actor[key] for key in ('id','lump','x','y','width','height','requested_width','scale_host_ns')} |
                       {'admitted': reason is None, 'fallback': reason,
                        'visible_pixels': int(np.count_nonzero(pix * mask)), 'entries': len(trial_oam) // 4})
    oam = bytes(entries) + bytes((0xEF, 0, 0, 0)) * (WORLD_ENTRIES - len(entries) // 4)
    return bytes(art), oam, recipes, {'actors': reports, 'world_oam': len(entries) // 4,
        'world_chr_bytes': len(art), 'combined_peak_scanline': int(occupancy.max()),
        'scanline_occupancy': occupancy.tolist()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wad', required=True, type=Path)
    parser.add_argument('--background', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('/tmp/fcpico-world-sprites'))
    parser.add_argument('--frames', type=int, default=180)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scope = json.loads((ROOT / 'doom/plan/world-sprite-scope.json').read_text())
    manifest = build(args.wad, scope, output / 'art')
    original = args.background.read_bytes()
    if len(original) != protocol.VRAM_BUF_BYTES_V3:
        raise ValueError('background must be a native status v4 stream')
    background = stream.decode_frame(stream.expand_native_text_stream(original))
    mailbox = original[protocol.VRAM_MAILBOX_OFF_V4:protocol.VRAM_MAILBOX_OFF_V4 + protocol.FC_COM_BUF_SIZE_V4]
    if not mailbox[protocol.MBX_FLAGS] & protocol.MBX_FLAG_V4:
        raise ValueError('background must use the v4 mailbox layout')
    ui_flags = mailbox[protocol.MBX_UI + 1]
    if ui_flags & 5 != 1:
        raise ValueError('this probe requires gameplay with the native status visible')
    summary, images = {}, []
    for scene in ('open', 'corner', 'window', 'crowded', 'near'):
        for candidate in ('resident-bands', 'rp-lut', '6502-lut'):
            directory = output / scene / candidate
            directory.mkdir(parents=True, exist_ok=True)
            instances = actors(output / 'art', manifest, scene, candidate)
            art, oam, recipes, report = allocate(instances)
            pixels = background.copy()
            # The fixture's foreground geometry matches the visibility masks.
            if scene == 'corner':
                pixels[105:164, 68:80] = 2
            elif scene == 'window':
                pixels[137:144, 54:86] = 2
                pixels[152:156, 54:86] = 2
                pixels[105:164, 65:69] = 2
            # Render rejected actors wholly into the stream for this generation.
            for actor, admission in zip(instances, report['actors']):
                masked = actor['pixels'] * actor['visible']
                Image.fromarray((actor['visible'] * 255).astype(np.uint8)).save(directory / f"mask-{actor['id']}.png")
                if admission['admitted']:
                    continue
                for ay, ax in zip(*np.nonzero(masked)):
                    x, y = actor['x'] + ax, actor['y'] + ay
                    if 0 <= x < 256 and 8 <= y < 192:
                        pixels[y, x] = masked[ay, ax]
            frame = directory / 'frame.bin'
            frame.write_bytes(stream.encode_frame(pixels, mailbox, native_text=True))
            if not art:
                raise AssertionError('sample has no admitted actors')
            report['control'] = make_rom(directory, art, oam, recipes, candidate)
            if candidate == '6502-lut':
                report['control']['py65_cycles'] = measure_control(directory / 'probe.nes', art)
            env = dict(os.environ, FCPICO_WORLD_BASE_LUA=str(HERE / 'lua/D0.lua'))
            run([sys.executable, str(HERE / 'run_doom_frame.py'), str(frame), '--rom',
                 str(directory / 'probe.nes'), '--output', str(directory / 'mesen'),
                 '--frames', str(args.frames), '--min-correlation', '0.70',
                 '--lua-script', str(HERE / 'lua/world_sprites.lua')], directory / 'cosim.log', env=env)
            dumps = directory / 'mesen/run'
            actual_oam = (dumps / 'oam.bin').read_bytes()
            actual_chr = (dumps / 'sprite_chr.bin').read_bytes()
            if actual_oam[WORLD_OAM * 4:] != oam:
                raise AssertionError('NES OAM differs from whole-actor admission')
            if actual_chr[WORLD_CHR - 0x1000:WORLD_CHR - 0x1000 + len(art)] != art:
                raise AssertionError('NES scaled/masked CHR differs from scalar reference')
            reference = decode_with_sprites(stream.expand_native_text_stream(frame.read_bytes()),
                actual_oam, actual_chr, (dumps / 'sprite_pal.bin').read_bytes(),
                (dumps / 'attributes.bin').read_bytes(), (dumps / 'palette.bin').read_bytes())
            Image.fromarray(reference).save(directory / 'reference.png')
            baseline = directory / 'baseline'
            baseline.mkdir(exist_ok=True)
            make_rom(baseline, art, bytes((0xEF, 0, 0, 0)) * WORLD_ENTRIES, recipes, 'resident-bands')
            run([sys.executable, str(HERE / 'run_doom_frame.py'), str(frame), '--rom',
                 str(baseline / 'probe.nes'), '--output', str(baseline / 'mesen'),
                 '--frames', str(args.frames)], baseline / 'cosim.log')
            base_dumps = baseline / 'mesen/run'
            base_reference = decode_with_sprites(stream.expand_native_text_stream(frame.read_bytes()),
                (base_dumps / 'oam.bin').read_bytes(), (base_dumps / 'sprite_chr.bin').read_bytes(),
                (base_dumps / 'sprite_pal.bin').read_bytes(),
                (base_dumps / 'attributes.bin').read_bytes(), (base_dumps / 'palette.bin').read_bytes())
            actual = np.asarray(Image.open(dumps / 'final.png').convert('RGB'))
            base_actual = np.asarray(Image.open(base_dumps / 'final.png').convert('RGB'))
            changed = np.any(actual != base_actual, axis=2)
            expected_changed = np.any(reference != base_reference, axis=2)
            if not np.array_equal(changed, expected_changed):
                raise AssertionError(f'composited sprite mask mismatch: {np.count_nonzero(changed != expected_changed)}')
            report['exact_changed_pixels'] = int(changed.sum())
            observed_lines = [sum(actual_oam[n * 4] + 1 <= y < actual_oam[n * 4] + 9
                                  for n in range(64)) for y in range(240)]
            if max(observed_lines) > 8:
                raise AssertionError('actual combined OAM exceeds sprite evaluation capacity')
            report['observed_scanline_occupancy'] = observed_lines
            report['observed_peak_scanline'] = max(observed_lines)
            for capture in ('consecutive-1.png', 'consecutive-2.png'):
                if (dumps / capture).read_bytes() != (dumps / 'final.png').read_bytes():
                    raise AssertionError('fixed generation changed across consecutive frames')
            report['nmi'] = json.loads((dumps / 'lua_result.json').read_text())
            report['screenshot_sha256'] = hashlib.sha256((dumps / 'final.png').read_bytes()).hexdigest()
            report['evidence'] = 'exact CHR/OAM and changed-pixel mask against paired Mesen baseline; 3 consecutive frames; real sprite limit; stable stream/NMI'
            (directory / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            summary[f'{scene}/{candidate}'] = report
            images.append((scene, candidate, Image.open(dumps / 'final.png').convert('RGB').copy()))
            print(f"{scene}/{candidate}: OAM={report['world_oam']}, CHR={len(art)}, line={report['combined_peak_scanline']}", flush=True)
    sheet = Image.new('RGB', (3 * 512, 5 * 504), '#202020')
    draw = ImageDraw.Draw(sheet)
    for n, (scene, candidate, image) in enumerate(images):
        x, y = n % 3 * 512, n // 3 * 504
        draw.text((x + 8, y + 5), f'{scene} / {candidate}', fill='white')
        sheet.paste(image.resize((512, 480), Image.Resampling.NEAREST), (x, y + 24))
    sheet.save(output / 'comparison.png')
    summary_path = output / 'summary.json'
    summary_path.write_text(json.dumps(summary, indent=2) + '\n')
    lines = ['# World sprite cosimulation samples', '',
        'Controlled visibility fixtures over a captured Doom frame; 9016 native HUD. '
        'The background can contain its original streamed actors. Probe actors are added '
        'at explicit fixture coordinates, not replacements for actors in that recording.', '',
        'All art is WAD-derived. Hardware, runtime upload scheduling, engine visibility, '
        'WHX identity and audio contention remain unproven. No device firmware was built.', '',
        '| Scene / approach | Native actors / requested | OAM | CHR bytes | Actual peak line | Exact changed pixels | 6502 setup cycles |',
        '|---|---:|---:|---:|---:|---:|---:|']
    for key, report in summary.items():
        admitted = sum(actor['admitted'] for actor in report['actors'])
        lines.append(f"| {key} | {admitted}/{len(report['actors'])} | {report['world_oam']} | "
                     f"{report['world_chr_bytes']} | {report['observed_peak_scanline']} | "
                     f"{report['exact_changed_pixels']} | {report['control'].get('py65_cycles', '—')} |")
    lines += ['', 'Each sample retains its ROM, source, stream, masks, paired baseline, '
        'three consecutive Mesen PNGs, exact OAM/CHR dumps, bus trace and NMI log. '
        'RemoveSpriteLimit is false. The independent scalar sampler checks LUT and '
        'resident output; py65 also executes the assembled 6502 scaler independently.', '',
        'Resident bands use generated CHR from disk (a flash-residency prototype). '
        'RP LUT executes on the host; its Python timings are not RP2350 measurements. '
        '6502 LUT runs on the NES CPU with rendering disabled, then stays resident. '
        'Its generalized routing/DDA recipes adapt the reference byte LUT idea, '
        'not the original demo’s 74-cycle row schedule.', '',
        'Gameplay borrows $1380..$15FF (40 tiles), which includes menu font glyphs. '
        'This fits these fixed gameplay fixtures. A menu transition must restore '
        'the font before publication; no permanent free CHR claim is made. '
        'Enemy art shares face palette 1; pickup art shares arms/key palette 3. '
        'The 29 world OAM slots retain the 35-slot HUD partition.', '',
        'Crowded: four of seven actors admitted, three rendered wholly in the fixture stream. '
        'Near: the 64-pixel actor is rendered wholly in the fixture stream. '
        'These fallback pixels are a fixture renderer, not the engine’s legacy conversion.', '',
        '## Excluded alternatives', '',
        'Ordinary behind-BG priority hides visible actors wherever a far wall/floor uses '
        'nonzero BG. BG holes still leak actor pixels wherever nearer geometry naturally '
        'uses zero; eliminating those zeros changes the existing four-color world encoding. '
        'Mask sprites have the same zero-BG leakage and consume additional scanline slots '
        '(the crowded fixture already fills eight). Tile-aligned cropping loses visible '
        'parts of partially occluded tiles, so it is ineffective at the tested thin corners '
        'and disjoint windows. All are unsuitable as general clipping replacements.', '',
        'Traditional mapper banking/IRQs do not supply pixel visibility and are not '
        'available in the present FcPico transport. Fetch substitution lacks demonstrated '
        'PPU address/instance signals on this board. Those hardware redesigns have no '
        'present-board cosimulation candidate here.', '',
        '## Next gate', '',
        'Review these samples before device firmware. Then resolve engine ownership and '
        'background restoration, runtime tile double buffering, menu font restoration, '
        'WHX frame identities, bounded cache cost and audio/CHR upload deadlines. '
        'P3-W1/W2 are partial; W3/W4/W5 remain open.', '']
    (output / 'review.md').write_text('\n'.join(lines))
    rows = ''.join(f'<h3>{scene} / {candidate}</h3><a href="{scene}/{candidate}/report.json">metrics</a> '
                   f'<a href="{scene}/{candidate}/baseline/mesen/run/final.png">paired baseline</a><br>'
                   f'<img width="512" height="480" style="image-rendering:pixelated" '
                   f'src="{scene}/{candidate}/mesen/run/final.png">'
                   for scene, candidate, image in images)
    (output / 'review.html').write_text('<!doctype html><meta charset="utf-8"><title>World sprites</title>'
        '<style>body{background:#202020;color:white;font:16px sans-serif}a{color:#9df}</style>'
        '<h1>World sprite cosimulation samples</h1><p>Controlled masks over Doom background; '
        'not engine migration or runtime upload evidence.</p><a href="review.md">Report</a>'
        '<br><img width="1200" src="comparison.png">' + rows)
    print(output / 'comparison.png')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
