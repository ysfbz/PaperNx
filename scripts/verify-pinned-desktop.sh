#!/usr/bin/env bash
set -euo pipefail
mkdir -p verification build-mupdf-desktop prefix-mupdf-desktop/lib prefix-mupdf-desktop/include
exec > >(tee verification/pinned-desktop.log) 2>&1
root="$PWD"
archive="$root/build-mupdf-switch/mupdf-1.23.11-source.tar.gz"
if [ ! -f "$archive" ]; then
    archive="$root/build-mupdf-desktop/mupdf-1.23.11-source.tar.gz"
    curl --connect-timeout 20 --max-time 600 --retry 3 -fsSL https://mupdf.com/downloads/archive/mupdf-1.23.11-source.tar.gz -o "$archive.download"
    mv "$archive.download" "$archive"
fi
printf '%s  %s\n' 478f2a167feae2a291c8b8bc5205f2ce2f09f09b574a6eb0525bfad95a3cfe66 "$archive" | sha256sum -c -
if [ ! -d build-mupdf-desktop/mupdf-1.23.11-source ]; then
    tar -xzf "$archive" -C build-mupdf-desktop
fi
make -C build-mupdf-desktop/mupdf-1.23.11-source -j4 build=release OUT=build/papernx-system-fonts USE_SYSTEM_HARFBUZZ=yes USE_SYSTEM_FREETYPE=yes HAVE_X11=no HAVE_GLFW=no HAVE_GLUT=no HAVE_CURL=no USE_SYSTEM_LIBS=no libs
cp -r build-mupdf-desktop/mupdf-1.23.11-source/include/mupdf prefix-mupdf-desktop/include/
cp build-mupdf-desktop/mupdf-1.23.11-source/build/papernx-system-fonts/libmupdf{,-third}.a prefix-mupdf-desktop/lib/
python3 tests/generate_fixtures.py
python3 tests/generate_comic_fixtures.py
cmake -S . -B build-pinned -G Ninja -DPAPERNX_NATIVE=ON -DBUILD_TESTING=ON -DMUPDF_PREFIX="$root/prefix-mupdf-desktop"
cmake --build build-pinned --parallel 4
ctest --test-dir build-pinned --output-on-failure
