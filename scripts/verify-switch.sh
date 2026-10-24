#!/usr/bin/env bash
set -euo pipefail
verify_root="${PAPERNX_VERIFY_ROOT:-verification}"
build_root="${PAPERNX_BUILD_ROOT:-build-switch}"
mkdir -p "$verify_root" "$build_root"
exec > >(tee "$verify_root/switch.log") 2>&1
export PATH="${DEVKITPRO}/portlibs/switch/bin:${PATH}"
BUILD_DIR="${PAPERNX_MUPDF_BUILD_ROOT:-build-mupdf-switch}" \
  bash scripts/build-mupdf-switch.sh "${PAPERNX_MUPDF_PREFIX:-/work/prefix-mupdf-switch}"
export MUPDF_PREFIX="${PAPERNX_MUPDF_PREFIX:-/work/prefix-mupdf-switch}"
version="${PAPERNX_RELEASE_VERSION:-v0.9.1}"
release_dir="${PAPERNX_RELEASE_DIST:-dist/${version}}"
receipt="$verify_root/switch-build-receipt.json"
target="$build_root/PaperNX"
python3 scripts/build_receipt.py start --platform switch --receipt "$receipt"
make all -B -j4 TARGET="$target" BUILD="$build_root/objects"
python3 scripts/build_receipt.py finish --platform switch --elf "$target.elf" --nro "$target.nro" --receipt "$receipt"
"${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-readelf" -h "$target.elf"
dkp-pacman -Q > "$verify_root/switch-packages.txt"
python3 scripts/verify_artifacts.py --version "$version" --elf "$target.elf" --nro "$target.nro" --output "$verify_root/artifacts.json"
if [[ "${PAPERNX_BUILD_ONLY:-0}" == "1" ]]; then
  echo "Build-only verification complete; release packaging was not requested."
  exit 0
fi
mkdir -p third-party-licenses
cp -r "$MUPDF_PREFIX/licenses/." third-party-licenses/
mkdir -p third-party-licenses/Switch-portlibs
cp -r "${DEVKITPRO}/portlibs/switch/licenses/." third-party-licenses/Switch-portlibs/
mkdir -p third-party-licenses/SDK-notices
cp scripts/license-notices/* third-party-licenses/SDK-notices/
mkdir -p third-party-licenses/unarr
cp third_party/unarr/COPYING third_party/unarr/COPYING.GPLv3 third_party/unarr/AUTHORS third_party/unarr/SOURCE.md third-party-licenses/unarr/
python3 scripts/package_release.py "$target.nro" --destination "$release_dir" --version "$version" --build-receipt "$receipt"
python3 scripts/verify_distribution.py --dist "$release_dir" --version "$version"
