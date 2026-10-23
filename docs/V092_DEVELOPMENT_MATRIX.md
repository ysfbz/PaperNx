# PaperNX v0.9.2 — Development Acceptance Matrix

> **Audit correction (2026-10-23):** The older `CODE COMPLETE / TESTED` labels in this historical matrix are withdrawn as acceptance evidence. The latest dated section at the end is the evidence-based status and supersedes every earlier completion or release-readiness statement in this file.
> **G4 correction (2026-10-24):** The earlier updater row's “rollback on failure” and “deferred restart swap” description is historical and no longer describes current code. The application now stages a verified NRO for external manual replacement and has no self-install API. Desktop tests do not verify Switch FAT behavior or physical replacement.

**Baseline:** `main@8db85384a1b6c277c74766601ef46e9a672328df` (2026-10-23 audit)  
**Feature branch:** `feature/v0.9.2-complete-reader-enhancements`  
**Latest source-bearing HEAD:** `0923141c72d543c0c578da539337f5ec4ad6b1a4` (G7 cover-queue fairness checkpoint).
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
| 05 | Multiple sources and asynchronous scanning | TARGETED DESKTOP PASS; full acceptance pending | `scanRoots()` remains the single walker. `LibraryScanner` reports incomplete traversal and does not replace the visible list or inventory on missing/inaccessible roots. UTF-8 paths round-trip through the inventory. Focused evidence: `tests/library_index_tests.cpp` and `tests/modern_library_tests.cpp`; the 100/500/1000/10000 stress, Switch SD permission and full CTest matrices remain open. |
| 06 | Chapter/page reader chrome | CODE COMPLETE / TESTED | `src/reader_book_chrome.inc` provides chapter titles, page range captions (`bookChapterCaption`, `readerFooterCaption`), scrubber, bookmark indicators, and low-distraction folio. Verified with `tests/book_chrome_tests.cpp`. |
| 07 | Modern menus, app theme and accent colors | HISTORICAL STATUS WITHDRAWN; palette integrated in shared tokens | `include/papernx/interface_theme.hpp` is deleted; palette resolution now lives in `include/papernx/theme.hpp`. Its fork-derived lineage and Refael Bar attribution remain documented. Current acceptance still requires the dated tests/device matrix below. |
| 08 | Secure native self-update | HISTORICAL STATUS WITHDRAWN; G4 verified-manual path | Current code verifies and stages the release beside `PaperNX.nro`, but does not mutate the canonical NRO. See dated G4 evidence below. Switch file-manager replacement and legacy boot recovery remain unverified on hardware. |
| 09 | Persistent cover cache optimization and safe clearing | HISTORICAL STATUS WITHDRAWN; TARGETED CACHE TESTS PASS; REAL-DOCUMENT PERFORMANCE UNVERIFIED | Existing worker cache and safe-clear path remain; disk hit/miss and size stats are now visible in settings. Synthetic 100/500/1000 repository I/O is documented in [`V092_COVER_BENCHMARK.md`](V092_COVER_BENCHMARK.md); it does not measure document decode or Switch performance. |
| 10 | Natural paper reading (Soft E-Ink) | CODE COMPLETE / TESTED | Preset 9 `Soft E-Ink` (`#242424` text / `#EAE7DF` background) added to `src/v04_choices.inc` as a neutral 10th preset. Available in reading and global themes with Turkish and English localization (`paperPresetLabel`). Verified via `tests/check_utf8_integrity.py`. |
| 11 | Brightness | CODE COMPLETE / HARDWARE UNVERIFIED | Full-frame software dimming pass (10–100% brightness) after portrait composition in `src/main.cpp`. Added Nintendo Switch hardware backlight hook (`updateSwitchBacklight` via `lblSetCurrentBrightnessSetting`) under `#ifdef __SWITCH__`. Interactive slider and reset button in Appearance tab. Note: Physical Switch backlight verification is marked HARDWARE UNVERIFIED. |

## Invariants and release gate

- The older “all 11 features complete” summary is withdrawn as current acceptance evidence; see the latest dated checkpoint and the open gates below.
- The older Ponytail “strictly maintained” and extraction summary is historical only; no current full-repository Ponytail audit is claimed here.
- UTF-8 and packaging results above are historical checks from prior snapshots, not a verification claim for the current source HEAD.
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
| 09 | Cover cache | Repository safety and synthetic scale tests PASS; real-document performance NOT RUN | `src/cover_repository.cpp`, `src/main.cpp`; focused clear/corruption/budget tests pass. Five synthetic repository-only runs cover placeholder counts 100/500/1000; no actual archive-open count, decode latency, UI frame time, or before/after speedup was captured. See [`V092_COVER_BENCHMARK.md`](V092_COVER_BENCHMARK.md). |
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

