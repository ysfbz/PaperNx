# PaperNX v0.9.2 — Development Acceptance Matrix

> **Audit correction (2026-10-23):** The older `CODE COMPLETE / TESTED` labels in this historical matrix are withdrawn as acceptance evidence. Section H at the end is the latest, evidence-based status and supersedes every earlier completion or release-readiness statement in this file.

**Baseline:** `main@8db85384a1b6c277c74766601ef46e9a672328df` (2026-10-23 audit)  
**Feature branch:** `feature/v0.9.2-complete-reader-enhancements`  
**Current code HEAD:** `1cb06a135dc30dfff676a7124e2602b2ed06cc4f`; final audit documents are recorded in Section G.
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

---

## G. 2026-10-23 final hardening audit — current source of truth

**Final decision: INCOMPLETE — BLOCKERS REMAIN.** Desktop behavior and the Switch cross-build are verified. The requested independent reimplementation remains incomplete for fork-identical library scanner/index and updater JSON parser files. Update power-loss recovery has a documented manual-recovery window; real Switch TLS, input and backlight behavior remain hardware-unverified. Do not prepare a release from this matrix.

| # | Area | Current result | Source and executed evidence |
|---|---|---|---|
| 01 | Comic cache/worker | DESKTOP PASS; device timing unverified | `include/papernx/comic_pages.hpp`, `src/comic_document.c`, `src/page_cache.cpp`; `tests/comic_performance_tests.cpp`, `tests/comic_app_tests.cpp`. Bounded cache/worker and stale cancellation assertions passed. |
| 02 | EPUB/PDF spreads | DESKTOP PASS | `include/papernx/reader_layout.hpp`, `src/reader_sheets.inc`; `tests/reader_spread_tests.cpp`, `tests/page_navigation_tests.cpp`. |
| 03 | Smart comic spreads | DESKTOP PASS | `src/comic_spreads.inc`, `src/comic_document.c`; smart spread, bookmark navigation and comic app tests passed. |
| 04 | Spread page curl | DESKTOP PASS; Switch GPU/input unverified | `include/papernx/page_curl.hpp`, `src/main.cpp`; curl geometry, fallback and app tests passed. |
| 05 | Multi-root asynchronous scan | DESKTOP PASS; provenance blocker remains | `src/library_scan.inc`, `src/library_index.cpp`, `include/papernx/library_index.hpp`; `tests/library_index_tests.cpp`, `tests/modern_library_tests.cpp`. Corrupt cache, cancellation, nested roots, persistence and active reader checks passed. |
| 06 | Chapter/page progress | DESKTOP PASS | `src/reader_book_chrome.inc`, `src/reader_sheets.inc`, `src/main.cpp`; `tests/book_chrome_tests.cpp`, `tests/page_navigation_tests.cpp`, `tests/reader_spread_tests.cpp`. |
| 07 | Menus/themes/accent | DESKTOP PASS | `include/papernx/theme.hpp`, `include/papernx/ui.hpp`, `src/v04_choices.inc`, `src/v04_focus.inc`; `tests/ui_component_tests.cpp`, `tests/modern_library_tests.cpp`, app tests. Interface theme was consolidated into existing theme tokens. |
| 08 | Self-updater | DESKTOP HTTPS PASS; Switch compile/link PASS; install power-loss and physical-device behavior not fully verified | `src/self_update.cpp`, `src/self_update_ui.inc`, `src/update_json.hpp`; updater unit and local HTTPS integration tests passed. `make all` linked `-lcurl`; NRO verification found the CA bundle in RomFS. Manual restore of `.old` is required if power loss removes the canonical NRO during the two-rename window. |
| 09 | Cover cache | Functional tests PASS; requested scale/performance matrix NOT RUN | `src/cover_repository.cpp`, `src/main.cpp`; `tests/cover_repository_tests.cpp` and full native CTest passed. No before/after cover timing, 100/500/1000-book workload or archive-open count was captured. |
| 10 | Soft E-Ink/Natural Paper | DESKTOP PASS | `include/papernx/theme.hpp`, `src/v04_choices.inc`, `include/papernx/localization.hpp`; UI and application tests cover persisted settings and TR/EN labels. Comic colors remain on their existing artwork path. |
| 11 | Brightness | Persistence/slider/software fallback PASS; physical backlight UNVERIFIED | `include/papernx/app_settings.hpp`, `src/core.cpp`, `src/main.cpp`, `include/papernx/ui.hpp`; core/UI/app tests passed. Values clamp consistently (software 50–100%, system 10–100%); `lbl` control restores saved hardware state on exit/background, with software fallback on unavailable/docked systems. |

### Verification actually run

