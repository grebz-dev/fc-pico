#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Capture real PD visibility, plan native actors, replay with pre-insertion suppression.

Replay is an offline two-pass integration oracle. Coherent snapshots run through
Mesen's real PPU; it does not claim a device runtime upload implementation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0,str(ROOT/'doom/tools'))
from audio.wadextract import read_wad
from world_sprite_scene import load_frame, tiles, palette_study, compose, SHARED, NES
from run_world_sprite_probe import make_rom, upload
from fcpico import protocol, stream
from ppu_decode import decode_to_rgb, decode_with_sprites


def run(command, logfile):
    with logfile.open('w') as f:
        subprocess.run(list(map(str,command)),check=True,stdout=f,stderr=subprocess.STDOUT)


def capture(args, out, admission=None):
    out.mkdir(parents=True,exist_ok=True)
    for folder in ('base','raw','stream'):
        (out/folder).mkdir(exist_ok=True)
    command=[args.engine,'--whx',ROOT/'doom/rp2040-doom/doom1.whx','--demo',str(args.demo),
             '--frames',args.frames,'--lockstep','--world-capture',out/'base',
             '--dump-8bit',out/'raw','--dump-stream',out/'stream']
    if admission:
        command += ['--world-admit',admission]
    run(command,out/'engine.log')


def palette_rom(path, palettes):
    # Reassemble the existing snapshot ROM with two gameplay-only palettes.
    # Face palette 1 and keys/arms palette 3 are preserved.
    from build_sprite_probe import SOURCE, ANCHOR, ROOT as REPO, table
    source=(path/'probe.asm').read_text()
    values=b''.join(bytes((15,*row)) for row in palettes)
    setup=''
    for p in (0,2):
        setup+=upload(f'world_palette_{p}',values[p*4+1:p*4+4],0x3f11+4*p)
    source=source.replace(ANCHOR,setup+ANCHOR)
    source=source.replace('        .bank 3\n        org $ED00',
        ''.join(table(f'world_palette_{p}',values[p*4+1:p*4+4]) for p in (0,2))+
        '        .bank 3\n        org $ED00')
    source=source.replace('20DOOM-04-9021','20DOOM-04-9023')
    (path/'probe.asm').write_text(source)
    tmp=SOURCE.parent/'PG_world_scene_tmp.asm'
    try:
        tmp.write_text(source)
        with (path/'assembly.log').open('w') as f:
            subprocess.run([str(REPO/'.build/tools/nesasm/nesasm'),'-s','-f','-o',
                str(path/'probe.raw.nes'),tmp.name],cwd=SOURCE.parent,check=True,stdout=f,stderr=subprocess.STDOUT)
    finally:
        tmp.unlink(missing_ok=True)
    run([sys.executable,REPO/'doom/tools/nes/normalize_ines.py',path/'probe.raw.nes',path/'probe.nes'],path/'normalize.log')
    run([sys.executable,REPO/'doom/tools/nes/check_fixbank.py',path/'probe.nes',
         REPO/'tutorial_project/BOOTROM/bootrom_fixr.bin'],path/'fixbank.log')


