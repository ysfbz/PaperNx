# Changelog

All notable changes to **PaperNX** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] — post-v0.9.2 fixes (not yet built, tested or published)

### Reader and theme recovery
- Restored a unified default application palette that follows the global reading theme while keeping optional Dark/Light interface overrides and seven custom accent colors.
- Aligned the Turkish paper-preset labels with the actual ten preset identities, and restored v0.9.1-style controller selection/focus behaviour.
- Added a nonblocking comic-geometry lookup fed by independent background decoding; unknown dimensions are conservatively paired only after discovery.
- Primed the restored comic page and immediate neighbour on open, expanded direction-aware prefetching with a bounded ZIP-only reverse neighbour, and removed the routine centre-screen “Preparing page” overlay.
- Retained the existing bounded texture cache, page-curl path, document-scoped controls, and archived format support in source. Physical Switch behaviour has **not** been validated.

### Pending before v0.9.3 release
- Focused regressions, RAR4/RAR5/solid RAR and CBZ performance measurements, memory and controller acceptance checks, native Switch build/NRO validation, version consistency, source identity, packaging hashes and publication.
- The current shipping binary and download links remain **v0.9.2** until a new tested release is published.

## [0.9.2] - 2026-10-24

### Added
- Optional two-page EPUB/PDF and comic layouts, smart comic spread pairing, and wide-page handling; single-page remains the default.
- Interactive page curl, comic series and page navigation, reading journal, book-scoped reading settings, and Natural Paper / Soft E-Ink appearance options.
- Shared touch `+` / Joy-Con Plus quick menu, asynchronous library scanning, and supported software/system brightness controls.

### Changed
- Improved EPUB, PDF, CBZ and CBR reading, including RAR4/RAR5/solid comic archives, cover loading and reader navigation.
- Updates are staged only after TLS, SHA-256, NRO/NACP identity and version validation. Installing the staged NRO remains a manual file-manager action.

### Fixed
- Preserved the last complete library inventory when scans are cancelled, incomplete, stale, or cannot save their inventory.
- Preserved the previous verified update candidate across cancellation and staging write, rename, or storage failures.
- Hardened release metadata parsing, scanner path checks, candidate selection, and cover telemetry accounting.

### Performance
- Added application-level 100/500/1,000-book cover-pipeline measurement and bounded telemetry for startup, viewport fill, cache, decode, upload queue and frame times.
- Reduced avoidable metadata work and kept the existing 48 MiB disk / 512-entry cover cache bounds.

### Security
- The updater keeps TLS peer/host verification enabled and fails closed on invalid certificates, release metadata, hashes, NRO identity or versions.
- The running canonical NRO is not changed by download/staging. The distribution ZIP excludes user books, settings, progress, bookmarks and Reading Journal data.
- Retained AGPL-3.0 and upstream/fork attribution; see [LICENSES.md](docs/LICENSES.md) and [SOURCE_ATTRIBUTION.md](docs/SOURCE_ATTRIBUTION.md).

### Known Limitations
- Verified updates are staged; replacing the installed NRO is manual. There is no power-loss-tolerant automatic launcher installation.
- Broad physical Nintendo Switch acceptance testing has not been completed; this release is not hardware-certified.
- First startup and cover-cache performance for large libraries varies with the console, SD card, document mix and cache state.

## [0.9.1] - 2026-10-20

### Documentation clarification (main branch, after binary re-release)
- Corrected current NRO digest references and the 38-item physical Switch acceptance checklist; no device PASS claims were added.
- Aligned comic formats, controls, roadmap and source provenance with the shipped CBR/page-curl implementation. The existing v0.9.1 tag and assets remain unchanged by this documentation-only commit.

### Added
- **Full CBR & Enhanced CBZ Comic Book Engine:**
  - Built-in reader support for `.cbz` (ZIP) and `.cbr` (RAR4, RAR5, and solid archives via vendored `unarr` decoder).
  - Magic-byte container sniffing to gracefully detect and load misnamed comic archives.
  - Pure RGB rendering preserving original comic colors, with optional reversible reading palette.
  - Strict security and resource limits (no SD temp extraction, reject path traversal/symlinks/encryption, 48 MiB per-image safety cap, 100 MP limit).
  - ComicInfo.xml metadata extraction (Title, Writer, Series, Number, Genre, Manga direction, FrontCover, Page bookmarks).
  - Expanded cover worker memory budget (96 MiB) with automatic fallback to subsequent pages if the cover page is damaged.
- **Interactive 3D Perspective Page Curl (Page Flip):**
  - Authentic developable cylinder mesh curl (`PageCurlRenderer`) tracking finger contact position, grip height (`anchorY`), vertical drag (`dragY`), underside tint, specular highlight, and contact shadow.
  - Interactive touch drag-to-turn with physics-based commit/cancel flick thresholds.
  - Supports paginated EPUB, PDF, CBZ, and CBR (including RTL manga page direction and zoomed sheets).
  - Persisted user toggle in Settings → Navigation (`pageCurlAnimation`, default on).
