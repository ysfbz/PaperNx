# PaperNX v0.9.4 — Library Scanning & Unified Focus Hotfix Report

**Release target:** `v0.9.4` (pre-release; hardware validation pending)  
**Baseline commit:** `a8872156c3c98ef6c8a514614391d864c2f658f7` (`main`, post-`v0.9.3` tag `65a7882`)  
**Development branch:** `fix/v0.9.4-library-focus-hotfix`

---

## 1. User-Reported Symptoms on Nintendo Switch (v0.9.3)

On real Nintendo Switch hardware running v0.9.3:
1. Adding new EPUB/PDF/CBZ/CBR files failed to update the library, showing `"Library scan incomplete"` / `"Kitaplık taraması tamamlanamadı"` and leaving the library frozen on the previously indexed 25 books.
2. Opening the Quick Actions menu (`+`) rendered two selection/focus outlines simultaneously, especially around `quick.pages`, `quick.orientation`, `quick.close`, and after closing/reopening the panel.
3. Reading settings rows (`AaModal`), Catalog rows, Choice lists, and TOC cells sometimes retained stale focus borders alongside the active controller focus box, and rapidly alternating D-pad, analog stick, and touch input left focus desynchronized.
4. Bottom items and focus borders in scroll viewports could overflow panel boundaries.
5. Certain newly discovered or CBR books rendered incomplete/stale cover states.

---

## 2. Root-Cause Findings & Git Regression Lineage

### 2.1 Switch SD (`sdmc:/`) `std::filesystem` Failures (introduced around `8204f774ee`)
Inspection of the actual Switch cross-toolchain inside `papernx-switch:latest` (`libnx 4.12.0-1`, `devkita64-gcc 16.1.0-1`, `devkita64-newlib 4.6.0.20260123-4`, `libstdc++` `bits/fs_path.h`):
- **`fs::weakly_canonical()` corruption:** `libstdc++` only defines `_GLIBCXX_FILESYSTEM_IS_WINDOWS` under `_WIN32`. On `__SWITCH__`, `fs::path("sdmc:/switch/PaperNX/books").is_absolute()` evaluates to `false`. Consequently, `fs::weakly_canonical("sdmc:/...")` in `scanRoots()`, `addBrowserFolder()`, and `openBrowser()` prepended `getcwd()`, yielding `sdmc:/switch/PaperNX/sdmc:/switch/PaperNX/books`, which `libnx` `fsdev_fixpath` rejected with `errno = EINVAL (22)` due to the second `:`.
- **`fs::equivalent()` zero-inode collision:** `libnx` `fsdev_stat` / `fsdev_fstat` zeroes `st_dev = 0` and `st_ino = 0` for SD entries. `libstdc++` `equiv_files` compares file type + `st_dev` + `st_ino`, so `fs::equivalent(dirA, dirB)` returned `true` for unrelated SD directories, silently dropping extra library folders.
- **`fs::path::lexically_relative()` mismatch:** POSIX `libstdc++` treated `"sdmc:/book.epub"` relative to `"sdmc:/"` as `"../book.epub"`.
- **`fsdev_dirnext` / `readdir` filename limit:** `fsdev_dirnext` returns `errno = ENAMETOOLONG (91)` when an entry name reaches >= 255 bytes, and `fsdev_fixpath` rejects invalid UTF-8 with `EILSEQ (138)`. Previously any single directory error failed the entire scan.

### 2.2 All-or-Nothing Scan Publication (commits `2411111bce`, `c81145c920`, `1daedb6324`, `722172055d`, `ae0a600206`)
- `scanInto()` called `fail()` on any single unreadable file, subdirectory error, depth > 16, missing extra root, or `last_write_time()` failure.
- `scanLibrary()` cleared usable progress and returned `complete = false`, while `processLibraryScan()` early-returned before updating `books_` — discarding all validly discovered books and keeping the old 25 books indefinitely.

### 2.3 Duplicate Focus Authority & Unclipped Borders (commits up to `36aec98125`)
- Legacy parallel state variables (`quickFocus_`, `aaRow_`, `catalogFocus_`, inline `ui::focusRing` calls in `renderQuickMenu`, `renderAaModal`, `renderCatalogPanel`, `renderContents`, `renderChoices`, and `reading_journal_ui.cpp`) rendered a second outline alongside `FocusManager::paintFocus()`.
- `toggleQuickMenu()` reset `quickFocus_ = 0` without resetting `"quick"` context memory in `FocusManager`.
- `focusReveal()` did not synchronize `quickFocus_ = -1` on `quick.pages` / `quick.orientation` / `quick.close`.
- `paintFocus()` drew focus rings without applying scrollable viewport clip rectangles (`SDL_RenderSetClipRect`).
- Positional `book.N` IDs lost focus continuity across Grid/List toggles and sorting.

---

## 3. Architecture & Changes in v0.9.4