## P. 2026-10-24 — G6 cover-cache counters and synthetic scale run

The settings screen now exposes the existing disk-only cache hit/miss counters alongside entry count and bytes. The worker refreshes those values after each cache lookup, including misses, and after cache writes so a failed decode or a newly created thumbnail is reflected accurately. Turkish and English labels are covered by localization tests. The configured 32 MiB and 512-entry limits were left unchanged.

Five runs of `papernx_cover_performance_tests` measured repository-only I/O for placeholder counts of 100, 500 and 1000. With 94,556-byte synthetic cache blobs, the byte budget retained 100, 354 and 354 entries respectively. The complete run output and median/p95 summary are in [`V092_COVER_BENCHMARK.md`](V092_COVER_BENCHMARK.md) and `verification/v092/cover-benchmark/synthetic-container-runs.txt`. This is not a real EPUB/PDF/comic decode benchmark; `COVER-01` remains open and no speedup is claimed.

| Area | G6 result | Evidence |
|---|---|---|
| Cover repository, settings counters, localization | TARGETED DESKTOP PASS | `PaperNX` built; `papernx_cover_tests`, `papernx_cover_performance_tests`, and `papernx_localization_tests` passed **3/3**. |
| Safe clearing, cache budget, rebuild | TARGETED PASS | `papernx_cover_tests` exercises safe removal, user-file preservation, budget limits, corruption handling, and regeneration. |
| 100/500/1000 real-document cold/warm, decode/reopen, UI/FPS/RAM, Switch SD | NOT RUN / UNVERIFIED | The five-run scale test uses placeholder `.epub` files and fixed RGB buffers. No real document decode or renderer is involved. |

**Decision remains INCOMPLETE — BLOCKERS REMAIN.** Continue with G7 reader/UI/brightness regression and Ponytail audit, then close feasible native and Switch gates. Keep real-document cover performance and physical Switch measurements explicitly open.

## Q. 2026-10-24 — G7 host regression and cover-worker fairness

`thumbWorkerLoop()` previously consumed thumbnails whenever any were queued, so a continuously replenished visible-cover queue could delay metadata indexing indefinitely. A small three-thumbnail burst limit now gives one metadata job a turn when both queues have work; metadata still runs immediately when there are no thumbnails. The visible viewport remains prioritized, and no second worker or queue was added. `shouldProcessCoverMetadata()` has direct boundary assertions in `tests/modern_library_tests.cpp`.

| Area | G7 result | Evidence |
|---|---|---|
| Cover worker scheduling | TARGETED DESKTOP PASS | `papernx_modern_library_tests` passed **1/1** including metadata-turn boundaries and existing async library flows. |
| Reader, settings migration, brightness, Joy-Con, spreads and Page Curl | TARGETED DESKTOP PASS | Current G7 set passed **6/6**: core, reader input, reader spread, Page Curl app, application test, and its fixture setup test. |
| Switch cross-build/link | PASS | `docker compose run --rm switch bash -lc 'export MUPDF_PREFIX=/work/prefix-mupdf-switch; make all -j4'` built `PaperNX.elf` and NRO from the G7 source worktree; CA and font RomFS validation ran. |
| Physical console / Switch rendering, input, brightness and SD | NOT RUN / UNVERIFIED | No device was available; the expanded v0.9.2 test record is [`V092_DEVICE_ACCEPTANCE.md`](V092_DEVICE_ACCEPTANCE.md). |

**Decision remains INCOMPLETE — BLOCKERS REMAIN.** This checkpoint covers a starvation edge case and targeted host/cross-build checks only. Real-document cover timing, full acceptance, physical device evidence, independent source redesign, and full current-HEAD sanitizer/native gates remain open.

### Ponytail full audit findings and code decisions

- Fixed the recursive Switch Makefile path so the verified MuPDF prefix reaches the linker; the resulting build proves `-lcurl` resolves with the Switch toolchain.
- Centralized interface palette selection in `ThemePalette` and removed the duplicate `interface_theme.hpp` token source.
- Kept existing page cache/curl algorithms rather than changing hashes for appearance; spread protection/clipping and preference integration remain covered by regression tests.
- Corrected stale fixtures and synchronous-scan assumptions in legacy application tests; dedicated tests exercise the actual asynchronous scan path.
- Kept the main application class and existing `.inc` organization to avoid a broad, behavior-risking move without a focused boundary. The file remains large and is a maintainability risk, not a completed modularization.
- Source origin remains conservative; see [`V092_SOURCE_PROVENANCE_AUDIT.md`](V092_SOURCE_PROVENANCE_AUDIT.md). No legal attribution was removed.

