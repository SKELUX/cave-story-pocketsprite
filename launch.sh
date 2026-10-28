#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "${1:-}" != --in-terminal ]]; then
    exec konsole --separate --workdir "$PWD" -e "$PWD/launch.sh" --in-terminal "$@"
fi
shift
if ! python3 tools/build_desktop.py; then
    printf '\nBuild failed. Press Enter to close this terminal.\n'
    read -r
    exit 1
fi
printf '\nStarting Cave Story native PC test. Controls are printed below.\n\n'
if ./build/cave-desktop "$@"; then
    exit 0
else
    status=$?
    printf '\nGame exited with status %s. Press Enter to close this terminal.\n' "$status"
    read -r
    exit "$status"
fi
