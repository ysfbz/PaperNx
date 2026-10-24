#!/usr/bin/env bash
set -euo pipefail
verify_root="${PAPERNX_VERIFY_ROOT:-verification}"
build_root="${PAPERNX_BUILD_ROOT:-build}"
mkdir -p "$verify_root" "$build_root"
PAPERNX_TEST_EVIDENCE_ROOT="$(cd "$verify_root" && pwd)/test-output"
export PAPERNX_TEST_EVIDENCE_ROOT
mkdir -p "$PAPERNX_TEST_EVIDENCE_ROOT"
exec > >(tee "$verify_root/desktop.log") 2>&1
core_build="$build_root/core"
desktop_build="$build_root/desktop"
cmake -S . -B "$core_build" -G Ninja -DBUILD_TESTING=ON
cmake --build "$core_build" --parallel 4
ctest --test-dir "$core_build" --output-on-failure
python3 tests/test_package.py
python3 tests/generate_fixtures.py
python3 tests/generate_comic_fixtures.py
python3 scripts/build_receipt.py start --platform desktop --receipt "$verify_root/desktop-build-receipt.json"
cmake -S . -B "$desktop_build" -G Ninja -DPAPERNX_NATIVE=ON -DBUILD_TESTING=ON
cmake --build "$desktop_build" --parallel 4
python3 scripts/build_receipt.py finish --platform desktop --binary "$desktop_build/PaperNX" --receipt "$verify_root/desktop-build-receipt.json"
ctest --test-dir "$desktop_build" --output-on-failure
