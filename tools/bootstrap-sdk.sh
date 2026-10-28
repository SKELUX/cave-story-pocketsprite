#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p toolchain artifacts
if [[ ! -x toolchain/python2/bin/python2.7 ]]; then
    curl -fL --retry 2 https://www.python.org/ftp/python/2.7.18/Python-2.7.18.tgz -o toolchain/Python-2.7.18.tgz
    echo 'da3080e3b488f648a3d7a4560ddee895284c3380b11d6de75edb986526b9a814  toolchain/Python-2.7.18.tgz' | sha256sum -c -
    tar -xzf toolchain/Python-2.7.18.tgz -C toolchain
    (
        cd toolchain/Python-2.7.18
        ./configure --prefix="$(pwd)/../python2" --without-ensurepip
        make -j4 CFLAGS='-O2 -std=gnu99 -fwrapv -fno-strict-aliasing'
        make install
    ) > artifacts/python-build.log 2>&1
fi
if ! toolchain/python2/bin/python2.7 -c 'import serial' 2>/dev/null; then
    python3 -m pip download --no-deps --dest toolchain pyserial==3.5
    echo 'c4451db6ba391ca6ca299fb3ec7bae67a5c55dde170964c7a14ceefec02f2cf0  toolchain/pyserial-3.5-py2.py3-none-any.whl' | sha256sum -c -
    python3 - <<'PY'
import zipfile
zipfile.ZipFile('toolchain/pyserial-3.5-py2.py3-none-any.whl').extractall(
    'toolchain/python2/lib/python2.7/site-packages')
PY
fi
# The legacy ncurses probe uses implicit int; modern GCC treats that as an error.
make -C upstream/8bkc-sdk/esp-idf/tools/kconfig CC='gcc -std=gnu99 -Wno-error=implicit-int'
