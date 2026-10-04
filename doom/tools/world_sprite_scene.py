# SPDX-License-Identifier: BSD-3-Clause
"""Bounded native sprite composition from resolved engine ownership.

PD already samples scaled WHX art. Reuse those exact pixels, then map through
fcvideo's native sampling, quantize each 8x8 tile, and atomically admit actors.
"""
from __future__ import annotations
import csv
from pathlib import Path
import re
import numpy as np
from build_world_sprite_art import pack_tile
from fcvideo_ref import decimate_320_to_256, place_letterbox

ROOT = Path(__file__).resolve().parents[2]
# The older project RGB table differs substantially from this cosimulator's
# NTSC palette. Fit and independently decode against the pinned 2C02 model.
def mesen_palette() -> np.ndarray:
    source = (ROOT / 'doom/sim/mesen2/Mesen2/Core/NES/NesDefaultVideoFilter.cpp').read_text()
    row = source.split('/* 2C02 */', 1)[1].split('}', 1)[0]
    values = [int(v,16) for v in re.findall(r'0xFF([0-9A-Fa-f]{6})',row)]
    if len(values)!=64:
        raise ValueError('Mesen 2C02 palette format changed')
    return np.array([[(v>>16)&255,(v>>8)&255,v&255] for v in values],np.uint8)

NES = mesen_palette()
NAMES = re.findall(r'\bSPR_([A-Z0-9]+)\s*,', (ROOT / 'doom/rp2040-doom/src/doom/info.h').read_text())
SUPPORTED = set('TROO POSS SPOS SARG HEAD BOSS SKUL STIM MEDI ARM1 ARM2 BON1 BON2 CLIP AMMO SHEL SBOX BPAK BKEY RKEY YKEY BSKU RSKU YSKU SOUL PVIS SUIT'.split())
SHARED = np.array([[0x16, 0x0f, 0x16], [0x07, 0x18, 0x26],
                   [0x12, 0x22, 0x38], [0x12, 0x38, 0x16]], np.uint8)


def native(source: np.ndarray) -> np.ndarray:
    return place_letterbox(decimate_320_to_256(source))


def load_frame(base: Path, raw: Path, number: int, playpal: np.ndarray) -> tuple[list[dict], np.ndarray, np.ndarray]:
    stem = f'frame{number:06d}'
    state = [int(v) for v in (base / f'{stem}.state').read_text().strip().split(',')]
    owner_source = np.fromfile(base / f'{stem}.owners', np.uint8).reshape(200, 320)
    owners = native(owner_source)
    fuzzy = np.fromfile(base / f'{stem}.fuzz', np.uint8).reshape(200,320).astype(bool)
    fuzzy_dilated = fuzzy.copy()
    fuzzy_dilated[1:] |= fuzzy[:-1]
    fuzzy_dilated[:-1] |= fuzzy[1:]
    spans = np.fromfile(base / f'{stem}.spans', '<u2').reshape(-1,4)
    weapon = native(np.fromfile(base / f'{stem}.weapon', np.uint8).reshape(200, 320)).astype(bool)
    colors = native(np.fromfile(base / f'{stem}.unlit', np.uint8).reshape(200, 320))
    lit = native(np.fromfile(raw / f'{stem}.raw', np.uint8).reshape(200, 320))
    # Keep lighting but lift saturated art toward its original PLAYPAL treatment.
    rgb = (0.85 * playpal[colors] + 0.15 * playpal[lit]).astype(np.float32)
    luminance = (rgb * np.array([.25,.5,.25],np.float32)).sum(axis=-1,keepdims=True)
    rgb = np.clip(luminance + 1.2 * (rgb - luminance),0,255)
    records = []
    with (base / f'{stem}.actors.csv').open() as file:
        for row in csv.DictReader(file):
            actor = {k: int(v) for k, v in row.items()}
            actor['family'] = NAMES[actor['sprite']]
            mask = (owners == actor['id']) & ~weapon
            mask[:8] = False
            mask[192:] = False
            actor['fuzz_dependency'] = bool(np.any((owner_source==actor['id']) & fuzzy_dilated))
            projected = np.zeros((200,320),bool)
            for owner,x,yl,yh in spans[spans[:,0]==actor['id']]:
                projected[yl:yh+1,x] = True
            # Geometry owns ID zero. Exclude fuzzy footprints: they are a later pass.
            actor['geometry_hidden_pixels'] = int(np.count_nonzero(projected & (owner_source==0) & ~fuzzy_dilated))
            actor['mask'] = mask
            actor['rgb'] = rgb
            actor['eligible'] = (state[0] == 0 and state[1] == 3 and state[2] == 0 and
                state[3] == state[4] == 0 and actor['family'] in SUPPORTED and
                bool(actor['flags'] & (1 | 0x400000)) and not actor['fuzz_dependency'] and
                not actor['flags'] & (0x40000 | 0x100000 | 0xc000000) and actor['light'] >= 0)
            actor['native_pixels'] = int(mask.sum())
            records.append(actor)
    return records, owners, weapon


