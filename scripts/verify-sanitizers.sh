#!/usr/bin/env bash
set -euo pipefail
verify_root="${PAPERNX_VERIFY_ROOT:-verification}"
build_root="${PAPERNX_BUILD_ROOT:-build-sanitized}"
mkdir -p "$verify_root" "$build_root"
PAPERNX_TEST_EVIDENCE_ROOT="$(cd "$verify_root" && pwd)/test-output"
export PAPERNX_TEST_EVIDENCE_ROOT
mkdir -p "$PAPERNX_TEST_EVIDENCE_ROOT"
exec > >(tee "$verify_root/sanitizers.log") 2>&1
python3 tests/generate_fixtures.py
python3 tests/generate_comic_fixtures.py
cmake -S . -B "$build_root" -G Ninja -DPAPERNX_NATIVE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build "$build_root" --parallel 4
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir "$build_root" --output-on-failure
