#!/usr/bin/env python3
"""Incremental SDL desktop build with the native device audio/resource path."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess
import sys
import os
ROOT=Path(__file__).resolve().parent.parent
def run(*args): subprocess.run(list(map(str,args)),cwd=ROOT,check=True)
def main():
    if not (ROOT/'build/generated/ai_gen.h').exists(): run(sys.executable,'tools/check_engine.py')
    if not (ROOT/'build/music/12000/cavestory.dat').exists(): run(sys.executable,'tools/build_music.py')
    asset_inputs = [ROOT/'build/music/12000/manifest.json', ROOT/'tools/generate_assets.py']
    if not (ROOT/'build/device-assets/assets.c').exists() or (ROOT/'build/device-assets/assets.c').stat().st_mtime < max(p.stat().st_mtime for p in asset_inputs):
        run(sys.executable,'tools/generate_assets.py','--output','build/device-assets','--music','build/music/12000')
    flags=subprocess.check_output(['pkg-config','--cflags','sdl2','opus'],text=True).split()
    libs=subprocess.check_output(['pkg-config','--libs','sdl2','opus'],text=True).split()
    flags+=['-std=gnu11','-O2','-g','-fcommon','-fno-builtin','-Wno-attributes',
            '-iquote','engine','-iquote','platform','-iquote','build/generated']
    sources=sorted((ROOT/'engine').glob('*.c'))+sorted((ROOT/'engine/ai').glob('*.c'))+sorted((ROOT/'engine/db').glob('*.c'))
    sources += [ROOT/'platform'/s for s in ['dialog.c','credit_text.c','video.c','uftc.c','zx0.c','comp.c','sram.c','input.c','music.c','mixer.c','ui.c','viewport.c','scene_focus.c','presentation.c']]
    sources += [ROOT/'build/device-assets/assets.c',ROOT/'host/desktop.c',ROOT/'host/save_file.c']
    headers=list((ROOT/'engine').rglob('*.h'))+list((ROOT/'platform').glob('*.h'))+list((ROOT/'build/generated').glob('*.h'))
    newest=max(p.stat().st_mtime for p in headers+[Path(__file__)])
    sanitize=os.environ.get('CS_SANITIZE')=='1'
    if sanitize: flags+=['-O1','-fsanitize=address','-fno-omit-frame-pointer'];libs+=['-fsanitize=address']
    out=ROOT/('build/desktop-asan' if sanitize else 'build/desktop');out.mkdir(parents=True,exist_ok=True)
    def compile(source):
        obj=out/(str(source.relative_to(ROOT)).replace('/','_')+'.o')
        if not obj.exists() or obj.stat().st_mtime<max(newest,source.stat().st_mtime):
            run('cc',*flags,'-c',source,'-o',obj)
        return obj
    with ThreadPoolExecutor(max_workers=4) as pool: objects=list(pool.map(compile,sources))
    binary='build/cave-desktop-asan' if sanitize else 'build/cave-desktop'
    run('cc',*objects,*libs,'-pthread','-o',binary)
    print(f'Built {binary} (native SDL window, music, effects and separate PC saves).')
if __name__=='__main__': main()
