# PaperNX v0.9.1 — License and corresponding source

PaperNX is [AGPL-3.0](../LICENSE) software developed by Yusuf Buzrul with AI assistance.

The [GitHub v0.9.1 Release](https://github.com/ysfbz/PaperNx/releases/tag/v0.9.1) separates user-installable files and license notices:

- `PaperNX-switch-v0.9.1.zip`: microSD NRO, font copies/OFL and an empty books directory. It **does not** install `switch/PaperNX/licenses/`.
- `PaperNX-licenses-v0.9.1.zip`: the project `LICENSE` plus collected third-party notices under `third-party-licenses/`.
- [Tagged source code](https://github.com/ysfbz/PaperNx/tree/v0.9.1) and build scripts: the corresponding PaperNX implementation.

Major dependencies: [MuPDF 1.23.11](https://github.com/ArtifexSoftware/mupdf/tree/1.23.11) (AGPL/COPYING and third-party material), libnx, SDL2/SDL2_ttf, FreeType, HarfBuzz, JPEG/PNG/zlib-related libraries and [Noto OFL fonts](../fonts/OFL.txt). The exact SDK dependency version depends on the build environment; the Dockerfile pins the base image digest but installs updates from devkitPro repositories.

`scripts/build-mupdf-switch.sh` verifies the MuPDF source tarball checksum; `scripts/collect_licenses.py` collects notices and `SOURCE.txt`; `scripts/verify-switch.sh` adds SDK notice files; `scripts/package_release.py` produces the **separate** license ZIP.

AGPL/third-party redistribution may require complete corresponding source and appropriate build/install information. A notices archive alone does not replace those obligations. See [build guide](BUILD_SWITCH.md) and [current Release Report](RELEASE_REPORT.md).