def tiles(actor: dict):
    """Use fixed screen tile boundaries: no offscreen OAM wrap or masked slivers."""
    mask = actor['mask']
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return
    for y in range(int(ys.min()) // 8 * 8, int(ys.max()) + 1, 8):
        for x in range(int(xs.min()) // 8 * 8, int(xs.max()) + 1, 8):
            visible = mask[y:y + 8, x:x + 8]
            if visible.any():
                yield x, y, visible, actor['rgb'][y:y + 8, x:x + 8]


def color_distance(rgb: np.ndarray) -> np.ndarray:
    """Weighted luminance plus chroma error discourages gray substitutions."""
    target = np.asarray(rgb, np.float32)
    diff = target[..., None, :] - NES.astype(np.float32)
    luma = (diff * np.array([.25, .5, .25], np.float32)).sum(axis=-1)
    chroma = diff - luma[..., None]
    # Hue matters for recognition: a brown imp must not become olive merely
    # because the darker green entry has a slightly closer luminance.
    target_c = target - target.mean(axis=-1,keepdims=True)
    palette_c = NES.astype(np.float32) - NES.mean(axis=-1,keepdims=True)
    target_norm = np.linalg.norm(target_c,axis=-1)
    palette_norm = np.linalg.norm(palette_c,axis=-1)
    cosine = (target_c[...,None,:] * palette_c).sum(axis=-1) / np.maximum(target_norm[...,None]*palette_norm,1)
    hue = 3000 * (1-np.clip(cosine,-1,1)) * np.minimum(target_norm[...,None]/25,1)
    return luma * luma + 1.4 * (chroma * chroma).sum(axis=-1) + hue


def palette_study(samples: list[np.ndarray]) -> tuple[np.ndarray, dict]:
    """Fit two gameplay palettes; preserve the face and keys/arms palettes."""
    # Equal-sized stratified samples prevent huge near actors dominating.
    sample = np.stack([pixels[np.linspace(0, len(pixels) - 1, 16).astype(int)]
                       for pixels in samples[::max(1, len(samples) // 600)]])
    distances = color_distance(sample)
    permitted = [v for v in range(64) if v % 16 < 13 and v not in (0x20, 0x3c)] + [0x0f]
    def loss(palettes, per_tile=True, slots=(0, 1, 2, 3)):
        errors = np.stack([distances[..., palettes[p]].min(axis=-1).mean(axis=-1) for p in slots], axis=1)
        return float(errors.min(axis=1).mean()) if per_tile else float(errors.mean())
    trials = []
    best = None
    for name, extra in [('red-green-white', ((0x0f,0x16,0x30),(0x0b,0x2a,0x30))),
                        ('blue-green-white', ((0x0f,0x19,0x30),(0x02,0x22,0x30))),
                        ('warm-cool', ((0x06,0x16,0x30),(0x09,0x29,0x31)))]:
        palettes = SHARED.copy()
        palettes[0], palettes[2] = extra
        initial = loss(palettes)
        for _ in range(3):
            changed = False
            for p in (0, 2):
                for slot in range(3):
                    before = int(palettes[p, slot])
                    selected, cost = before, loss(palettes)
                    for color in permitted:
                        if color in [int(palettes[p, i]) for i in range(3) if i != slot]:
                            continue
                        palettes[p, slot] = color
                        # Retain a bright accent and chromatic color in each
                        # gameplay palette; unconstrained error favors dull grays.
                        if not any(int(c)>=0x20 for c in palettes[p]) or not any(1<=int(c)%16<=12 for c in palettes[p]):
                            continue
                        error = loss(palettes)
                        if error < cost - 1e-4:
                            selected, cost = color, error
                    palettes[p, slot] = selected
                    changed |= before != selected
            if not changed:
                break
        final = loss(palettes)
        trials.append({'seed':name, 'initial_error':initial, 'optimized_error':final,
                       'palettes':palettes.tolist()})
        if best is None or final < best[0]:
            best = final, palettes.copy()
    return best[1], {'sample_tiles':len(sample), 'metric':'luminance + 1.4 chroma squared; hue-direction penalty protects actor identity',
        'rgb_calibration':'pinned Mesen 2C02 NTSC palette',
        'lighting':'85% original + 15% engine shaded RGB, 1.2x chroma; bright accent per gameplay palette; tinted frames stay streamed',
        'shared_two_palette_error':loss(SHARED, slots=(1,3)),
        'shared_four_palette_error':loss(SHARED), 'trials':trials,
        'selected':best[1].tolist(), 'selected_error':best[0]}


def compose(records: list[dict], palettes: np.ndarray, mode: str = 'tile') -> tuple[bytes, bytes, np.ndarray, dict]:
    entries, patterns = bytearray(), bytearray()
    pattern_ids = {}
    occupancy = np.zeros(240, np.uint8)
    owner_mask = np.zeros((240, 256), bool)
    colored = np.zeros((240, 256, 3), np.uint8)
    report = []
    # Threats first, then nearer pickups. IDs only identify this immutable generation.
    for actor in sorted(records, key=lambda a:(-bool(a['flags'] & 0x400000), -a['scale'], a['id'])):
        fields = {k:v for k,v in actor.items() if k not in ('rgb','mask','eligible')}
        reason = None
        if not actor['eligible']:
            reason = 'unsupported actor/effect or non-gameplay/tinted frame'
        elif actor['native_pixels'] < 8:
            reason = 'too few visible pixels'
        if reason:
            report.append(fields | {'admitted':False, 'reason':reason})
            continue
        new_entries, new_patterns = bytearray(), bytearray()
        new_ids = dict(pattern_ids)
        new_lines = occupancy.copy()
        tile_images = []
        actor_palette = None
        slots = (1,3) if mode == 'shared' else (0,1,2,3)
        if mode == 'actor':
            distance = color_distance(actor['rgb'][actor['mask']])
            actor_palette = min(slots, key=lambda p:float(distance[:,palettes[p]].min(axis=1).sum()))
        for x,y,mask,rgb in tiles(actor):
            distance = color_distance(rgb)
            p = actor_palette if actor_palette is not None else min(slots,
                key=lambda p:float(distance[...,palettes[p]].min(axis=-1)[mask].sum()))
            indexed = np.where(mask, distance[...,palettes[p]].argmin(axis=-1) + 1, 0).astype(np.uint8)
            pattern = pack_tile(indexed)
            if pattern not in new_ids:
                new_ids[pattern] = 0x38 + len(new_ids)
                new_patterns += pattern
            new_entries += bytes((y-1,new_ids[pattern] & 255,p,x))
            new_lines[y:y+8] += 1
            tile_images.append((x,y,mask,NES[palettes[p]][np.maximum(indexed,1)-1]))
        if len(entries) + len(new_entries) > 29*4:
            reason = '29 world OAM slots'
        elif len(new_ids)>40:
            reason = '40 gameplay CHR tiles'
        elif new_lines.max()>8:
            reason = 'eight sprites per scanline'
        if reason is None:
            entries += new_entries
            patterns += new_patterns
            pattern_ids = new_ids
            occupancy = new_lines
            owner_mask |= actor['mask']
            for x,y,mask,rgb in tile_images:
                colored[y:y+8,x:x+8][mask] = rgb[mask]
        report.append(fields | {'admitted':reason is None,'reason':reason,'tiles':len(new_entries)//4})
    hidden = bytes((0xef,0,0,0))*(29-len(entries)//4)
    return bytes(patterns), bytes(entries)+hidden, colored, {
        'actors':report,'oam':len(entries)//4,'chr_bytes':len(patterns),
        'world_peak_line':int(occupancy.max()),'world_lines':occupancy.tolist(),
        'native_pixels':int(owner_mask.sum())}
