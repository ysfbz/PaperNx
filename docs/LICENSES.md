# PaperNX v0.9.1 — License and corresponding source

PaperNX is [AGPL-3.0](../LICENSE) software developed by Yusuf Buzrul with AI assistance.

The [GitHub v0.9.1 Release](https://github.com/ysfbz/PaperNx/releases/tag/v0.9.1) separates user-installable files and license notices:

- `PaperNX-switch-v0.9.1.zip`: microSD NRO, font copies/OFL and an empty books directory. It **does not** install `switch/PaperNX/licenses/`.
- `PaperNX-licenses-v0.9.1.zip`: the project `LICENSE` plus collected third-party notices under `third-party-licenses/`.
- [Tagged source code](https://github.com/ysfbz/PaperNx/tree/v0.9.1) and build scripts: the corresponding PaperNX implementation.

Major dependencies: [MuPDF 1.23.11](https://github.com/ArtifexSoftware/mupdf/tree/1.23.11) (AGPL/COPYING and third-party material), [unarr](https://github.com/selmf/unarr) (LGPL-3.0 / GPL-3.0, LZMA SDK, 7-Zip), libnx, SDL2/SDL2_ttf, FreeType, HarfBuzz, JPEG/PNG/zlib/libarchive/libwebp-related libraries and [Noto OFL fonts](../fonts/OFL.txt). The exact SDK dependency version depends on the build environment; the Dockerfile pins the base image digest but installs updates from devkitPro repositories.

`scripts/build-mupdf-switch.sh` verifies the MuPDF source tarball checksum; `scripts/collect_licenses.py` collects notices and `SOURCE.txt`; `scripts/verify-switch.sh` adds SDK notice files; `scripts/package_release.py` produces the **separate** license ZIP.

AGPL/third-party redistribution may require complete corresponding source and appropriate build/install information. A notices archive alone does not replace those obligations. See [build guide](BUILD_SWITCH.md) and [current Release Report](RELEASE_REPORT.md).

## Source provenance and community contributions

The upstream PaperNX application is maintained by Yusuf Buzrul. The independently maintained [rtk19/PaperNx fork](https://github.com/rtk19/PaperNx), credited by that project's README to **Refael Bar (@rtk19)**, introduced comic archive, page-curl, preview, controller and series work. This is **not solely conceptual inspiration**: source comparison for the v0.9.1 code identifies **byte-identical Git blobs** shared with that fork, including `src/comic_document.c`, `include/papernx/page_curl.hpp`, `include/papernx/page_previews.hpp`, `src/controller_input.inc`, `src/series_library.inc`, and their associated tests. The integrated code should be treated as incorporated community code rather than represented as independently rewritten from ideas alone.

The fork [publishes its source under AGPL-3.0](https://github.com/rtk19/PaperNx/blob/main/LICENSE) and identifies the original PaperNX authorship. Preserve both the original project's attribution and the fork contributor's attribution, along with copyright headers and notices attached to any integrated files. The vendored RAR decoder has its own provenance and license: [`third_party/unarr/SOURCE.md`](../third_party/unarr/SOURCE.md), [`COPYING`](../third_party/unarr/COPYING), [`AUTHORS`](../third_party/unarr/AUTHORS). A compatible project-level license does not remove independent obligations to retain source, third-party notices or applicable license terms.

The published v0.9.1 tag and downloadable binaries are historical artifacts; documentation-only changes on `main` after the tag do not alter those tagged files or GitHub Release assets. [Release verification and checksums](RELEASE_REPORT.md) remain the reference for published artifacts.
