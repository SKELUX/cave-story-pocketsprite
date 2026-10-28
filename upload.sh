#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "${1:-}" != --in-terminal ]]; then
    exec konsole --separate --workdir "$PWD" -e "$PWD/upload.sh" --in-terminal "$@"
fi
shift
status=0
mkdir -p artifacts
build_log=$(mktemp "$PWD/artifacts/upload-build.XXXXXX.log")
printf 'Building latest development version...\nBuild log: %s\n' "$build_log"
if bash tools/build-device.sh >"$build_log" 2>&1; then
    printf '\nBuild succeeded. Uploading to PocketSprite...\n'
    if python3 -u tools/device.py "$@" bundle artifacts/device-build; then
        printf '\nLatest development app and soundtrack uploaded successfully.\n'
    else
        status=$?
        printf '\nUpload failed (status %s). See the device error above.\n' "$status"
    fi
else
    status=$?
    tail -n 40 "$build_log"
    printf '\nBuild failed (status %s). Full log: %s\n' "$status" "$build_log"
fi
printf 'Press Enter to close this terminal.\n'
read -r || true
exit "$status"
