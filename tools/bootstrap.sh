#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p upstream toolchain
clone_pinned() {
    local url="$1" directory="$2" revision="$3"
    # Source-only handoff archives carry pinned assets/tools without Git history.
    if [[ -f "$directory/.source-revision" ]] && [[ "$(<"$directory/.source-revision")" == "$revision" ]]; then
        return
    fi
    if [[ ! -d "$directory/.git" ]]; then
        git clone "$url" "$directory"
    fi
    if [[ "$(git -C "$directory" rev-parse HEAD)" != "$revision" ]]; then
        git -C "$directory" checkout "$revision"
    fi
}
clone_pinned https://github.com/andwn/cave-story-md.git upstream/cave-story-md 9c27c6607a1c26edc7ff6e980925b5a294433e4a
clone_pinned https://github.com/PocketSprite/8bkc-sdk.git upstream/8bkc-sdk 4abf325e539b878c0d53c40f93667962056b2d34
git -C upstream/8bkc-sdk submodule update --init --recursive --depth 1
if [[ ! -x toolchain/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc ]]; then
    curl -fL --retry 2 https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz -o toolchain/xtensa.tar.gz
    echo '3fe96c151d46c1d4e5edc6ed690851b8e53634041114bad04729bc16b0445156  toolchain/xtensa.tar.gz' | sha256sum -c -
    tar -xzf toolchain/xtensa.tar.gz -C toolchain
fi
toolchain/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc --version
