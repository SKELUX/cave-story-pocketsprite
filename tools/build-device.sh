#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
exec 9>build/device-build.lock
if ! flock -n 9; then
    printf 'Another device build is running; waiting for it to finish...\n'
    flock 9
fi
source tools/sdk-env.sh
python3 tools/patch-sdk-panic.py
python3 tools/patch-sdk-appfs.py
python3 tools/check_engine.py
if [[ ! -f build/music/12000/cavestory.dat ]]; then
    python3 tools/build_music.py
fi
if [[ ! -f build/opus/.libs/libopus.a ]]; then
    bash tools/build-opus.sh
fi
python3 tools/generate_assets.py --output build/device-assets --music build/music/12000
# Migrate existing configs too: the old COMPILER_OPTIMIZATION name was ignored
# by this pinned SDK, leaving development builds at -Og.
python3 - <<'PYCONFIG'
from pathlib import Path
p=Path('device/sdkconfig')
if p.exists():
    lines=[line for line in p.read_text().splitlines() if not line.startswith(
        ('CONFIG_OPTIMIZATION_LEVEL_DEBUG=', 'CONFIG_OPTIMIZATION_LEVEL_RELEASE=',
         'CONFIG_NEWLIB_NANO_FORMAT='))]
    lines+=['CONFIG_OPTIMIZATION_LEVEL_DEBUG=', 'CONFIG_OPTIMIZATION_LEVEL_RELEASE=y',
            'CONFIG_NEWLIB_NANO_FORMAT=y']
    updated='\n'.join(lines)+'\n'
    if updated!=p.read_text(): p.write_text(updated)
PYCONFIG
make -C device defconfig
make -C device -j4 app
mkdir -p artifacts/device-build
cp build/device/cavestory.bin artifacts/device-build/cavestory.app.tmp
mv artifacts/device-build/cavestory.app.tmp artifacts/device-build/cavestory.app
cp build/music/12000/cavestory.dat artifacts/device-build/cavestory.dat.tmp
mv artifacts/device-build/cavestory.dat.tmp artifacts/device-build/cavestory.dat
sha256sum artifacts/device-build/cavestory.app artifacts/device-build/cavestory.dat
