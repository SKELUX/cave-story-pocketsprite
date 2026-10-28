#!/usr/bin/env python3
"""Build the actual native engine with a no-audio host diagnostic adapter."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parent.parent
sources=sorted((ROOT/'engine').glob('*.c'))+sorted((ROOT/'engine/ai').glob('*.c'))+sorted((ROOT/'engine/db').glob('*.c'))
sources += [ROOT/'platform'/s for s in ['dialog.c','credit_text.c','video.c','uftc.c','zx0.c','comp.c','sram.c','input.c']]
sources += [ROOT/'build/generated/assets.c',ROOT/'host/headless.c',ROOT/'host/save_file.c']
from concurrent.futures import ThreadPoolExecutor
out=ROOT/'build/host';out.mkdir(exist_ok=True)
flags=['-std=gnu11','-O1','-g','-fcommon','-fno-builtin','-DNSOUND',
       '-fsanitize=address','-fno-omit-frame-pointer','-Wno-attributes',
       '-iquote','engine','-iquote','platform','-iquote','build/generated']
headers=list((ROOT/'engine').rglob('*.h'))+list((ROOT/'platform').glob('*.h'))+list((ROOT/'build/generated').glob('*.h'))
newest=max(p.stat().st_mtime for p in headers+[Path(__file__)])
def compile(source):
    obj=out/(str(source.relative_to(ROOT)).replace('/','_')+'.o')
    if not obj.exists() or obj.stat().st_mtime < max(newest,source.stat().st_mtime):
        subprocess.run(['cc',*flags,'-c',str(source),'-o',str(obj)],cwd=ROOT,check=True)
    return str(obj)
with ThreadPoolExecutor(max_workers=4) as pool:
    objects=list(pool.map(compile,sources))
subprocess.run(['cc','-fsanitize=address',*objects,'-o','build/cave-host'],cwd=ROOT,check=True)
print('Built build/cave-host (no-audio gameplay diagnostic, not the deliverable app).')
