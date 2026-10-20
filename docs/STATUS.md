# PaperNX v0.9.1 — Project status

**Current Release:** [v0.9.1](https://github.com/ysfbz/PaperNx/releases/tag/v0.9.1), published 20 October 2026  
**Prior Release:** [v0.9.0](https://github.com/ysfbz/PaperNx/releases/tag/v0.9.0), published 19 October 2026  
**Platform:** Nintendo Switch (native C++17/C11, SDL2, SDL2_ttf, MuPDF 1.23.11); Linux desktop targets support development/testing.  
**Developer:** Yusuf Buzrul, with AI assistance.

## Shipped implementation

- Offline EPUB, PDF, CBZ and CBR comic reader, paginated and continuous EPUB reading, PDF/CBZ/CBR zoom/fit/pan.
- Native CBZ (ZIP) and full CBR (RAR4, RAR5, solid archives via vendored unarr) reading; true RGB rendering preserving original comic colors.
- Interactive developable 3D page curl animation with finger follow, touch hold/drag physics, underside tint, specular highlight, contact shadow, and reversible navigation toggle.
- 3-tab contents/navigation modal (Chapters, All Pages grid, Bookmarks) with lazy background preview cache, and interactive scrubber with live preview.
- Library series grouping collapsing multi-issue comic series into single shelf cards with full issue navigation.
- Overflow-safe natural numeric sorting (`compareNatural`) for titles, authors, series, and volume numbers.
- Touch-first library and file browser, quick `+` menu, display orientation-aware analog stick panning and menu focus navigation, direct digital D-pad page turns.
- Reading bookmarks/progress/history, library management, metadata, collections, and guarded file deletion.
- Offline reading journal, goals, statistics and export to SD.
- English default UI, switchable Turkish and saved language preference. Hebrew language/BiDi shaping excluded per project design.
- Minimal Switch NRO ZIP and independent license/notice archive.

## Verification state

Fresh local automated verification confirms portable 7/7, native desktop 19/19 (including native CBZ/CBR rendering, page curl tests, series tests, and full UI workflow), and sanitizer (ASan/UBSan) 19/19 test suites passing, plus verified cross-compiled AArch64 NRO and distribution package metadata. See [TEST_RESULTS.md](TEST_RESULTS.md).

**Physical Nintendo Switch acceptance remains UNVERIFIED for all 35 checks** in [DEVICE_TEST_PLAN.md](DEVICE_TEST_PLAN.md). Desktop simulation cannot establish real touchscreen latency, portrait touch mapping, controller reconnect, suspend/resume or microSD durability. The public Release is labelled a normal release, but the GitHub release flag does not imply hardware certification.

See [KNOWN_ISSUES.md](KNOWN_ISSUES.md), [BUILD_SWITCH.md](BUILD_SWITCH.md), [RELEASE_REPORT.md](RELEASE_REPORT.md) and [PRODUCT_ROADMAP_2026.md](PRODUCT_ROADMAP_2026.md). Earlier v0.3/v0.4 branches and deleted reports are historical and not active release targets.
