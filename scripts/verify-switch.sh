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
version="${PAPERNX_RELEASE_VERSION:-v0.9.3}"
release_dir="${PAPERNX_RELEASE_DIST:-dist/${version}}"
licenses_root="${PAPERNX_LICENSES_ROOT:-third-party-licenses}"
receipt="$verify_root/switch-build-receipt.json"
target="$build_root/PaperNX"
objects_root="${build_root//\//-}-objects"
python3 scripts/build_receipt.py start --platform switch --receipt "$receipt"
make all -B -j4 TARGET="$target" BUILD="$objects_root"
python3 scripts/build_receipt.py finish --platform switch --elf "$target.elf" --nro "$target.nro" --receipt "$receipt"
"${DEVKITPRO}/devkitA64/bin/aarch64-none-elf-readelf" -h "$target.elf"
dkp-pacman -Q > "$verify_root/switch-packages.txt"
python3 scripts/verify_artifacts.py --version "$version" --elf "$target.elf" --nro "$target.nro" --output "$verify_root/artifacts.json"
if [[ "${PAPERNX_BUILD_ONLY:-0}" == "1" ]]; then
  echo "Build-only verification complete; release packaging was not requested."
  exit 0
fi
mkdir -p "$licenses_root"
cp -r "$MUPDF_PREFIX/licenses/." "$licenses_root/"
mkdir -p "$licenses_root/Switch-portlibs"
cp -r "${DEVKITPRO}/portlibs/switch/licenses/." "$licenses_root/Switch-portlibs/"
mkdir -p "$licenses_root/SDK-notices"
cp scripts/license-notices/* "$licenses_root/SDK-notices/"
mkdir -p "$licenses_root/unarr"
cp third_party/unarr/COPYING third_party/unarr/COPYING.GPLv3 third_party/unarr/AUTHORS third_party/unarr/SOURCE.md "$licenses_root/unarr/"
cat > "$licenses_root/SOURCE.txt" <<EOF
PaperNX source for this release: https://github.com/ysfbz/PaperNx/tree/$version
Project license: GNU AGPL-3.0; see LICENSE and docs/LICENSES.md in this archive.
The source tag contains the corresponding application source and build instructions.
Third-party notices below retain their individual license terms and attributions.
EOF
python3 scripts/package_release.py "$target.nro" --destination "$release_dir" --version "$version" --build-receipt "$receipt" --licenses-root "$licenses_root"
python3 scripts/verify_distribution.py --dist "$release_dir" --version "$version" --licenses-root "$licenses_root" --nro "$target.nro" --elf "$target.elf" --evidence "$verify_root/package-integrity.json"
