#!/usr/bin/env bash
set -euo pipefail
mkdir -p verification
exec > >(tee verification/sanitizers.log) 2>&1
python3 tests/generate_fixtures.py
python3 tests/generate_comic_fixtures.py
cmake -S . -B build-sanitized -G Ninja -DPAPERNX_NATIVE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitized --parallel 4
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitized --output-on-failure
