#!/usr/bin/env bash
# Existing local Docker images and cached MuPDF prefixes; no downloads/rebuilds.
set -euo pipefail
mode="${1:-native}"
mkdir -p verification
exec > >(tee "verification/v04-${mode}.log") 2>&1
if [[ "$mode" == switch ]]; then
    test -s prefix-mupdf-switch/lib/libmupdf.a
    export MUPDF_PREFIX="$PWD/prefix-mupdf-switch"
    export PATH="${DEVKITPRO}/portlibs/switch/bin:${PATH}"
    python3 scripts/build_receipt.py start
    # Recompile application objects and regenerate NACP/NRO; retain SDK/MuPDF caches.
    make all -B -j4
    python3 scripts/build_receipt.py finish
    python3 scripts/verify_artifacts.py --version "${PAPERNX_RELEASE_VERSION:-v0.9.2}"
    exit
fi
python3 tests/generate_fixtures.py
options=(-DBUILD_TESTING=ON)
case "$mode" in
    portable) options+=(-DPAPERNX_NATIVE=OFF -DCMAKE_BUILD_TYPE=Release);;
    native|release|sanitizers)
        test -s prefix-mupdf-desktop/lib/libmupdf.a
        options+=(-DPAPERNX_NATIVE=ON "-DMUPDF_PREFIX=$PWD/prefix-mupdf-desktop")
        if [[ "$mode" == release ]]; then options+=(-DCMAKE_BUILD_TYPE=Release); fi
        if [[ "$mode" == sanitizers ]]; then
            options+=(-DCMAKE_BUILD_TYPE=Debug '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined')
        fi;;
    *) echo "Expected portable/native/release/sanitizers/switch" >&2; exit 2;;
esac
cmake -S . -B "build-v04-$mode" -G Ninja "${options[@]}"
cmake --build "build-v04-$mode" --parallel 4
ASAN_OPTIONS=detect_leaks=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir "build-v04-$mode" -j1 --output-on-failure
