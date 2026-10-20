#!/usr/bin/env bash
set -euo pipefail
mkdir -p verification
exec > >(tee verification/switch.log) 2>&1
export PATH="${DEVKITPRO}/portlibs/switch/bin:${PATH}"
bash scripts/build-mupdf-switch.sh /work/prefix-mupdf-switch
export MUPDF_PREFIX=/work/prefix-mupdf-switch
version="${PAPERNX_RELEASE_VERSION:-v0.9.1}"
release_dir="${PAPERNX_RELEASE_DIST:-dist/${version}}"
python3 scripts/build_receipt.py start
make all -B -j4
python3 scripts/build_receipt.py finish
"${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-readelf" -h PaperNX.elf
mkdir -p third-party-licenses
cp -r prefix-mupdf-switch/licenses/. third-party-licenses/
mkdir -p third-party-licenses/Switch-portlibs
cp -r "${DEVKITPRO}/portlibs/switch/licenses/." third-party-licenses/Switch-portlibs/
mkdir -p third-party-licenses/SDK-notices
cp scripts/license-notices/* third-party-licenses/SDK-notices/
mkdir -p third-party-licenses/unarr
cp third_party/unarr/COPYING third_party/unarr/COPYING.GPLv3 third_party/unarr/AUTHORS third_party/unarr/SOURCE.md third-party-licenses/unarr/
dkp-pacman -Q > verification/switch-packages.txt
python3 scripts/verify_artifacts.py --version "$version"
python3 scripts/package_release.py PaperNX.nro --destination "$release_dir" --version "$version" --build-receipt verification/v04-switch-build-receipt.json
python3 scripts/verify_distribution.py --dist "$release_dir" --version "$version"
