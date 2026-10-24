# V092 Code-Only Handoff (recovery session, 2026-10-24)

Status: **G4 manual staging IMPLEMENTED — UNVERIFIED; G4 automatic/A-B install BLOCKED; G5 IMPLEMENTED — UNVERIFIED; G6 IMPLEMENTED — UNVERIFIED.** No build, test, benchmark or device run was performed. Supersedes the earlier short handoff (its SHAs ab0f3dd/4ff3469 remain in history).

Code HEAD before this report: `93070ea` (`git log` is authoritative).

## G4 — updater
- **BLOCKED (automatic, power-loss-tolerant install):** needs evidence that an HBL-launched fixed launcher NRO can chainload `versions/<v>/PaperNX.nro` (`envSetNextLoad` contract, argv/ROMFS handoff) and FAT rename/flush behaviour on the Switch SD. Neither is provable from the repo; a launcher would also force a migration for v0.9.1 users from `switch/PaperNX/PaperNX.nro`. No rename-swap was written.
- Implemented: `UpdateState::Verifying`; `Ready` documented as STAGED only; `validatePending(UpdateRelease&,...)` repopulates version/sha/bytes after restart; `download()` keeps the previous verified `.new/.ready` until the new payload passes `validateNro` and only then replaces it (failure before the swap leaves it intact); `LegacyMarkers` gates staging; `prepareBoot` keeps the only `.old` when the NRO is missing; UI (`self_update_ui.inc`) shows version, SHA-256, staged path, manual steps, "NOT installed yet".
## G5 — scanner / parser
- `library_index.hpp/.cpp`: `ScanRequest`, `ScanStatus` (Complete/Cancelled/RootUnavailable/Incomplete/InventoryWriteFailed/Failed), `InventoryRepository` (same on-disk codec, schema marker kept), thread-free `scanLibrary`, `LibraryScanner::begin` returning a generation; `take()` never reports a cancelled scan complete; inventory written only for complete uncancelled scans.
- `library_scan.inc`: `libraryScanGeneration_` checked in `processLibraryScan` (stale/cancelled results dropped, restart honoured).
- `update_json.hpp`: `Limits`, `JsonCursor`, `ReleaseDecoder<Policy>`, `PaperNxAssetPolicy`; `Parser` alias keeps the old API (tests unchanged).
## G6 — covers
- `MetadataLedger` (bounded LRU 4096, no bulk clear) consulted in `scheduleMetadataIndex` and `collectBookMetadata` (covers thumbnail-first and metadata-first orders, and a skipped own-open). Cache-hit books with current catalog fingerprint are never planned.
- `CoverTelemetry` in `cover_pipeline.hpp`: counters always on; `CoverPipelineStats` via `PaperNxApp::coverPipelineStats()`; opt-in export `logs/cover-performance.json` at shutdown when `data/cover-perf.flag` exists (line 1 dataset label, line 2 `cold`/`warm`). JSON has counters, first-visible/50%/95% epoch times, upload queue-wait ring and frame-time ring (256 each). `sourceCommit` comes from `-DPAPERNX_SOURCE_COMMIT=...` (else `unknown`). Epoch = first request after all outstanding requests resolved.
- `cover_repository.cpp` startup inventory: removed per-`.pnxc` stats in the tmp/bak pass and the extra `regular()` stat in the count pass.

## For the test agent
1. Build host + NRO (`-fno-exceptions`): `update_json.hpp` template/base lookup, `library_index.cpp`, `main.cpp` (`cover_pipeline.hpp`), `self_update.cpp` `inspectLegacyMarkers`.
2. Existing `self_update_tests`/`library_index_tests`/`modern_library_tests`/cover tests; add tests for: failed download keeps old `.new`; Verifying cancel; `.boot.ok` with missing NRO; scan cancel/stale generation; `RootUnavailable` keeps inventory; ledger LRU eviction; telemetry epoch/percentiles; JSON export has no paths.
3. Behaviour risk: duplicate-asset policy moved into `PaperNxAssetPolicy` (expects `assets.size()==2` test still true); scan `take()` now sets `complete=false` if cancelled flag raised.
4. Benchmark 100/500/1000 with the flag; device checks stay with G9–G12 (not performed).
