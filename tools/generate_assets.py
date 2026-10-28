#!/usr/bin/env python3
"""Build upstream assets and express their resource graph as native C objects."""
from pathlib import Path
import hashlib
import json
import re
import shlex
import struct
import subprocess
import argparse

ROOT = Path(__file__).resolve().parent.parent
UP = ROOT / 'upstream/cave-story-md'
OUT = ROOT / 'build/generated'


def numbers(data, width, endian='little'):
    if len(data) % width:
        data += bytes(width - len(data) % width)
    return [int.from_bytes(data[i:i+width], endian) for i in range(0, len(data), width)]


def array(f, ctype, name, values, static=False):
    prefix = 'static ' if static else ''
    f.write(f'{prefix}const {ctype} {name}[] __attribute__((aligned(4))) = {{\n')
    for i in range(0, len(values), 16):
        f.write('  ' + ','.join(map(str, values[i:i+16])) + ',\n')
    f.write('};\n')


def pack_pcm(data):
    """Lossless signed PCM: 128-sample blocks with a minimum and bit width."""
    samples=[v if v<128 else v-256 for v in data]
    count=(len(data)+127)//128
    offsets=[];blocks=bytearray()
    for start in range(0,len(data),128):
        values=samples[start:start+128];minimum=min(values)
        bits=(max(values)-minimum).bit_length()
        offsets.append(count*2+len(blocks))
        blocks.extend((minimum&255,bits))
        word=0;used=0
        for value in values:
            word|=(value-minimum)<<used;used+=bits
            while used>=8:
                blocks.append(word&255);word>>=8;used-=8
        if used:blocks.append(word&255)
    if offsets and offsets[-1]>65535: return data
    result=struct.pack('<'+'H'*count,*offsets)+blocks
    return bytes(result) if len(result)<len(data) else data


