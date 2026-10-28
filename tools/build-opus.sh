#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/sdk-env.sh
if [[ ! -d upstream/opus-1.3.1 ]]; then
    curl -fL --retry 2 https://downloads.xiph.org/releases/opus/opus-1.3.1.tar.gz -o toolchain/opus-1.3.1.tar.gz
    echo '65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d  toolchain/opus-1.3.1.tar.gz' | sha256sum -c -
    tar -xzf toolchain/opus-1.3.1.tar.gz -C upstream
fi
mkdir -p build/opus
cd build/opus
# No executable is produced by this cross build. ESP-IDF supplies final startup,
# libc and linker scripts. Long calls are required for ESP32 ROM functions.
../../upstream/opus-1.3.1/configure --host=xtensa-esp32-elf \
    --disable-shared --enable-static --enable-fixed-point --disable-intrinsics \
    --disable-rtcd --disable-extra-programs --disable-doc \
    CFLAGS='-Os -mlongcalls -ffunction-sections -fdata-sections' \
    LDFLAGS='-nostdlib -Wl,-e,main'
make -j4