- `docker compose run --rm desktop bash -lc 'ctest --test-dir build-native --output-on-failure'` — **PASS, 29/29**.
- `docker compose run --rm desktop bash -lc 'ctest --test-dir build-native -R "comic.*performance" --output-on-failure -V'` — **PASS, 3/3**, including a generated 37-page 43,484,977-byte RAR5 fixture.
- `docker compose build switch` — **PASS**. The devkitPro image confirmed `switch-curl 7.69.1-5` and its declared zlib/libnx dependency chain.
- `docker compose run --rm switch bash -lc 'make all'` — **PASS**, including ELF link and NRO packaging. The first attempt exposed recursive-Makefile `MUPDF_PREFIX` loss; an absolute exported prefix fixed it.
- `docker compose run --rm switch bash -lc 'python3 scripts/verify_artifacts.py --version 0.9.1'` — **PASS**: AArch64 ELF, NRO/NACP assets and exact 118-certificate CA bundle, built from source commit `1cb06a135dc30dfff676a7124e2602b2ed06cc4f`. NRO SHA-256 `a99bd4f5287a0843d0d8a9aa7087db0bbe0840f33e67122674cacbd8edf8c167`; CA bundle 200,116 bytes; hardware field is `UNVERIFIED`.
- Comic host measurements: 37-page RAR5, four idle/toolbar cycles 57 ms, 30 curl frames 486 ms, 30 spread-pan frames 182 ms, and zero UI rasterizations in those measured loops. This is a Docker software-renderer result with no baseline comparison; it proves no speedup and does not predict Switch frame rate.
- Not run: 100/500/1000-book cover cold/warm benchmark, cold/warm archive-open/metadata-reopen counters, physical Switch interaction/brightness/TLS, and power-cut-at-each-install-step testing.

### Ponytail full audit findings and code decisions

- Fixed the recursive Switch Makefile path so the verified MuPDF prefix reaches the linker; the resulting build proves `-lcurl` resolves with the Switch toolchain.
- Centralized interface palette selection in `ThemePalette` and removed the duplicate `interface_theme.hpp` token source.
- Kept existing page cache/curl algorithms rather than changing hashes for appearance; spread protection/clipping and preference integration remain covered by regression tests.
- Corrected stale fixtures and synchronous-scan assumptions in legacy application tests; dedicated tests exercise the actual asynchronous scan path.
- Kept the main application class and existing `.inc` organization to avoid a broad, behavior-risking move without a focused boundary. The file remains large and is a maintainability risk, not a completed modularization.
- Source origin remains conservative; see [`V092_SOURCE_PROVENANCE_AUDIT.md`](V092_SOURCE_PROVENANCE_AUDIT.md). No legal attribution was removed.

## H. 2026-10-23 continuation after the reader quick-menu checkpoint

**Current source HEAD:** `b9a22bcf023165ab0338c6e9cbf9a92253290e7f` on `feature/v0.9.2-complete-reader-enhancements`. **Decision remains INCOMPLETE — BLOCKERS REMAIN.** The six-file byte-identity statement is historical at `a35aa7b`; at current HEAD five existing files have different blobs and `include/papernx/interface_theme.hpp` is deleted. Fork-derived attribution remains in place. See [`V092_SOURCE_PROVENANCE_AUDIT.md`](V092_SOURCE_PROVENANCE_AUDIT.md).

| Checkpoint | Result | Evidence |
|---|---|---|
| Library scan cancellation | PASS | Commit `e791394`. `LibraryScanner` publishes cancellation so the owner can join the worker; canceled snapshots are ignored and queued rescan requests restart after the worker is joined. Targeted `papernx_library_index_tests` and `papernx_modern_library_tests`: **2/2 passed**. |
| Updater incomplete-state recovery | PASS on host | Commit `b9a22bc`. Missing boot record plus `.swap`/`.boot.started` restores `.old`; ambiguous state without backup reports an error; safe orphan backup is cleaned only with canonical NRO present. Targeted updater unit and local HTTPS tests: **2/2 passed**. |
| Switch link and RomFS | PASS | `docker compose run --rm switch bash -lc 'make -j4 && python3 scripts/verify_artifacts.py --version 0.9.1'`: `PaperNX.elf`/NRO validated for AArch64; `-lcurl` linked; embedded CA matched `romfs/cacert.pem` (118 certificates, 200,116 bytes). NRO SHA-256 `e6efb4e6a1f3e3163c1237ffc3982d2047409e34f502712f06d370e9ea420b64`. Hardware field remains `UNVERIFIED`. |
| Reader quick menu and single-page defaults | PASS on targeted host tests | Commit `36aec98`; earlier focused reader/app test set passed. Joy-Con SDL START event and touch `+` use the same quick menu state. Physical controller remains unverified. |
| Full native suite at this HEAD | NOT RUN | A full 30/30 CTest run is recorded at the earlier `8204f77` checkpoint. It was not restarted after the reader checkpoint and later cancellation/updater changes, per the instruction to avoid restarting the full verification chain. |
| Performance matrix | PARTIAL / UNVERIFIED | Synthetic cache scale checks exist; no cold/warm end-to-end 100/500/1000-book timing, archive reopen counters, Switch FPS/RAM traces, or before/after speedup data. |
| Hardware | UNVERIFIED | Physical Switch TLS/NIFM, brightness restoration, suspend/resume, Joy-Con input, and SD interruption/recovery were not exercised. |

The updater now handles some incomplete metadata states if PaperNX can launch. It does **not** make the two-rename FAT interruption window power-loss proof: if the canonical NRO path is absent, manual restoration from `.old` using an external file manager or computer is still required. No release, tag, PR, merge, or GitHub Actions run was created.
