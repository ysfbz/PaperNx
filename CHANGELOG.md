# Changelog

All notable changes to **PaperNX** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.1] - 2026-10-20

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
