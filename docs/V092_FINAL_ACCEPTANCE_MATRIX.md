# PaperNX v0.9.2 — Final Acceptance Matrix (work in progress)

**Branch:** feature/v0.9.2-complete-reader-enhancements  
**Verified code HEAD:** a486060edd10900f1686ddb84e17da04718cef4e  
**Version in verification artifact:** 0.9.1 (development build only; no v0.9.2 release/package)  
**Decision:** INCOMPLETE — BLOCKERS REMAIN

| Gate | Status | Current evidence / open condition |
|---|---|---|
| G0 — HEAD, branch, provenance and test inventory | PASS (snapshot only) | Branch and current GitHub origin checked; source history/provenance records maintained in V092_SOURCE_PROVENANCE_AUDIT.md. User-owned finalization plan and generated fixture are excluded from commits. |
| G1 — comic worker idle/ownership | PASS (targeted host + cross-build) | Source a486060; desktop CTest 1/1, ASan/UBSan CTest 1/1, direct 37-page RAR5 run exit 0; worker stays open through 30-second idle and closes on suspend/stop. Switch AArch64 ELF/NRO and RomFS verification passed. |
| G2 — cold-cache turns and touch/controller queue | TARGETED PASS | Earlier focused reader/comic tests cover delayed texture, queued turns and cancellation; current comic performance CTest passes. Final full-current-HEAD native suite remains open. |
| G3 — paper backside and fixed folio | TARGETED PASS | Existing pixel assertions and screenshot evidence are recorded in V092_CONTINUATION_STATE.md and verification/v092/ui/comic-folio/. Full current-HEAD suite remains open. |
| G4 — updater safety / interrupted installation | PARTIAL | Verified-manual staged update avoids app replacing canonical NRO; local HTTPS/parser/recovery tests exist. Full S0–S11 fault and storage/power-cut matrix, physical SD evidence and final updater recovery decision are not complete. |
| G5 — scanner/index/parser architecture and provenance | PARTIAL | Cancellation, inventory/path validation and parser boundary tests exist; modules remain fork-derived/adapted and requested PaperNX-specific responsibility redesign is unfinished. AGPL/author attribution remains. |
| G6 — cover cache 100/500/1000 benchmark | PARTIAL | Synthetic and repeated-fixture cache measurements plus clear/regeneration tests exist. Real diverse-document cold/warm decode, viewport latency/FPS/RAM and Switch SD measurements are missing. |
| G7 — 11 features, single-page default, Joy-Con and brightness | PARTIAL | Targeted host settings/reader/UI/app tests are recorded. No full-current-HEAD acceptance of all 11 features; physical Joy-Con, backlight lifecycle and Switch visual checks are unverified. |
| G8 — Ponytail full / source audit | PARTIAL | Concrete safety, ownership, test and duplicated-theme fixes are recorded. Final diff-wide audit is pending; src/main.cpp remains large. |
| G9 — full native, sanitizer and Switch verification | PARTIAL | Current source Switch build and selected comic ASan/UBSan pass. Full CTest and complete sanitizer suite have not run at this final source HEAD. |
| G10 — physical Switch acceptance | UNVERIFIED | No physical console session or device logs are available. Do not mark hardware PASS. |
| G11 — v0.9.2 build/package/licenses | NOT RUN | Current verified NRO is a development 0.9.1 artifact. Release NACP, ZIP, manifests, source bundle and package-level verification are not prepared. |
| G12 — main/tag/GitHub Release | NOT RUN / BLOCKED | Do not merge, tag, publish, or run workflows before G4–G11 gates and required hardware gate are resolved. |

## Current evidence for G1

- Desktop: cmake --build build-native --target papernx_comic_performance_tests --parallel 4 followed by focused CTest — 1/1 PASS (32.44 sec).
- ASan/UBSan: corresponding build-sanitize target/CTest — 1/1 PASS (38.59 sec).
- RAR5: SDL_VIDEODRIVER=dummy build-native/papernx_comic_performance_tests tests/fixtures/comics/performance-37.cbr — exit 0; fixture SHA-256 unchanged: EB9EF2C91FEF3AED592F4A14EBF4524BAC13548E94D1BB9C717ED2B8E79B0E5A.
- Switch: make all -j4 && python3 scripts/verify_artifacts.py --version 0.9.1 in the Switch Docker service — PASS at source a486060; NRO 49,588,671 bytes, SHA-256 20b3e4d202bc15c3250c32dec2ad0ab300cfc7620fce2688a4ffa3f79f9050a0; CA 118 certs / 200,116 bytes.
- Performance is host/software-renderer-only: 30 curl frames 336 ms, 30 spread-pan frames 155 ms, zero UI rasterizations in measured loops. No baseline; no speedup claimed.

This matrix is updated as gates are completed. A historical test result is not a current-source PASS unless the tested source SHA is stated.