def pack_uftc(data):
    """Keep the pixel dictionary verbatim; compact its even byte offsets."""
    size=int.from_bytes(data[:2],'big')
    assert size%2==0 and (len(data)-2-size)%8==0
    indices=[int.from_bytes(data[i:i+2],'big') for i in range(2+size,len(data),2)]
    assert all(v%2==0 and v+8<=size for v in indices)
    if not indices: return data
    bits=max(1,(max(indices)//2).bit_length())
    packed=bytearray(b'\xff\xff'+data[:2]+bytes([bits])+data[2:2+size])
    word=0;used=0
    for value in indices:
        word|=(value//2)<<used;used+=bits
        while used>=8:
            packed.append(word&255);word>>=8;used-=8
    if used: packed.append(word&255)
    packed.extend(b'\0\0')  # permits one fixed 24-bit read at the final index
    return bytes(packed) if len(packed)<len(data) else data


def sprites(f):
    asm = OUT / 'sprites.s'
    subprocess.run([str(UP/'bin/rescomp'), str(UP/'res/resources.res'), str(asm)],
                   stdout=subprocess.DEVNULL, check=True)
    blocks = {}
    current = None
    for line in asm.read_text().splitlines():
        line = line.strip()
        if line.endswith(':'):
            current = line[:-1]
            blocks[current] = []
        elif line.startswith(('dc.w', 'dc.l')):
            blocks[current].extend(x.strip() for x in line[4:].split(','))
    count = 0
    tile_data = {}
    part_data = {}
    aliases = {}
    metadata = {}
    def share(kind,name,values):
        key=(kind,tuple(aliases.get(v,v) for v in values))
        if key in metadata:
            aliases[name]=metadata[key]
            f.write(f'#define {name} {metadata[key]}\n')
            return True
        metadata[key]=name
        return False
    for name, vals in blocks.items():
        if name.endswith('_tiles'):
            # Packed pixels are bytes, not native uint16 values.
            raw = b''.join(struct.pack('>H', int(v, 0) & 65535) for v in vals)
            # Identical immutable frames (including blank frames) need only
            # one flash copy. Keep frame tables and animation timing intact.
            if raw in tile_data:
                aliases[name]=tile_data[raw]
                f.write(f'#define {name} {tile_data[raw]}\n')
            else:
                tile_data[raw] = name
                array(f, 'uint32_t', name, numbers(raw, 4), True)
        elif re.search(r'_sprite\d+$', name):
            key=tuple(int(v,0) for v in vals)
            if key in part_data:
                aliases[name]=part_data[key]
                f.write(f'#define {name} {part_data[key]}\n')
            else:
                part_data[key]=name
                f.write(f'static const VDPSpriteInf {name} = {{'+','.join(vals)+'};\n')
        elif name.endswith('_sprites'):
            if not share('parts',name,vals):
                array(f, 'VDPSpriteInf * const', name, ['&'+v for v in vals], True)
        elif re.search(r'_tileset\d+$', name):
            if not share('frame',name,vals):
                f.write(f'static const TileSet {name} = {{'+','.join(vals)+'};\n')
        elif name.endswith('_tilesets'):
            if not share('frames',name,vals):
                array(f, 'TileSet * const', name, ['&'+v for v in vals], True)
        else:
            assert len(vals) == 6, (name, vals)
            f.write(f'const SpriteDef {name} = {{'+','.join(vals)+'};\n')
            count += 1
    return count


def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--music',type=Path,help='Offline CSM1 music directory for device assets')
    args=parser.parse_args()
    music={t['name']:t for t in json.loads((args.music/'manifest.json').read_text())} if args.music else {}
    OUT=args.output.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    (ROOT/'artifacts').mkdir(exist_ok=True)
    with (ROOT/'artifacts/assets-build.log').open('w') as log:
        subprocess.run(['make','-C',str(UP),'-j4','assets.d','bin/rescomp','bin/salvador'],
                       stdout=log, stderr=log, check=True)
    ctypes = {}
    for path in (ROOT/'engine/res').glob('*.h'):
        for kind, name in re.findall(r'extern const (uint\d+_t)\s+(\w+)\[[^\]]*\]', path.read_text()):
            ctypes[name] = kind
    blobs, pointers, tables = [], [], {}
    for path in sorted((ROOT/'engine/res').glob('*.s')):
        table = None
        for line in path.read_text().splitlines():
            fields = shlex.split(line.replace(',', ' '), comments=True)
            if not fields:
                continue
            if fields[0] in ('BIN','PCM','SPRITE'):
                blobs.append(fields)
            elif fields[0] == 'PTR':
                pointers.append(fields[1:3])
            elif fields[0] in ('TSC_GLOB:', 'TSC_STAGE:'):
                table = fields[0][:-1]
                tables[table] = []
            elif fields[0].endswith(':'):
                table = None
            elif table and fields[0] == 'dc.l':
                tables[table].append(fields[1])
    manifest = []
    with (OUT/'assets.c').open('w') as f:
        f.write('/* Generated by tools/generate_assets.py. Do not edit. */\n')
        f.write('#include "md/types.h"\n#include "assets.h"\n')
        sprite_count = sprites(f)
        for fields in blobs:
            op, name, rel = fields[:3]
            path = UP/'build'/rel
            if not path.exists():
                path = UP/rel
            data = path.read_bytes()
            if args.music and name.startswith('BGM_'):
                track=music[Path(rel).stem]
                data=struct.pack('<4sIII',b'CSE1',track['offset'],track['bytes'],0)
            expanded = 0
            if name.startswith('UFTC_'):
                data=pack_uftc(data)
            if op == 'PCM':
                packed=pack_pcm(data)
                if len(packed)<len(data): expanded=len(data);data=packed
            elif name.startswith('PAT_Ill'):
                expanded=len(data)
                packed=OUT/(name+'.zx0')
                if not packed.exists() or packed.stat().st_mtime<path.stat().st_mtime:
                    subprocess.run([str(UP/'bin/salvador'),'-c',str(path),str(packed)],
                                   stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,check=True)
                data=packed.read_bytes()
            elif name.startswith('PXM_'):
                expanded = len((UP/rel.replace('.cpxm','.pxm')).read_bytes()) - 4
            elif name.startswith('PXA_'):
                expanded = len((UP/rel.replace('.cpxa','.pxa')).read_bytes())
            if op == 'SPRITE':
                w, h, frames = map(int, fields[3:6])
                assert len(data) == w*h*frames*32, name
                f.write(f'const struct {{ uint16_t width,height,frames,fsize; uint32_t tiles[{len(data)//4}]; }} {name} = {{')
                f.write(f'{w},{h},{frames},{w*h},{{'+','.join(map(str,numbers(data,4)))+'}};\n')
            else:
                kind = ctypes.get(name,'uint8_t')
                width = int(kind[4:-2]) // 8
                endian = 'big' if name.startswith(('PAL_', 'MAP_', 'PXE_')) else 'little'
                if name.startswith('TSC_'):
                    # TSB count stays big-endian; Event structs become native LE.
                    # Command operands are already LE and MUST NOT be swapped.
                    count = int.from_bytes(data[:2],'big')
                    stop = 2 + count*4
                    assert stop <= len(data), name
                    data = data[:2] + b''.join(data[i:i+2][::-1] for i in range(2,stop,2)) + data[stop:]
                array(f, kind, name, numbers(data,width,endian))
                if op == 'PCM':
                    f.write(f'__asm__(".global {name}_end\\n.set {name}_end, {name}+{len(data)}");\n')
            manifest.append(dict(name=name, path=rel, size=len(data), expanded=expanded,
                                 sha256=hashlib.sha256(data).hexdigest()))
        f.write('const uint8_t LANGUAGE[4] = {\'E\',\'N\',0,0};\n')
        for name, dest in pointers:
            if name.startswith('SPR_'):
                f.write(f'const LocSprite * const {name} = (const LocSprite *)&{dest};\n')
            elif name.startswith('TS_'):
                f.write(f'const uint16_t * const {name}[] = {{(const uint16_t *){dest}}};\n')
            else:
                f.write(f'const uint8_t * const {name} = (const uint8_t *){dest};\n')
        for name, vals in tables.items():
            array(f, 'uint8_t * const', name, vals)
        f.write('const CsAsset cs_assets[] = {\n')
        for item in manifest:
            if not (item['expanded'] or item['name'].startswith('BGM_')): continue
            f.write('{&'+item['name']+f', {item["size"]}, {item["expanded"]}'+'},\n')
        f.write('};\nconst size_t cs_asset_count = sizeof(cs_assets)/sizeof(cs_assets[0]);\n')
    (OUT/'assets.json').write_text(json.dumps(dict(sprites=sprite_count, blobs=manifest),indent=2)+'\n')
    print(f'Native assets: {sprite_count} sprite definitions, {len(manifest)} blobs; {sum(x["size"] for x in manifest):,} blob bytes.')


if __name__ == '__main__':
    main()
