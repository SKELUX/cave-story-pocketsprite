#!/usr/bin/env python3
"""Offline candidate soundtrack: full tracks, original loop sample positions.

Requires host libgme and libopus development packages. This conversion is not
linked into firmware. Tracks use peak-safe level normalization and music-oriented CELT encoding.
"""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parent.parent
def compact_track(data):
    """Deduplicate exact encoded packets; never decode or re-encode audio."""
    magic,rate,frame,count,size,total,loop,skip=struct.unpack('<4s7I',data[:32])
    assert magic==b'CSM1' and len(data)==32+count*size
    unique={};packets=[];indices=[]
    for start in range(32,len(data),size):
        packet=data[start:start+size]
        if packet not in unique:
            unique[packet]=len(packets);packets.append(packet)
        indices.append(unique[packet])
    if len(packets)>65536: return data
    packed=b'CSM2'+data[4:32]+struct.pack('<'+'H'*count,*indices)+b''.join(packets)
    return packed if len(packed)<len(data) else data

def write_bundle(output,tracks):
    with (output/'cavestory.dat').open('wb') as pack:
        pack.write(b'CSD1')
        for track in tracks:
            data=compact_track((output/(track['name']+'.csm')).read_bytes())
            track['offset']=pack.tell();track['bytes']=len(data)
            track['sha256']=hashlib.sha256(data).hexdigest()
            pack.write(data)
    (output/'manifest.json').write_text(json.dumps(tracks,indent=2)+'\n')

def main():
    bitrate=int(sys.argv[1]) if len(sys.argv)>1 else 12000
    output=ROOT/'build/music'/str(bitrate)
    output.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'build/render-music'
    subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror',
                    'tools/render_music.c','-lgme','-lopus','-lm','-o',str(exe)],cwd=ROOT,check=True)
    def convert(source):
        dest=output/(source.stem+'.csm')
        subprocess.run([str(exe),str(source),str(dest),str(bitrate)],check=True)
        data=dest.read_bytes()
        magic,rate,frame,count,size,total,loop,skip=struct.unpack('<4s7I',data[:32])
        assert magic==b'CSM1' and len(data)==32+count*size
        return dict(name=source.stem,rate=rate,frame=frame,packets=count,
                    packet_bytes=size,samples=total,loop=loop,skip=skip,bytes=len(data),
                    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    sha256=hashlib.sha256(data).hexdigest())
    sources=sorted((ROOT/'upstream/cave-story-md/res/bgm').glob('*.vgm'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        tracks=list(pool.map(convert,sources))
    write_bundle(output,tracks)
    print(f'{len(tracks)} complete tracks: {sum(t["bytes"] for t in tracks):,} bytes at {bitrate}bps')
if __name__=='__main__': main()
