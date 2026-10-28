#!/usr/bin/env python3
"""Compile every gameplay/AI module with the pinned ESP32 compiler (no link yet)."""
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def main():
    generated = ROOT / 'build/generated'
    generated.mkdir(parents=True, exist_ok=True)
    lines = ['#ifndef CS_RESOURCES_H', '#define CS_RESOURCES_H', '#include "md/types.h"']
    for line in (ROOT / 'upstream/cave-story-md/res/resources.res').read_text().splitlines():
        fields = shlex.split(line, comments=True)
        if not fields:
            continue
        if fields[0] != 'SPRITE':
            raise ValueError(f'Unsupported resource declaration: {line}')
        lines.append('extern const SpriteDef ' + fields[1] + ';')
    lines.append('#endif')
    (generated / 'resources.h').write_text('\n'.join(lines) + '\n')
    tool = ROOT / 'build/hppgen'
    subprocess.run(['cc', 'upstream/cave-story-md/tools/hppgen.c', '-o', str(tool)], cwd=ROOT, check=True)
    ai = sorted((ROOT / 'engine/ai').glob('*.c'))
    subprocess.run([str(tool), str(generated / 'ai_gen.h'), *map(str, ai)], check=True)
    sources = sorted((ROOT / 'engine').glob('*.c')) + ai + sorted((ROOT / 'engine/db').glob('*.c'))
    output = ROOT / 'build/objects'
    output.mkdir(exist_ok=True)
    (ROOT / 'artifacts').mkdir(exist_ok=True)
    failed = []
    with (ROOT / 'artifacts/compile-audit.log').open('w') as log:
        for source in sources:
            obj = output / (str(source.relative_to(ROOT)).replace('/', '_') + '.o')
            cmd = [str(ROOT / 'toolchain/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc'),
                   '-std=gnu11', '-Os', '-fno-builtin', '-ffunction-sections', '-fdata-sections',
                   '-Iengine', '-Iplatform', '-Ibuild/generated', '-c', str(source), '-o', str(obj)]
            result = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=log)
            if result.returncode:
                failed.append(str(source.relative_to(ROOT)))
    print(f'ESP32 compilation: {len(sources) - len(failed)}/{len(sources)} gameplay modules passed.')
    print('This check does not link or verify runtime behavior. Log: artifacts/compile-audit.log')
    for name in failed:
        print('FAILED:', name)
    return bool(failed)


if __name__ == '__main__':
    sys.exit(main())