## H. 2026-10-23 continuation after the reader quick-menu checkpoint

**Current branch HEAD:** `8c38513` on `feature/v0.9.2-complete-reader-enhancements`; application source is unchanged from `b9a22bc` and `8c38513` adds only a test. **Decision remains INCOMPLETE — BLOCKERS REMAIN.** The six-file byte-identity statement is historical at `a35aa7b`; at current HEAD five existing files have different blobs and `include/papernx/interface_theme.hpp` is deleted. Fork-derived attribution remains in place. See [`V092_SOURCE_PROVENANCE_AUDIT.md`](V092_SOURCE_PROVENANCE_AUDIT.md).

| Checkpoint | Result | Evidence |
|---|---|---|
| Library scan cancellation | PASS | Commit `e791394`; test-only commit `8c38513` covers queued rescan coalescing. Targeted `papernx_library_index_tests` and `papernx_modern_library_tests`: **2/2 passed** after the code fix; the updated modern-library queue regression also passed **1/1**. |
| Updater incomplete-state recovery | PASS on host | Commit `b9a22bc`. Missing boot record plus `.swap`/`.boot.started` restores `.old`; ambiguous state without backup reports an error; safe orphan backup is cleaned only with canonical NRO present. Targeted updater unit and local HTTPS tests: **2/2 passed**. |
| Switch link and RomFS | PASS | `docker compose run --rm switch bash -lc 'make -j4 && python3 scripts/verify_artifacts.py --version 0.9.1'`: `PaperNX.elf`/NRO validated for AArch64; `-lcurl` linked; embedded CA matched `romfs/cacert.pem` (118 certificates, 200,116 bytes). NRO SHA-256 `e6efb4e6a1f3e3163c1237ffc3982d2047409e34f502712f06d370e9ea420b64`. Hardware field remains `UNVERIFIED`. |
| Reader quick menu and single-page defaults | PASS on targeted host tests | Commit `36aec98`; earlier focused reader/app test set passed. Joy-Con SDL START event and touch `+` use the same quick menu state. Physical controller remains unverified. |
| Full native suite at this HEAD | NOT RUN | A full 30/30 CTest run is recorded at the earlier `8204f77` checkpoint. It was not restarted after the reader checkpoint and later cancellation/updater changes, per the instruction to avoid restarting the full verification chain. |
| Performance matrix | PARTIAL / UNVERIFIED | Synthetic cache scale checks exist; no cold/warm end-to-end 100/500/1000-book timing, archive reopen counters, Switch FPS/RAM traces, or before/after speedup data. |
| Hardware | UNVERIFIED | Physical Switch TLS/NIFM, brightness restoration, suspend/resume, Joy-Con input, and SD interruption/recovery were not exercised. |

The updater now handles some incomplete metadata states if PaperNX can launch. It does **not** make the two-rename FAT interruption window power-loss proof: if the canonical NRO path is absent, manual restoration from `.old` using an external file manager or computer is still required. No release, tag, PR, merge, or GitHub Actions run was created.

## K. 2026-10-24 — G2 cold-cache comic turns

Code checkpoint `ffb6d7047b3874746ae5a120209ab608f2590e4c` defers cold comic destination pages to `ComicPages`, preserves the current page/progress while waiting, coalesces rapid turns, and clears failed requests without changing saved position. Active current-page fallback remains in the existing SDL/main-thread path; missing partner and destination rasters remain worker-owned. Touch swipe release while waiting becomes a queued turn.

| Area | G2 result | Evidence |
|---|---|---|
| Comic cache / input / spreads | TARGETED DESKTOP PASS | `papernx_comic_performance_tests`, `papernx_reader_input_tests`, `papernx_reader_spread_tests`: **3/3 PASS** after the G2 changes. The targets rebuilt again after a small helper simplification; latest `papernx_reader_input_tests`: **1/1 PASS**. |
| Cold-cache user action | PASS | Performance test checks controller rapid-turn coalescing, delayed touch release, no page/progress mutation before the target is ready, worker failure cleanup, and no target UI rasterization. |
| 37-page RAR5 idle/turn exercise | PASS, host only | Direct executable run with `tests/fixtures/comics/performance-37.cbr`; 30 curl frames 321 ms, 30 pan frames 10 ms, zero UI rasterizations in those loops, worker opens/closes balanced after suspend/stop. No baseline; no speedup claimed. |
| Full native / Switch / hardware | NOT RUN / UNVERIFIED | Full CTest and Switch cross-build were not rerun at G2. Physical Switch renderer/input/timing remain unverified. |

