# Source attribution

PaperNX is maintained by Yusuf Buzrul and licensed under [AGPL-3.0](../LICENSE). This release includes existing community code and adaptations; changed file hashes or reorganized interfaces do not make that work independent or clean-room.

## PaperNX and rtk19/PaperNx

The [rtk19/PaperNx fork](https://github.com/rtk19/PaperNx) credits **Refael Bar (@rtk19)**. PaperNX v0.9.2 retains fork-derived/adapted work in comic archive/document handling, ComicInfo and series grouping, page-curl and page previews, controller integration, scanner/index behavior, theme and page-cache adaptations. The current scanner/index code includes PaperNX-specific safety and inventory changes.

The fork also credits **Refael Avramashvili** for updater and release JSON parser contributions. PaperNX retains that lineage in the bounded release parser and updater while using verified, two-slot manual staging. The updater does not replace the running NRO.

At feature import revision `a35aa7b0f51aee58f44d524616a2aa55477d670c`, six tracked paths matched the pinned fork snapshot `d2b4bcfd10b1a66043c5b8cd36da08340cffec6d` byte-for-byte. Those paths were `src/library_scan.inc`, `src/library_index.cpp`, `include/papernx/library_index.hpp`, `src/self_update.cpp`, `src/update_json.hpp` and `include/papernx/interface_theme.hpp`. Later revisions changed or removed those blobs; this is historical evidence of provenance, not a statement that current files are identical. Existing author notices and AGPL obligations remain.

## Third-party components

The exact license texts and notices are included in `PaperNX-licenses-v0.9.2.zip` and retained in the repository:

- **MuPDF 1.23.11:** AGPL/COPYING and upstream third-party notices under `third-party-licenses/MuPDF-1.23.11/`.
- **unarr RAR decoder:** source, local changes, authors and COPYING files under `third_party/unarr/` and `third-party-licenses/unarr/`; its components include LGPL/GPL, LZMA SDK and 7-Zip terms.
- **libnx and Switch portlibs**, including SDL2/SDL2_ttf, FreeType, HarfBuzz, image/archive/compression libraries: preserve their individual SDK/portlibs notices under `third-party-licenses/`.
- **Noto fonts:** OFL notice in `fonts/OFL.txt`, also embedded in RomFS and the Switch package.
- **Mozilla CA bundle:** provenance and license details in `scripts/license-notices/curl-ca-bundle.md`.

The separate release license archive contains `LICENSE`, this source attribution, `docs/LICENSES.md`, and the collected dependency notices. The corresponding application source and build/install instructions are published in the v0.9.2 source tag. These summaries do not replace the terms in the supplied license texts.
