#!/usr/bin/env python3
"""Build assets + gameplay into a relocatable ESP32 object; report missing platform APIs.

This is a link audit, NOT an executable firmware/application build.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def run(*args, **kwargs):
    return subprocess.run(list(map(str, args)), cwd=ROOT, check=True, **kwargs)


def main():
    run(sys.executable, 'tools/generate_assets.py')
    run(sys.executable, 'tools/check_engine.py')
    compiler = ROOT/'toolchain/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc'
    objects = []
    for source in ['build/generated/assets.c', 'platform/dialog.c', 'platform/credit_text.c', 'platform/uftc.c', 'platform/zx0.c', 'platform/comp.c', 'platform/video.c', 'platform/sram.c', 'platform/input.c']:
        obj = ROOT/'build/objects'/('native_' + Path(source).stem + '.o')
        run(compiler, '-std=gnu11', '-Os', '-Wall', '-Wextra', '-Wno-unused-function',
            '-Werror', '-Iengine', '-Iplatform', '-c', source, '-o', obj)
        objects.append(obj)
    sources = list((ROOT/'engine').glob('*.c')) + list((ROOT/'engine/ai').glob('*.c')) + list((ROOT/'engine/db').glob('*.c'))
    for source in sources:
        objects.append(ROOT/'build/objects'/(str(source.relative_to(ROOT)).replace('/', '_')+'.o'))
    binpath = compiler.parent
    run(binpath/'xtensa-esp32-elf-ld', '-r', *objects, '-o', 'build/cave-engine.o')
    result = run(binpath/'xtensa-esp32-elf-nm', '-u', 'build/cave-engine.o', capture_output=True, text=True)
    (ROOT/'artifacts/native-unresolved.txt').write_text(result.stdout)
    run(binpath/'xtensa-esp32-elf-size', 'build/cave-engine.o')
    print('Relocatable link passed. Remaining platform symbols: artifacts/native-unresolved.txt')
    print('No executable application or hardware behavior is verified by this check.')


if __name__ == '__main__':
    main()