**Decision remains INCOMPLETE — BLOCKERS REMAIN.** Independent scanner/index/parser redesign, updater's canonical-NRO power-loss window, full current-HEAD test coverage, hardware validation and the requested end-to-end cover performance matrix remain open.

## I. 2026-10-24 — Queued scan regression

The test-only branch commit `8c38513` exercises the production `startLibraryScan()` coalescing path: a second request cancels the running scan, the partial/cancelled snapshot is not applied, and one replacement scan is published. `papernx_modern_library_tests` was rebuilt and passed **1/1**. The first run found a stale assertion expecting the old five-book state after the newly queued scan; the expected post-scan count was corrected to six, and the focused test passed on rerun. No production source changed after `b9a22bc`; the full native suite, hardware matrix and performance matrix were not rerun.

## J. 2026-10-24 — Cancellation during inventory staging

The code commit `faecf30` adds an atomic cancellation check while converting the completed scan into the disposable `ProgressStore` inventory. A cancelled worker skips the save, leaving the prior inventory file untouched; the existing atomic store save still completes a whole snapshot if cancellation arrives after persistence begins. `papernx_library_index_tests` and `papernx_modern_library_tests` rebuilt and passed **2/2**. The Switch cross-build and artifact validation also passed at `faecf30`: AArch64 ELF/NRO, `-lcurl`, 118 certificates embedded (200,116 bytes), NRO SHA-256 `30c29d0ca291c5dacdc39040627e0a094f25d300259607f22aeb4b3a0e782a95`. No full CTest rerun, hardware test or end-to-end performance measurement was performed. This narrows cancellation's inventory-write window but does not change the provenance classification or complete the requested independent scanner/index redesign.

## L. 2026-10-24 — G3 comic paper reverse / fixed folio

Code commit `37d978f0f34ba86f0f1bda95088d061cae946723` is pushed to the existing feature branch. Comic curls now paint one pinned page/range caption in the reader overlay instead of animating separate folio textures. Comic backs use a theme-derived pale paper surface with subdued reverse ink; paired curls preserve the full-color target page at the final endpoint. Tests read the footer region at six animation progresses and compare single/wide and paired texture captures.

| Area | G3 result | Evidence |
|---|---|---|
| Comic paper reverse / fixed folio | TARGETED DESKTOP PASS | `papernx_reader_input_tests`, `papernx_reader_spread_tests`, `papernx_comic_app_tests`, `papernx_page_curl_app_tests`; the first three also cover single/paired/zoom and endpoint/cancel behavior. |
| Comic worker performance regression | PASS | `papernx_comic_performance_tests`; no new UI rasterization regression was observed. No before/after timing baseline was captured. |
| Page Curl normal and fallback renderers | PASS, 2/2 | `papernx_page_curl_tests` and `papernx_page_curl_fallback_tests`. |
| Pixel screenshot evidence | PASS | `verification/v092/ui/comic-folio/comic-paired-0.bmp`, `...-20.bmp`, `...-40.bmp`, `...-60.bmp`, `...-80.bmp`, `...-100.bmp`; footer pixels are asserted identical during the curl. |
| Full native / Switch / sanitizers / physical device | NOT RUN / UNVERIFIED | Full suite and Switch build were not restarted at G3; hardware renderer, Joy-Con and SD behavior remain unverified. |
| Performance impact | UNVERIFIED | The extra per-frame folio render targets were removed, but texture memory/frame-time/FPS were not measured against a baseline. |

**Decision remains INCOMPLETE — BLOCKERS REMAIN.** Updater canonical-NRO power-loss recovery, source-module redesign, full current-HEAD suite, Switch verification, physical-device acceptance and cover benchmarks remain open.

## N. 2026-10-24 — G4 verified-manual updater and shared Joy-Con/touch `+`

Source commit `81a3b8c389e7b79ee018343223719cba67992675` removes the application's self-install operation. After verified download, the payload stays as adjacent `PaperNX.nro.new` plus `.ready`; UI asks the user to close PaperNX and replace the canonical NRO with an external file manager, retaining a backup. No new `.old/.swap/.boot` transaction is created. Recovery code remains only for legacy transactions left by earlier builds. The external replacement is not atomicity- or rollback-guaranteed by PaperNX.

