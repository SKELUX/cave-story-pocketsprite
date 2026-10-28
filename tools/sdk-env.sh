#!/usr/bin/env bash
# Source from any directory before invoking the legacy SDK makefiles.
CS_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export POCKETSPRITE_PATH="$CS_ROOT/upstream/8bkc-sdk"
export IDF_PATH="$POCKETSPRITE_PATH/esp-idf"
export PATH="$CS_ROOT/toolchain/python2/bin:$CS_ROOT/toolchain/xtensa-esp32-elf/bin:$PATH"
export PYTHON="$CS_ROOT/toolchain/python2/bin/python2.7"