- **Comic Reader Ergonomics & Series Grouping:**
  - Multi-issue comic series (`#1`, `01`, `v01`, `Vol 01`, `Cilt 01`, `ComicInfo.xml`) collapsed into single series shelf cards in the library.
  - 3-tab contents modal (`Chapters`, `All Pages` grid, `Bookmarks`) with background thumbnail generation and interactive scrubber preview bubble.
  - Visible on-screen "Fit page" pill (`Sayfaya sığdır`) when zoomed in, restoring 1.0x view with one tap.
  - Continuous controller trigger zooming (ZL/ZR) and smooth D-pad/analog stick panning with fractional accumulator.
- **Overflow-Safe Natural Numeric Sorting:** Implemented `compareNatural` and `naturalCompare` for titles, authors, series, and filenames. Multi-digit runs are compared deterministically regardless of number length, without integer overflow risk.
- **Rotated Display Controller Ergonomics:**
  - Independent repeat, deadzone, and state tracking for both Left and Right analog sticks.
  - Trigger axes (ZL/ZR) excluded from inadvertent directional menu triggers.
  - Analog stick panning for zoomed PDFs and comics dynamically matches the active display orientation (Landscape, Portrait CW, Portrait CCW).
  - UI focus navigation on rotated screens follows visual screen directions.
  - Direct digital D-pad and shoulder page turns remain intuitive and uninterrupted across orientations.
- **Catalog & File Management Enhancements:** Added CBZ/CBR format badges and metadata display in book inspection sheet, guarded file deletion support for comics, and updated fallback cover designs with distinct comic styling.

### Fixed
- **CBZ/CBR Viewport Surround:** Eliminated pure white borders around zoomed and fitted comic pages; outer margins now seamlessly match the active book or global theme (`pal.bg`, e.g., Vintage Sepia, OLED Black).
- **Zoom & Pan Stutters:** Decoupled GPU page texture uploads from zoom and pan transformations; debounced high-resolution zoom refinements.
- **Navigation Modal Text Overlap:** Fixed layout in Navigation settings tab (`tab == 3`) where the continuous scroll hint overlapped row 5 by placing the page curl toggle at row 6 and positioning the hint below it.
- **Reader D-Pad Page Turning:** Fixed a controller routing regression where D-pad Right/Left in rotated portrait modes triggered vertical panning instead of advancing/reversing pages.
- **Cover Artwork Format Display:** Fixed fallback cover renderer and catalog detail modals displaying "EPUB" for comic archives.

### Changed
- **Version Integrity:** Retained exact target version `v0.9.1` across `core.hpp`, `CMakeLists.txt`, `Makefile`, NACP metadata, packaging tools, and documentation.
- **Release Assets:** Updated release distribution with verified SHA256 hashes (`PaperNX-switch-v0.9.1.zip`: `898a1f07...`, `PaperNX-licenses-v0.9.1.zip`: `2890920a...`, `PaperNX.nro`: `f411daf2...`).

---

## [0.9.0] - 2026-10-19

### Added
- **Production Open-Source Release:** First full production release of PaperNX for the Nintendo Switch.
- **English Default Localization:** Clean installations now launch in English (`language = 1`) by default.
- **Comprehensive UI String Translations:** Added over 60 missing bilingual translation pairs in `localization.hpp` covering all settings tabs, orientation toggles, reading stats, and dialogs.
- **Separate Compliance Package:** Automated packaging of `PaperNX-licenses-v0.9.0.zip` containing GNU AGPL-3.0 and all third-party SDK/library notices with SHA256 checksums.
- **Minimal SD Distribution:** Clean `PaperNX-switch-v0.9.0.zip` containing only necessary executable and font files; excludes test directories, logs, documentation, and user databases to prevent overwriting existing user data.
- **Automated Distribution Verifier:** `scripts/verify_distribution.py` verifying NACP metadata, NRO signatures, ZIP structure, and clean SD bundle layout.

### Changed
- **Author & Copyright:** Standardized application author to `Yusuf Buzrul` across NACP metadata, source code definitions, Makefiles, and build receipts.
- **Settings Reset Logic:** Resetting settings to defaults in Settings UI (Tab 5) now respects the default language setting (`appSettings_.get().language`) instead of hardcoding language 0.
- **Documentation Architecture:** Merged multilingual documentation into a unified `README.md` featuring an English default layout with a collapsible Turkish section.
- **Community Standards:** Added `CONTRIBUTING.md`, `SECURITY.md`, and comprehensive open-source documentation.

### Fixed
- **Reading Journal Test Boundary Condition:** Fixed day-of-week rollover bug in `tests/reading_journal_tests.cpp` where hardcoded test dates caused calendar week mismatch depending on the day the test suite was executed.
- **Font Pointer Cleanup:** Fixed destructor in `src/typography.cpp` to ensure `sectionFont_` and `buttonFont_` pointers are cleanly nulled during teardown.

---

## [0.4.0-rc3] - 2026-10-18 (Development Pre-release)

### Added
- Continuous vertical scrolling mode for EPUB documents.
- Nine natural paper appearance presets with unified UI coloring.
- Interactive live typography preview in settings modal.
- Bounded cover cache with automatic corruption recovery.
- Offline reading statistics and reading journal.

---

## [0.3.0] - 2026-10-15 (Foundation Release)

### Added
- Native Nintendo Switch EPUB/PDF reader core with SDL2 and MuPDF 1.23.11.
- Touchscreen-first user interface with Joy-Con controller support.
- File browser with SD card navigation.
- PDF rendering with zoom, pan, and document filters.
- Dual-language interface support (English / Turkish).