The Joy-Con physical `+` maps to SDL `START` and calls the existing `toggleQuickMenu()`; touch `+` calls the same method. One `quickMenu_` owns both inputs. Regression coverage toggles the menu across touch/controller inputs without creating a second menu state.

| Area | G4 result | Evidence |
|---|---|---|
| Updater desktop sources | PASS | `PaperNX`, `papernx_self_update_tests`, `papernx_reader_input_tests`, `papernx_localization_tests` built in the pinned desktop container. |
| Updater and input integration | PASS, 4/4 | CTest `papernx_self_update_tests`, `papernx_self_update_network_tests`, `papernx_reader_input_tests`, `papernx_localization_tests`. Includes HTTPS certificate rejection, bad hash/NRO identity/version, cancellation, interrupted staging cleanup, ready-package resume, unchanged canonical NRO/user-data sentinels, legacy marker recovery and cross-input `+` toggle. |
| Switch link/NRO | PASS | Pinned devkitPro `make all -j4` completed; ELF is AArch64 and has linked `curl_easy_init/perform/cleanup` symbols. NRO is 49,580,479 bytes, SHA-256 `9f2e05997fc1d1336ad68c6ddd6af251c7739786a495f76732c5de6f4bcca1b6`. |
| CA RomFS | PASS | Built NRO contains exact source `romfs/cacert.pem`: 118 certs, 200,116 bytes. Existing `verification/artifacts.json` was left untouched. |
| Physical Switch/manual replacement | NOT RUN / UNVERIFIED | No actual microSD transaction, external file manager replacement, power interruption, Joy-Con physical input or boot recovery test. |
| Current release readiness | INCOMPLETE | Full native suite, sanitizer, G5 source redesign, G6 benchmarks, G7 whole-feature regression and physical acceptance remain open. |

**Decision remains INCOMPLETE — BLOCKERS REMAIN.** Self-install is disabled and verified download remains usable. PaperNX makes no claim about external file-manager atomicity or automatic recovery after that external replacement.

## O. 2026-10-24 — G5 scanner/index error boundary and parser contract

**Source commits:** `2411111bce1c23c3dde54d0b300e81eb42432ccd` (`fix(library): preserve index on incomplete scans`) and `7aa78bc4da9714227a8ea3a9c48ae7d1552b881a` (`fix(library): restore history on scan worker`). Scanner/index code remains fork-derived/adapted.

- `scanRoots()` keeps the shared walker and reports incomplete traversal for unavailable roots or filesystem metadata/iteration errors. `LibraryScanner` discards partial results and does not write the inventory; `processLibraryScan()` keeps the visible library and shows an error. A successful traversal with a failed cache write remains distinguishable.
- Scanned `Book::path` and title use `std::filesystem::path::u8string()`. Tests cover Turkish/UTF-8 scan and inventory restoration. Publication skips redundant progress-file `stat` calls on the UI thread.
- When scan-on-launch is enabled, progress-only paths are passed to the worker; `init()` does not synchronously `stat` historical books. The modern library test verifies recovered title, author and page position.
- `update_json.hpp` source was not redesigned. Tests now reject duplicate keys inside ignored nested objects and JSON exceeding its node cap; the parser remains fork-derived.

| Check | Result | Evidence |
|---|---|---|
| Desktop build | **PASS** | `docker compose run --rm desktop bash -lc 'cmake --build build-desktop --target PaperNX papernx_library_index_tests papernx_modern_library_tests papernx_localization_tests papernx_catalog_tests papernx_core_tests papernx_self_update_tests --parallel 4'`. |
| Targeted CTest | **PASS, 7/7** | `papernx_library_index_tests`, `papernx_modern_library_tests`, `papernx_localization_tests`, `papernx_catalog_tests`, `papernx_core_tests`, `papernx_self_update_tests`, `papernx_self_update_network_tests`. |
| Incomplete-scan safety | **PASS** | Mixed valid/missing roots produce an incomplete result; inventory bytes and visible books remain unchanged. Unicode inventory round-trip passes. |
| Switch, full CTest, ASan/UBSan, physical Switch and cover benchmark | **NOT RUN / UNVERIFIED** | G5 did not rerun these checks. G4 Switch evidence does not cover G5 sources. |

**Decision remains INCOMPLETE — BLOCKERS REMAIN.** Scanner/index adaptations are safer but are not represented as independent/clean-room source. Parser redesign, full current-HEAD verification, G6 cover measurement and physical-device acceptance remain open. No release, tag, PR, merge or GitHub Actions run was created.
