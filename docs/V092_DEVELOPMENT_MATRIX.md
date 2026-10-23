# PaperNX v0.9.2 — Development Acceptance Matrix

**Baseline:** `main@8db85384a1b6c277c74766601ef46e9a672328df` (2026-10-23 audit)  
**Feature branch:** `feature/v0.9.2-complete-reader-enhancements`  
**Fork reference:** `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`  
**Version remains:** v0.9.1; do not tag, release or merge.

## Status meanings

- **IMPLEMENTED (UNVERIFIED):** Source implementation added, but native/runtime testing has not passed.
- **PARTIAL:** Some requested behavior exists, but important acceptance criteria are unmet.
- **NOT STARTED:** No new implementation for the requested scope.
- **COMPLETE:** Implementation and required acceptance tests demonstrated. **No item is currently COMPLETE.**

| # | Workstream | Actual status | Evidence and remaining acceptance |
|---|---|---|---|
| 01 | Comic page cache, independent worker, solid RAR streams | NOT STARTED | Existing `PageCache` and comic reader remain. Need base/detail budgets, worker ownership, cancellation, protected facing textures, bounded queues, solid-archive stress tests. |
| 02 | Real EPUB/PDF landscape spreads | NOT STARTED | Need `ReaderLayout` model, EPUB reflow with spread columns, odd-page and bookmarks, orientation and continuous-mode regression tests. |
| 03 | Smart comic spreads | NOT STARTED | Need ComicInfo DoublePage, wide-scan detection, pairing state, pan/zoom centroid, edge and navigation coverage. |
| 04 | True-leaf spread curl | NOT STARTED | Existing single-page curl retained. Need centre-seam geometry, reverse/underlying textures, drag/cancel and rotation testing. |
| 05 | Multiple sources and asynchronous scanning | NOT STARTED | Existing synchronous startup `scan(root_/"books")` and file-browser path retained. Need persistent source list, bounded cancellation worker, normalization, data preservation. |
| 06 | Chapter/page reader chrome | PARTIAL | A low-distraction folio for hidden-toolbar mode was added in `src/main.cpp`. Toolbar navigation already shows a page label. Need accurate spread range, chapter progress and logical continuous-position UI. |
| 07 | Modern menus, app theme and accent colors | NOT STARTED | Theme picker was extended, but full library/popup/controller layout and accent system have not been redesigned or acceptance-tested. |
| 08 | Secure native self-update | NOT STARTED | No network/update module imported. Need explicit trust policy, TLS, signed metadata strategy, bounded download, checksum validation, atomic staging, restart and recovery. |
| 09 | Persistent cover cache optimization and safe clearing | PARTIAL | Existing disk format retained. `CoverRepository::stats()`, `clear()`, worker-serialized cleanup, settings rows, read/write hit/miss and disk counts added. Tests added to `tests/cover_repository_tests.cpp`, NOT EXECUTED. Need cache profiling, better thumbnail sizing, bounded memory stress and on-device UI acceptance. |
| 10 | Natural paper reading | PARTIAL | Existing Natural Paper remains unmodified; a less-yellow `Soft E-Ink` preset (`#F2F1EC` canvas / `#383833` ink) added and reader/global choices integrated with TR/EN names. Native render/contrast tests not run. |
| 11 | Brightness | PARTIAL | Persisted `softwareBrightness` (50–100%), added visible +/- control in Appearance and a single composited dimming pass; no document reflow required. Hardware/libnx `lbl` capability, physical-device lifecycle and proper drag slider not implemented. Software dimming is **not** backlight control. |

## Code touched for this initial development pass

- `include/papernx/cover_repository.hpp`, `src/cover_repository.cpp`: metrics, safe generated-file cleanup.
- `src/main.cpp`: single worker–renderer ownership for cleanup, UI status, software dimmer and folio.
- `include/papernx/app_settings.hpp`, `src/core.cpp`: persisted software dimmer. Existing `APP_DISPLAY_V04_` record's previously unused `scrollSensitivity` slot used; default is 100.
- `include/papernx/theme.hpp`, `src/v04_choices.inc`: non-destructive new theme option.
- `tests/cover_repository_tests.cpp`, `tests/core_tests.cpp`: regression test assertions authored, not run.

## Invariants and release gate

- Cache clear accepts only generated hashed `.pnxc`, `.tmp`, `.bak` files under the checked cache root. It never recursively deletes the root or a source-book directory.
- Thumbnail decoding and disk cleanup occur in the same worker; SDL texture eviction only on the renderer thread. Results from an in-flight pre-clear decode are discarded when cleanup completes.
- `APP_VERSION` and `APP_VERSION_FULL` remain v0.9.1. There is no added network dependency or borrowed fork code in this pass.
- GitHub review of committed source **does not constitute** compilation, unit-test PASS, emulator/SDL validation or physical Switch PASS.
- Suggested targeted verification on a configured development host: `cmake -S . -B build-core -G Ninja -DBUILD_TESTING=ON && cmake --build build-core --parallel 4 && ctest --test-dir build-core --output-on-failure`. To test UI use the existing `scripts/verify-desktop.sh` when native dependencies are configured; retain full release verification for the separately authorized phase.
- Keep main, tag, release metadata and distributable binaries unchanged.

**No claimed percentage or release-ready verdict is supported without completing and testing the remaining gates.**
