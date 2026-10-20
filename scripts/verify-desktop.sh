#!/usr/bin/env bash
set -euo pipefail
mkdir -p verification
exec > >(tee verification/desktop.log) 2>&1
cmake -S . -B build-core -G Ninja -DBUILD_TESTING=ON
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
python3 tests/test_package.py
python3 tests/generate_fixtures.py
python3 tests/generate_comic_fixtures.py
cmake -S . -B build-desktop -G Ninja -DPAPERNX_NATIVE=ON -DBUILD_TESTING=ON
cmake --build build-desktop --parallel 4
ctest --test-dir build-desktop --output-on-failure