### 3.1 Switch-Safe Filesystem Adapter (`include/papernx/fs_adapter.hpp`, `src/fs_adapter.cpp`)
- Added `normalizeDevicePath`, `normalizeScanRoot`, `relativeComponentsSafe`, `pathsEquivalentSafe`, `containsPathSafe`, and injectable `FsOps` (`statPath`, `inspectRegularFile`, `listDirectory`, `lastWriteTime`, `assumeDevicePaths`).
- Preserves `<dev>:/...` paths without `getcwd()` prepending, avoids zeroed `st_dev=0,st_ino=0` false collisions by checking normalized components alongside non-zero device/inode metadata, uses `opendir`/`readdir`/`closedir` + POSIX/libnx file inspection safely, and shares identical behavior between Library scanning and the File Browser.

### 3.2 Fault-Tolerant Partial Scanning & User Data Preservation (`include/papernx/library_index.hpp`, `src/library_index.cpp`, `src/core.cpp`, `src/library_scan.inc`)
- Replaced brittle abort-on-first-error traversal with iterative `traverseRootsDetailed()`.
- `last_write_time()` failures become recoverable metadata warnings (`modified = 0` fallback) rather than rejecting valid regular book files.
- Single unreadable files, inaccessible subdirectories, missing extra roots, or bounded depth/count limits record structured `ScanIssue` entries (`ScanErrorCode`, `errno`, `libnx` `Result`) and proceed with remaining healthy directories.
- **Last Complete Inventory Protection + Discovery Overlay:** `library-index.txt` (`PAPERNX-LIBRARY-1`) is ONLY replaced on clean complete scans; safely verified books found during partial scans are persisted separately in `data/library-discoveries.txt` (`DiscoveryOverlayRepository` / `PAPERNX-DISCOVERY-1`) and merged atomically on startup via `loadMergedLibraryInventory()`.
- User `progress.txt`, `catalog.txt`, `settings.txt`, bookmarks, Reading Journal data, and existing books under temporarily unavailable roots are never erased.
- Structured rotating diagnostics are written to `data/library-scan.log` (256 KiB cap) and viewable in **Browse library → Scan status & log**.
- Scan banner clears only scan-originated errors (`currentErrorIsFromScan_`), leaving settings/journal/catalog errors visible.

### 3.3 Unified `FocusManager` Authority & Viewport Clipping (`include/papernx/focus.hpp`, `src/v04_focus.inc`, `src/main.cpp`, `src/catalog_ui.inc`, `src/page_navigation.inc`, `src/v04_choices.inc`, `src/reading_journal_ui.cpp`)
- Made `FocusManager` (`navigationFocus_`) the single authority for focus-ring rendering (`ui::focusRingDrawCount == 1` per interactive frame).
- Removed duplicate `ui::focusRing` calls from `renderQuickMenu`, `renderAaModal`, `renderCatalogPanel`, `renderContents`, `renderChoices`, and replaced the calendar selection highlight in `reading_journal_ui.cpp` with a 2px accent selection outline.
- Added `FocusManager::clearContext()` (invoked on `toggleQuickMenu()`), `FocusManager::addClipped()`, clip-aware touch hit testing, and `SDL_RenderSetClipRect` around `paintFocus()`.
- Added `8px` bottom padding to `quickSheet()` and anchored library cards via persistent `"book:<id>"` with backward-compatible `"book.<idx>"` aliases.

### 3.4 Cover Pipeline & Format Badges (`include/papernx/cover_design.hpp`, `src/cover_repository.cpp`, `src/library_scan.inc`)
- Fixed `coverEditionLabel`/`coverBadgeLabel` so `Format::Cbr` shows `"CBR"` / `"CBR COMIC"` instead of `"EPUB"`.
- Cleared stale `CoverState::Failed` / `Loading` entries on new scan generations so newly discovered books decode/render covers reliably.

---

## 4. Verification Summary

- **Portable CMake suite + packaging tests** (`papernx_fs_adapter_tests` including mocked `libnx` zero-inode & `ENAMETOOLONG` fault injection)
- **Native SDL Desktop suite (`papernx-desktop`)** (`papernx_focus_regression_tests` verifying single focus ring per frame across 1280×720, 1920×1080, portrait `720×1280`, overflow pixel clipping, 100-step D-pad/analog/touch alternation, cover loading, partial scan persistence, and error isolation)
- **ASan/UBSan sanitizers (`verify-sanitizers.sh`)**
- **Nintendo Switch AArch64 cross-build & artifact integrity (`verify-switch.sh`)**

> [!NOTE]
> Automated desktop software-renderer tests and mocked Switch fault-injection suites pass, and the Switch AArch64 NRO/NACP/RomFS packaging validation passes. Live testing on physical Nintendo Switch hardware remains **UNVERIFIED**; `v0.9.4` is published as a **pre-release** (`hardware validation pending`).
