# PaperNX v0.9.3 — Licenses and source attribution

PaperNX is developed by Yusuf Buzrul with AI assistance and distributed under the [GNU Affero General Public License v3.0](../LICENSE). The [v0.9.3 source tag](https://github.com/ysfbz/PaperNx/tree/v0.9.3) contains the corresponding application source and build instructions.

The release includes a separate `PaperNX-licenses-v0.9.3.zip` with the project license, source attribution, and collected dependency notices. The Switch install ZIP contains only the NRO, required fonts and licenses for those fonts; it does not include books, settings, progress, bookmarks, or Reading Journal data.

PaperNX includes adapted and incorporated work from the [rtk19/PaperNx fork](https://github.com/rtk19/PaperNx). The fork credits **Refael Bar (@rtk19)** for comic/archive reading, page-curl, preview, controller, series, scanner/index and related theme work. **Refael Avramashvili** is credited for updater and release JSON parser contributions. Some files were byte-identical at the historical import revision `a35aa7b`; later edits and refactors do not erase that source lineage. The current code remains fork-derived/adapted in those areas and is not described as clean-room work. See [SOURCE_ATTRIBUTION.md](SOURCE_ATTRIBUTION.md) for module-level notes.

Major bundled dependencies include MuPDF 1.23.11 (AGPL/COPYING and its third-party material), SDL2/SDL2_ttf, libnx, FreeType, HarfBuzz, libarchive/unarr (including its LGPL/GPL, LZMA SDK and 7-Zip notices), zlib, image libraries, and Noto fonts (OFL). The Mozilla CA bundle retains the license documented by its [curl CA extract source](https://curl.se/docs/caextract.html). Exact notices are supplied in the release license ZIP and the repository's `third-party-licenses/`, `third_party/`, and `fonts/OFL.txt` sources.

License summaries do not replace each component's license terms or any complete-corresponding-source obligations. Preserve the copyright, license, source and build notices when redistributing PaperNX or its dependencies.