def decoded(encoded):
    mailbox=encoded[protocol.VRAM_MAILBOX_OFF_V4:protocol.VRAM_MAILBOX_OFF_V4+protocol.FC_COM_BUF_SIZE_V4]
    return decode_to_rgb(stream.expand_native_text_stream(encoded),
        mailbox[protocol.MBX_ATTR:protocol.MBX_ATTR+64],mailbox[protocol.MBX_PAL:protocol.MBX_PAL+16], NES)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine',required=True,type=Path)
    parser.add_argument('--wad',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--frames',type=int,default=650)
    parser.add_argument('--demo',type=int,default=1,choices=(1,2,3))
    parser.add_argument('--reuse-capture',action='store_true')
    args=parser.parse_args()
    args.engine=args.engine.resolve()
    out=args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    if not args.reuse_capture:
        capture(args,out/'original')
    original=out/'original'
    pal=np.frombuffer(read_wad(args.wad.read_bytes())['PLAYPAL'][:768],np.uint8).reshape(256,3)
    samples=[]
    for frame in range(0,args.frames,5):
        records,_,_=load_frame(original/'base',original/'raw',frame,pal)
        for actor in records:
            if actor['eligible'] and 8<=actor['native_pixels']<=1800:
                for x,y,mask,rgb in tiles(actor):
                    samples.append(rgb[mask])
    palettes,study=palette_study(samples)
    (out/'palette-study.json').write_text(json.dumps(study,indent=2)+'\n')
    print('selected palettes',palettes.tolist(),'error',study['selected_error'],flush=True)
    plans={}; admission=[]
    for frame in range(args.frames):
        records,owners,weapon=load_frame(original/'base',original/'raw',frame,pal)
        art,oam,rgb,report=compose(records,palettes)
        admitted=[a['id'] for a in report['actors'] if a['admitted']]
        admission += [(frame,actor) for actor in sorted(admitted)]
        plans[frame]=report
    selection=out/'admission.csv'
    selection.write_text(''.join(f'{f},{a}\n' for f,a in admission))
    (out/'plans.json').write_text(json.dumps(plans,indent=2)+'\n')
    capture(args,out/'mixed',selection)
    print('replay complete; verifying geometry and held-weapon exclusion',flush=True)
    previews=[]; candidates=[]
    for frame in range(args.frames):
        stem=f'frame{frame:06d}'
        records,owners,weapon=load_frame(original/'base',original/'raw',frame,pal)
        art,oam,rgb,report=compose(records,palettes)
        admitted=[a['id'] for a in report['actors'] if a['admitted']]
        raw_owner=np.fromfile(original/'base'/f'{stem}.owners',np.uint8).reshape(200,320)
        before=np.fromfile(original/'raw'/f'{stem}.raw',np.uint8).reshape(200,320)
        after=np.fromfile(out/'mixed/raw'/f'{stem}.raw',np.uint8).reshape(200,320)
        native_mask=np.isin(owners,admitted)
        native_mask[:8]=False; native_mask[192:]=False
        if np.any(native_mask & weapon):
            raise AssertionError(f'frame {frame}: world overlaps weapon')
        if np.any((before!=after) & ~np.isin(raw_owner,admitted)):
            raise AssertionError(f'frame {frame}: replay changes pixels outside admitted actors')
        import csv
        with (out/'mixed/base'/f'{stem}.actors.csv').open() as f:
            replay=list(csv.DictReader(f))
        if len(records)!=len(replay):
            raise AssertionError('replay actor list diverged')
        for a,b in zip(records,replay):
            if any(a[key]!=int(b[key]) for key in ('id','sprite','frame','patch','flip','scale','x','y','z')):
                raise AssertionError(f'frame {frame}: replay state diverged')
        encoded=(out/'mixed/stream'/f'{stem}.bin').read_bytes()
        image=decoded(encoded)
        image[native_mask]=rgb[native_mask]
        if frame%2==0 and frame>80:
            previews.append(Image.fromarray(image))
        if admitted:
            visible=[a for a in report['actors'] if a['admitted']]
            clipping=sum(a['geometry_hidden_pixels'] * min(a['native_pixels']/150,1) for a in visible)
            weapon_overlap=sum(a['weapon_overlap'] for a in visible)
            pickups=sum(a['native_pixels'] for a in visible if a['flags']&1)
            enemies=sum(a['native_pixels'] for a in visible if a['flags']&0x400000)
            candidates.append((frame,clipping,weapon_overlap,pickups,enemies))
    # Select strong real evidence, plus adjacent frames to show motion.
    chosen={}
    for name,index in [('wall',1),('reload',2),('pickup',3),('enemy',4)]:
        ranked=sorted(candidates,key=lambda row:row[index],reverse=True)
        if not ranked or ranked[0][index]==0:
            print(f'no qualifying {name} frame',flush=True); continue
        chosen[name]=ranked[0][0]
    (out/'selected.json').write_text(json.dumps(chosen,indent=2)+'\n')
    if previews:
        previews[0].save(out/'gameplay-preview.gif',save_all=True,append_images=previews[1:],
            duration=100,loop=0,optimize=False)
    captures=[]
    for label,frame in chosen.items():
        for offset in (-1,0,1) if label in ('wall','reload') else (0,):
            number=max(0,frame+offset); stem=f'frame{number:06d}'
            records,owners,weapon=load_frame(original/'base',original/'raw',number,pal)
            for mode in ('shared','actor','tile') if offset==0 else ('tile',):
                used_palettes=SHARED if mode=='shared' else palettes
                art,oam,rgb,report=compose(records,used_palettes,mode)
                if [a['id'] for a in report['actors'] if a['admitted']] != [a['id'] for a in plans[number]['actors'] if a['admitted']]:
                    raise AssertionError('palette strategy changed admission; requires own replay')
                directory=out/'samples'/f'{label}-{number:06d}-{mode}'
                directory.mkdir(parents=True,exist_ok=True)
                encoded=(out/'mixed/stream'/f'{stem}.bin').read_bytes()
                (directory/'frame.bin').write_bytes(encoded)
                make_rom(directory,art,oam,[],'rp-lut')
                palette_rom(directory,used_palettes)
                env=dict(os.environ,FCPICO_WORLD_BASE_LUA=str(HERE/'lua/D0.lua'))
                with (directory/'cosim.log').open('w') as f:
                    subprocess.run([sys.executable,str(HERE/'run_doom_frame.py'),str(directory/'frame.bin'),
                        '--rom',str(directory/'probe.nes'),'--output',str(directory/'mesen'),
                        '--min-correlation','0.65','--lua-script',str(HERE/'lua/world_sprites.lua')],
                        check=True,stdout=f,stderr=subprocess.STDOUT,env=env)
                dumps=directory/'mesen/run'
                actual_oam=(dumps/'oam.bin').read_bytes()
                actual_chr=(dumps/'sprite_chr.bin').read_bytes()
                assert actual_oam[140:]==oam
                assert actual_chr[0x380:0x380+len(art)]==art
                for y in range(240):
                    assert sum(actual_oam[n*4]+1<=y<actual_oam[n*4]+9 for n in range(64))<=8
                mailbox=encoded[protocol.VRAM_MAILBOX_OFF_V4:]
                reference=decode_with_sprites(stream.expand_native_text_stream(encoded),actual_oam,actual_chr,
                    (dumps/'sprite_pal.bin').read_bytes(),mailbox[protocol.MBX_ATTR:protocol.MBX_ATTR+64],
                    mailbox[protocol.MBX_PAL:protocol.MBX_PAL+16], NES)
                actual=np.asarray(Image.open(dumps/'final.png').convert('RGB'))
                if not np.array_equal(actual[:192],reference[:192]):
                    raise AssertionError(f'calibrated PPU RGB mismatch: {np.count_nonzero(np.any(actual[:192]!=reference[:192],axis=2))}')
                Image.fromarray(reference).save(directory/'reference.png')
                baseline=out/'baselines'/stem
                if not (baseline/'mesen/run/final.png').exists():
                    baseline.mkdir(parents=True,exist_ok=True)
                    (baseline/'frame.bin').write_bytes((original/'stream'/f'{stem}.bin').read_bytes())
                    make_rom(baseline,art,bytes((0xef,0,0,0))*29,[],'rp-lut')
                    run([sys.executable,HERE/'run_doom_frame.py',baseline/'frame.bin',
                        '--rom',baseline/'probe.nes','--output',baseline/'mesen'],baseline/'cosim.log')
                Image.open(baseline/'mesen/run/final.png').save(directory/'streamed-baseline.png')
                Image.fromarray((weapon*255).astype(np.uint8)).save(directory/'weapon-mask.png')
                native_mask=np.isin(owners,[a['id'] for a in report['actors'] if a['admitted']]); native_mask[192:]=False
                Image.fromarray((native_mask*255).astype(np.uint8)).save(directory/'world-mask.png')
                report.update(frame=number,label=label,palette_mode=mode,palettes=used_palettes.tolist(),
                              screenshot_sha256=hashlib.sha256((dumps/'final.png').read_bytes()).hexdigest())
                (directory/'report.json').write_text(json.dumps(report,indent=2)+'\n')
                captures.append((label,number,mode,directory))
                print(label,number,mode,'oam',report['oam'],'CHR',report['chr_bytes'],flush=True)
    rows=[]
    for label,frame in chosen.items():
        images=[]
        for mode in ('shared','actor','tile'):
            path=out/'samples'/f'{label}-{frame:06d}-{mode}'
            if not images:
                images.append(('streamed',Image.open(path/'streamed-baseline.png').convert('RGB')))
            images.append((mode,Image.open(path/'mesen/run/final.png').convert('RGB')))
        rows.append((label,frame,images))
    sheet=Image.new('RGB',(4*512,len(rows)*504),'#181818'); draw=ImageDraw.Draw(sheet)
    for row,(label,frame,images) in enumerate(rows):
        for col,(mode,image) in enumerate(images):
            draw.text((col*512+6,row*504+4),f'{label} tic frame {frame}: {mode}',fill='white')
            sheet.paste(image.resize((512,480),Image.Resampling.NEAREST),(col*512,row*504+24))
    sheet.save(out/'comparison.png')
    html = ['<!doctype html><meta charset="utf-8"><title>World sprites in Doom</title>',
        '<style>body{background:#181818;color:#eee;font:16px system-ui;margin:24px}img{image-rendering:pixelated;max-width:100%}a{color:#9df}.row{display:flex;gap:16px;flex-wrap:wrap}figure{margin:8px}figcaption{max-width:512px}</style>',
        '<h1>World sprites in real Doom scenes</h1>',
        '<p>Engine positions, WHX animation frames and resolved wall/weapon visibility. '
        'Still images below are Mesen captures. The animation is the independent host composition preview; '
        'runtime cartridge transport is not implemented.</p>',
        '<p><a href="palette-study.json">Palette measurements</a> · <a href="verification.json">Verification</a></p>',
        '<h2>Gameplay preview</h2><img width="768" src="gameplay-preview.gif">']
    for label,frame,images in rows:
        html += [f'<h2>{label}: frame {frame}</h2><div class="row">']
        for mode,image in images:
            path=f'samples/{label}-{frame:06d}-tile/streamed-baseline.png' if mode=='streamed' else f'samples/{label}-{frame:06d}-{mode}/mesen/run/final.png'
            html += [f'<figure><img width="512" src="{path}"><figcaption>{mode}</figcaption></figure>']
        html += ['</div>']
        directory=out/'samples'/f'{label}-{frame:06d}-tile'
        report=json.loads((directory/'report.json').read_text())
        records,owners,weapon=load_frame(original/'base',original/'raw',frame,pal)
        mask=np.isin(owners,[a['id'] for a in report['actors'] if a['admitted']]); mask[192:]=False
        annotated=Image.open(directory/'mesen/run/final.png').convert('RGB')
        draw=ImageDraw.Draw(annotated)
        for actor in records:
            if actor['id'] not in [a['id'] for a in report['actors'] if a['admitted']]: continue
            yy,xx=np.nonzero(actor['mask'])
            if len(xx): draw.rectangle((int(xx.min()),int(yy.min()),int(xx.max()),int(yy.max())),outline='#00ffff')
        annotated.save(directory/'annotated.png')
        html += [f'<div class="row"><figure><img width="768" src="samples/{label}-{frame:06d}-tile/annotated.png"><figcaption>Cyan boxes identify admitted actors. All visible actor pixels follow the engine mask.</figcaption></figure>',
            f'<figure><img width="512" src="samples/{label}-{frame:06d}-tile/weapon-mask.png"><figcaption>Weapon and muzzle-flash silhouette. Native actor pixels never overlap this mask.</figcaption></figure></div>']
    (out/'review.html').write_text('\n'.join(html))

    (out/'verification.json').write_text(json.dumps({'engine_frames':args.frames,
        'replay_outside_actor_pixel_mismatches':0,'native_weapon_overlap_pixels':0,
        'mesen_samples':len(captures),'selected':chosen,'palette_study':study},indent=2)+'\n')
    print(out/'comparison.png')


if __name__=='__main__':
    main()
