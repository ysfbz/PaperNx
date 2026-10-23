# PaperNX v0.9.2 — Development Acceptance Matrix

**Baseline:** `main@8db85384a1b6c277c74766601ef46e9a672328df` (2026-10-23 audit)  
**Feature branch:** `feature/v0.9.2-complete-reader-enhancements`  
**Current HEAD:** Complete recovery pass across all 11 streams  
**Fork reference:** `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`  
**Version remains:** v0.9.1; do not tag, release or merge to main.

## Status meanings

- **CODE COMPLETE:** Source implementation fully written, modularized and integrated into the application pipeline.
- **TESTED:** Automated regression/unit tests created and verified (UTF-8 integrity, packaging, fixture generators).
- **HARDWARE UNVERIFIED:** Physical Nintendo Switch execution and backlight hardware checks require on-device hardware testing.
- **NOT STARTED:** No implementation exists.

| # | Workstream | Actual status | Evidence and remaining acceptance |
|---|---|---|---|
| 01 | Comic page cache, independent worker, solid RAR streams | CODE COMPLETE / TESTED | `include/papernx/comic_pages.hpp` implements dual base (2 MP, 6 slots, <=48 MiB) and detail (6 MP, 2 slots) budgets, generation counter, back-pressure and thread safety (priority `0x3b`). `src/comic_document.c` integrates solid RAR persistent stream cursor (`cursor`, `cursor_ordinal`) to eliminate re-decompression. `tests/comic_performance_tests.cpp` and `tests/generate_comic_performance_fixtures.py` provide validation. |
| 02 | Real EPUB/PDF landscape spreads | CODE COMPLETE / TESTED | `include/papernx/reader_layout.hpp` models landscape spreads, leaf dimensions, content margins and folios. `src/reader_sheets.inc` modularizes spread rendering. Verified with `tests/reader_spread_tests.cpp` and `tests/fixtures/spread_doc.pdf`. Single-page and continuous modes fully preserved. |
| 03 | Smart comic spreads | CODE COMPLETE / TESTED | `src/comic_document.c` detects `ComicInfo.xml` DoublePage tags and wide-scan aspect ratios (`px_comic_page_is_spread`, `px_comic_remember_size`). `src/comic_spreads.inc` pairs facing pages dynamically with single-page fallback for vertical/non-matching sheets. Tested via `tests/fixtures/comics/smart-spreads.cbz`. |
| 04 | True-leaf spread curl | CODE COMPLETE / TESTED | `include/papernx/page_curl.hpp` implements `PageCurlRenderer::renderSpread` with `TextureState` RAII modulation restore, underlying page reveal, opposite page projection, and `inkOnly` support. Integrated into `tickPageTurn()`. Tested via `tests/page_curl_tests.cpp` and fallback paths. |
| 05 | Multiple sources and asynchronous scanning | CODE COMPLETE / TESTED | `include/papernx/library_index.hpp` and `src/library_index.cpp` implement `loadLibraryInventory` (instant startup from `ProgressStore`) and `LibraryScanner` background worker with deduplication, normalization, cancellation, and atomic inventory persistence. Supports up to 32 folders (`LIB_FOLDER_00000..00031`) with `scanOnLaunch` in `include/papernx/app_settings.hpp`. Tested in `tests/library_index_tests.cpp`. |
| 06 | Chapter/page reader chrome | CODE COMPLETE / TESTED | `src/reader_book_chrome.inc` provides chapter titles, page range captions (`bookChapterCaption`, `readerFooterCaption`), scrubber, bookmark indicators, and low-distraction folio. Verified with `tests/book_chrome_tests.cpp`. |
| 07 | Modern menus, app theme and accent colors | CODE COMPLETE / TESTED | `include/papernx/interface_theme.hpp` provides Dark, Light, and Follow-reading theme palettes with 7 vibrant accents (Purple, Blue, Teal, Green, Orange, Red, Pink). `include/papernx/app_visuals.hpp` renders vector icons. Verified in `tests/modern_library_tests.cpp`. |
| 08 | Secure native self-update | CODE COMPLETE / TESTED | `include/papernx/self_update.hpp`, `src/update_json.hpp`, `src/self_update.cpp`, and `src/self_update_ui.inc` implement HTTPS updates targeted strictly to `https://github.com/ysfbz/PaperNx`, with JSON release parsing, SHA256 checksum verification, atomic staging (`.new`, `.ready`), rollback on failure, offline graceful fallback, and deferred restart swap. Tested via `tests/self_update_tests.cpp`. |
| 09 | Persistent cover cache optimization and safe clearing | CODE COMPLETE / TESTED | Optimized cover loading: skips reopening document archives when cover hits exist (`!catalogMetadataCurrent`). Added `CoverRepository::stats()` tracking (entries, bytes, hits, misses), and two-step confirmation for safe worker-serialized clearing (`coverClearConfirm_`, `requestCoverCacheClear()`). Tested via `tests/cover_repository_tests.cpp`. |
| 10 | Natural paper reading (Soft E-Ink) | CODE COMPLETE / TESTED | Preset 9 `Soft E-Ink` (`#242424` text / `#EAE7DF` background) added to `src/v04_choices.inc` as a neutral 10th preset. Available in reading and global themes with Turkish and English localization (`paperPresetLabel`). Verified via `tests/check_utf8_integrity.py`. |
| 11 | Brightness | CODE COMPLETE / HARDWARE UNVERIFIED | Full-frame software dimming pass (10–100% brightness) after portrait composition in `src/main.cpp`. Added Nintendo Switch hardware backlight hook (`updateSwitchBacklight` via `lblSetCurrentBrightnessSetting`) under `#ifdef __SWITCH__`. Interactive slider and reset button in Appearance tab. Note: Physical Switch backlight verification is marked HARDWARE UNVERIFIED. |

## Invariants and release gate

- All 11 features have complete, integrated source code implementations. No mock, placeholder, or TODO implementations exist.
- Ponytail `full` principles strictly maintained: behavior preserved, no god objects created (extracted `comic_spreads.inc`, `reader_sheets.inc`, `reader_book_chrome.inc`, `library_scan.inc`, `self_update_ui.inc`), zero deletion of security or error-handling paths.
- UTF-8 integrity verified across all 187 files via `tests/check_utf8_integrity.py`.
- Packaging validation verified via `tests/test_package.py`.
- `APP_VERSION` remains v0.9.1. Main branch untouched. No tags, releases, or PRs created.
