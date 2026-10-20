# Changelog

All notable changes to **PaperNX** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.1] - 2026-10-20

### Added
- **CBZ Comic Book Engine:** Built-in reader support for comic book archives (`.cbz`) powered directly by MuPDF without introducing new runtime dependencies or binary bloat.
  - Automatic comic page recognition and fast ZIP stream decoding.
  - Pure RGB rendering without destructive paper/ink inversions or synthetic texture overlays, preserving comic artwork integrity.
  - Bounded memory allocation (16 MiB archive safety limit) with fail-safe rejection of truncated or corrupt archives.
  - Complete library discovery, cover thumbnail extraction from page 0, file browser detection, and catalog metadata handling for `.cbz` files.
- **Overflow-Safe Natural Numeric Sorting:** Implemented `compareNatural` for titles, authors, series, and filenames. Multi-digit runs are compared deterministically (e.g., Volume 2 precedes Volume 10) regardless of number length, without integer overflow risk.
- **Rotated Display Controller Ergonomics:**
  - Independent repeat, deadzone, and state tracking for both Left and Right analog sticks.
  - Trigger axes (ZL/ZR) excluded from inadvertent directional menu triggers.
  - Analog stick panning for zoomed PDFs and comics dynamically matches the active display orientation (Landscape, Portrait CW, Portrait CCW).
  - UI focus navigation on rotated screens follows visual screen directions.
  - Direct digital D-pad and shoulder page turns remain intuitive and uninterrupted across orientations.
- **Catalog & File Management Enhancements:** Added CBZ format badge and metadata display in book inspection sheet, guarded file deletion support for CBZ, and updated fallback cover designs with distinct CBZ styling.

### Fixed
- **Reader D-Pad Page Turning:** Fixed a controller routing regression where D-pad Right/Left in rotated portrait modes triggered vertical panning instead of advancing/reversing pages.
- **Cover Artwork Format Display:** Fixed fallback cover renderer and catalog detail modals displaying "EPUB" for CBZ format documents.
- **Test Suite Logging:** Corrected escaped newline string in native MuPDF integration test output.

### Changed
- **Version Alignment:** Standardized application version across `core.hpp`, `CMakeLists.txt`, `Makefile`, NACP metadata, packaging tools, and documentation to `0.9.1` (`v0.9.1`).

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
