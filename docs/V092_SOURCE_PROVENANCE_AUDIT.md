# PaperNX v0.9.2 source provenance audit

**Audit date:** 2026-10-23  
**Feature branch at audit start:** `feature/v0.9.2-complete-reader-enhancements`  
**Starting HEAD:** `a35aa7b0f51aee58f44d524616a2aa55477d670c`  
**Comparison base:** `main@8db85384a1b6c277c74766601ef46e9a672328df`  
**Fork reference:** `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`

## Method

The audit compared each Git blob at the feature HEAD with the fork reference, checked whether the path existed at the comparison base, inspected the path's Git history, and read its production callers and focused tests. A matching blob establishes byte identity at those revisions; it does not, by itself, establish the authorship or license of every earlier line. AGPL-derived or shared code is therefore not called independently implemented.

## New v0.9.2 modules imported from the fork

All six paths below were absent from `main` at the comparison base and had exactly the same Git blob at the initial audited feature revision `a35aa7b0f51aee58f44d524616a2aa55477d670c` and the pinned fork reference. The source commits listed below belong to the fork history, not the feature branch's ancestry; feature commit `a35aa7b` imported the matching snapshots. These are **fork-derived AGPL-3.0 contributions**, not independent implementations. Git authorship identifies Refael Bar for scanner/index/interface-theme work and Refael Avramashvili for the updater and original JSON parser; both are credited alongside the original PaperNX authorship in `docs/LICENSES.md`.

| Path | Fork source history / initial feature blob | Design target (not completion evidence) | Behavior contract and focused test |
|---|---|---|---|
| `src/library_scan.inc` | Fork: `9bb1c95` (Refael Bar); initial feature blob `c8087dc4defa88af950bed947e6d50962bc7799f` | Rework application integration around the existing catalog/settings; retain SDL-thread publication and progress for missing roots. | Multi-root import/removal, active-reader continuity, duplicate/nested roots and UI publication in `tests/modern_library_tests.cpp`; scan persistence/cancellation in `tests/library_index_tests.cpp`. |
| `src/library_index.cpp` | Fork: `9bb1c95`, `699ef34` (Refael Bar); initial feature blob `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Replace inventory overloading with an explicit record, bounded traversal and cancellation-safe publication. | Cache reload, path changes, roots, cancellation, damaged inventory and preservation of independent progress in `tests/library_index_tests.cpp`. |
| `include/papernx/library_index.hpp` | Fork: `9bb1c95` (Refael Bar); initial feature blob `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Keep a small scanner result contract in the existing core layer. | Compile-time/public contract exercised by `tests/library_index_tests.cpp` and the application integration test. |
| `src/self_update.cpp` | Fork: `6cec7a7` (Refael Avramashvili); initial feature blob `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Target the fixed PaperNX release source, validate TLS/content, require approval and stage installation safely. | SemVer, release selection, NRO/hash validation, cancellation and interrupted-install behavior in `tests/self_update_tests.cpp`. |
| `src/update_json.hpp` | Fork: `6cec7a7` (Refael Avramashvili); initial feature blob `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Parse the bounded release-response contract and reject malformed/truncated input without a general JSON dependency. | Malformed, escaped, truncated, oversized and valid release JSON cases in `tests/self_update_tests.cpp`. |
| `include/papernx/interface_theme.hpp` | Fork: `9bb1c95` (Refael Bar); initial feature blob `be58dc293354a4e38e31eb9faa08284367845c53` | Resolve interface colors with PaperNX's existing `ThemePalette`; retain Light, Dark, Follow-reading and seven accents. | Palette selection, bounds, contrast and shared screen use in `tests/modern_library_tests.cpp` and `tests/ui_component_tests.cpp`. |

The design targets above are not completion claims; implementation status and later source identities are recorded below.

## Pre-existing v0.9.1 modules also identical to the fork

These two paths were present in `main` at the comparison base. Their algorithms are **historically shared v0.9.1 code**, not new v0.9.2 imports. Their feature-branch changes are evaluated only where the requested spread/cache work touches them; replacing them solely to alter a hash is out of scope.

| Path | Base blob | Feature blob | v0.9.2 change / test contract |
|---|---|---|---|
| `src/page_cache.cpp` | `a7ee84137a829fe9b8dae9cbdffa0eded1586f07` | `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a` | 8 changed lines for protected facing-page entries. Preserve LRU ownership and verify cache bounds, duplicate keys and protected pages through reader/comic app tests. |
| `include/papernx/page_curl.hpp` | `b040dac998111c70aa75f53fc9c6c7df96b653c4` | `e95f2d392b6e00a6d8d2b4c3d86b06b32b08d9ad` | Spread rendering added in the inherited file (65 insertions, 10 deletions versus base). Preserve the existing single-page renderer; validate both render paths, clipping, endpoints and SDL state restoration with `tests/page_curl_tests.cpp`, fallback tests and `tests/page_curl_app_tests.cpp`. |

The feature copies of these two files match the fork's audited blob, but that fact does not change their earlier presence in PaperNX. `docs/LICENSES.md` already records the v0.9.1 shared-code attribution to Yusuf Buzrul and Refael Bar; that history and all license notices remain in place.

## License and attribution

PaperNX and the referenced fork publish under AGPL-3.0. This audit does not remove or replace the project license, fork attribution, contributor credit, or third-party notices. Path-level attribution for the six new modules is recorded in `docs/LICENSES.md`. The Mozilla-derived CA bundle added for updater TLS is separately governed by MPL-2.0 and is documented with its source, revision date, checksum and size in `docs/UPDATER.md`.

## Final implementation record

### 2026-10-23 hardening source snapshot `1cb06a1`

The earlier table describes the pre-hardening feature HEAD. The current source was compared again with the pinned fork commit. A changed blob alone is not treated as proof of independent authorship.

| Module at final hardening HEAD | Final source identity | Result and honest origin classification | Integration / behavior evidence |
|---|---|---|---|
| `src/library_scan.inc` | `c8087dc4defa88af950bed947e6d50962bc7799f`, still byte-identical to the fork | Remains fork-derived AGPL code. The production application invokes `LibraryScanner` asynchronously and publishes results on the SDL thread; no independent rewrite was completed in this pass. | `tests/library_index_tests.cpp`, `tests/modern_library_tests.cpp`; both included in the passing 29-test native CTest run. |
| `src/library_index.cpp` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a`, still byte-identical to the fork | Remains fork-derived AGPL code. It reuses PaperNX `scan()` and `ProgressStore` persistence, but its file-level algorithm was not independently replaced; attribution remains required. | Async scan, inventory corruption/persistence, cancellation and data-preservation tests above. |
| `include/papernx/library_index.hpp` | `8a0ac82707cf5b402d0cbcebd790d751758b0003`, still byte-identical to the fork | Remains fork-derived AGPL API; not independently implemented. | Compiled into both application and scan tests. |
| `src/self_update.cpp` | `02da1eb1317bf9341899a26d16e6b78a7dda31ed`, no longer byte-identical | Materially redesigned around fixed repository trust, TLS/CA validation, bounded streaming, cancellation, NRO identity/hash checks, durable backup and first-frame boot confirmation. Because the implementation descends from the fork file, it is conservatively classified as an AGPL adaptation, not an independent clean-room implementation. | `papernx_self_update_tests` and 20 local HTTPS integration scenarios passed; Switch ELF/NRO link and embedded CA verification passed. Physical Switch networking/recovery remains unverified. |
| `src/update_json.hpp` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394`, still byte-identical to the fork | Remains fork-derived AGPL parser code; parser architecture was not replaced in this pass. | Malformed, escaped, truncated, oversized and valid release JSON cases are included in the passing updater tests. |
| `include/papernx/interface_theme.hpp` | Deleted; color resolution now lives in `include/papernx/theme.hpp` | The parallel interface palette header was removed and integrated with PaperNX's existing `ThemePalette` tokens. Historical fork origin and credit are retained in `docs/LICENSES.md`; no claim is made that moving code erased that origin. | Theme persistence, Light/Dark/Follow-reading, seven accents, shared UI use and bilingual screenshots are exercised by UI/application tests. |
| `src/page_cache.cpp` | `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a`, identical to fork; existed in the main project's v0.9.1 base | Shared pre-existing v0.9.1 code, not a v0.9.2 import. The facing-page protection change is retained for the spread cache; no hash-only rewrite was attempted. | Reader, spread, comic and cache tests passed in native CTest. |
| `include/papernx/page_curl.hpp` | `e95f2d392b6e00a6d8d2b4c3d86b06b32b08d9ad`, identical to fork; existed in the main project's v0.9.1 base | Shared pre-existing v0.9.1 code, not a v0.9.2 import. Spread curl remains attribution-bearing shared code; it was not rewritten solely to change its hash. | `page_curl_tests`, fallback tests and `page_curl_app_tests` passed in native CTest. |

`include/papernx/interface_theme.hpp` is intentionally no longer built or included; its replacement uses the existing theme token owner. All fork-derived paths remain attributed to Refael Bar and AGPL-3.0 in `docs/LICENSES.md`. The MPL-2.0 CA bundle has separate source and notice records.

**Independent-reimplementation acceptance:** not fully met. The library scanner/index and JSON parser still match the pinned fork at byte level; the updater has a substantive redesign but remains conservatively attributed as adapted code. They compile, are called by the app, and have passing host tests, but those facts do not make them independent implementations. Keep this as an open engineering item; do not label the entire copied-module set independently reimplemented.

**Validation executed:** full native CTest 29/29 passed; focused CBR/CBZ performance CTest 3/3 passed; devkitPro Switch `make all` linked `PaperNX.elf` and built the NRO; `scripts/verify_artifacts.py --version 0.9.1` confirmed AArch64, NRO assets and exact CA embedding. No physical Switch test was performed. The verified source revision is `1cb06a135dc30dfff676a7124e2602b2ed06cc4f`; later documentation-only changes do not alter these source identities.

### 2026-10-23 continuation: bounded release parsing and cache scale test (`4dc1e63` source snapshot)

At source snapshot `4dc1e63f98de79cd6202d8b1beb7ae4e71dc6d80` (the following provenance-doc commit did not change source blobs), the identities at that time were:

| Path | Current blob | Current relationship to pinned fork | Attribution / status |
|---|---|---|---|
| `src/library_scan.inc` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Still byte-identical | Fork-derived; no independent rewrite yet. Refael Bar source history. |
| `src/library_index.cpp` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Still byte-identical | Fork-derived; no independent rewrite yet. Refael Bar source history. |
| `include/papernx/library_index.hpp` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Still byte-identical | Fork-derived API; no independent rewrite yet. Refael Bar source history. |
| `src/self_update.cpp` | `61844fbc17ce2fdf237685a1ed8c730a20e9f6df` | Changed from fork blob | Substantially redesigned in `913fb8f` and adapted in `9eb613a`; source ancestry remains fork-derived, not clean-room. Refael Avramashvili's original contribution remains credited. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | Changed from fork blob | Replaced the generic value tree with a bounded parser that extracts only release and asset fields in `9eb613a`. This is a material redesign of fork-derived code, not a clean-room authorship claim; credit Refael Avramashvili for its source lineage. |
| `include/papernx/interface_theme.hpp` | Deleted | Not a current path | `resolveInterfacePalette` is in `include/papernx/theme.hpp` (`f0117e2304cd5fdc11a1b4cf08f49cbac0c31ce9`). The palette values/logic were moved and integrated; this remains fork-derived, not independent. Refael Bar source history. |

The fork source commits `9bb1c95`, `699ef34` and `6cec7a7` are reachable through `remotes/fork/main`, not ancestors of the feature branch. The feature import is `a35aa7b`; later PaperNX changes are in `913fb8f` and `9eb613a`. The prior v0.9.1 distinction for `src/page_cache.cpp` and `include/papernx/page_curl.hpp` remains unchanged: they predate this feature work and are not v0.9.2 imports.

`update_json::Parser` now emits only the updater's release/asset fields rather than allocating a general JSON tree. It retains the 4 MiB input cap, depth/node limits, duplicate-key rejection, Unicode escape validation and exact asset-origin checks. Parser/updater tests and the full native suite passed after this change. The synthetic `CoverRepository` test passed at 100/500/1000 book counts; it measures cache file IO and eviction only, not archive decoding, UI first paint or Switch performance. No before/after speedup is claimed.

**Historical corrected statement for source snapshot `4dc1e63`:** At the initial audited feature revision `a35aa7b0f51aee58f44d524616a2aa55477d670c`, all six paths were present and byte-identical to the pinned fork commit. At source snapshot `4dc1e63f98de79cd6202d8b1beb7ae4e71dc6d80`, `src/library_scan.inc`, `src/library_index.cpp` and `include/papernx/library_index.hpp` remained byte-identical; `src/self_update.cpp` and `src/update_json.hpp` had been redesigned while retaining fork-derived history; `include/papernx/interface_theme.hpp` was deleted and its palette resolver moved to `include/papernx/theme.hpp`. Fork history credits Refael Bar for scanner/index/theme work (`9bb1c95`, `699ef34`) and Refael Avramashvili for updater/JSON source (`6cec7a7`), alongside original PaperNX authorship and AGPL-3.0 provenance. These fork commits are not feature-branch ancestors; `a35aa7b` imported their matching snapshots. A redesign or file move does not establish independent authorship.

**Validation executed for this continuation:** native CMake build plus full CTest **30/30 passed** (324 seconds); focused parser and cover cache tests **2/2 passed**. The cover measurements are synthetic and have no before/after baseline. The updated parser has not yet been included in a Switch cross-build; physical hardware behavior remains unverified.

### 2026-10-23 continuation: unified library multi-root scan (`8204f77` source snapshot)

The PaperNX core now exposes `scanRoots()` and shares its file walker with the existing single-root `scan()` API. It canonicalizes configured roots, processes parents before nested roots, skips equivalent/overlapping roots, applies one total book limit and cancellation callback, and sorts the combined result. `LibraryScanner` delegates root traversal to this core path rather than maintaining a second root-normalization and nested-root algorithm; the SDL application still publishes the completed snapshot on its UI thread.

| Path | Source blob at `8204f774eed14be7236bf819462a4c491272a1ae` | Relationship to pinned fork | Classification |
|---|---|---|---|
| `src/library_scan.inc` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Byte-identical | Fork-derived integration; not independently rewritten. |
| `src/library_index.cpp` | `0b440a5debe4168ffb716405d051faa4419794f7` | Changed | Core multi-root traversal is delegated to PaperNX `scanRoots()`; remaining worker, inventory and preservation behavior descends from the fork version. Retain AGPL attribution; no clean-room claim. |
| `include/papernx/library_index.hpp` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Byte-identical | Fork-derived API; not independently rewritten. |
| `include/papernx/core.hpp`, `src/core.cpp` | New `scanRoots()` API/implementation in this feature | Not present in the fork source file set | PaperNX's existing single-root walker was refactored to share file enumeration and sorting across one or many roots. |
| `src/self_update.cpp` | `61844fbc17ce2fdf237685a1ed8c730a20e9f6df` | Changed | Material updater redesign, still fork-derived by history. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | Changed | Bounded release-specific parser redesign, still fork-derived by history. |
| `include/papernx/interface_theme.hpp` | Deleted; replacement blob `include/papernx/theme.hpp`: `f0117e2304cd5fdc11a1b4cf08f49cbac0c31ce9` | Original path absent | Palette logic was integrated into house tokens; historical fork attribution remains. |

`tests/library_index_tests.cpp` now checks nested/duplicate roots, the total result limit, mid-scan cancellation, corrupt inventory recovery and that a separate reading-progress file remains unchanged. The focused library test passed after this refactor. This design change improves responsibility sharing, but it does not make the still-matching `.inc`/header or remaining adapted worker code independent.

## Prior feature HEAD provenance — `e79139468a59b4b42287a2994a8e4ce7d602f01b`

The original byte-identity statement is historically accurate only for the initial audited revision `a35aa7b0f51aee58f44d524616a2aa55477d670c`. It must not be read as a current-HEAD comparison. The pinned fork commit remains the same fixed comparison reference.

| Path at `e791394` | Feature HEAD blob | Fork blob | Current relationship and attribution |
|---|---|---|---|
| `src/library_scan.inc` | `7e45b42f84fa84d3e20f2a4c209b0cc91d34b52b` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Changed by scan cancellation/rerun integration; still derived from the fork's app integration and not independent. Refael Bar / AGPL-3.0 attribution remains appropriate. |
| `src/library_index.cpp` | `44fe31d678c5164d7061f5ca6cc9b862b9e337d8` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Multi-root traversal now uses PaperNX `scanRoots()`; worker, inventory and cancellation contracts retain fork lineage. Adapted AGPL-3.0 code; Refael Bar credit remains appropriate. |
| `include/papernx/library_index.hpp` | `bfbc9980a93d5f95e93f89e9a117f62e85d4b860` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Adds cancellation to the fork-derived scanner contract. Changed blob, but not an independent API design. Refael Bar / AGPL-3.0 attribution remains appropriate. |
| `src/self_update.cpp` | `61844fbc17ce2fdf237685a1ed8c730a20e9f6df` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Material PaperNX-specific redesign and safety work, but it descends from the fork implementation. Adapted AGPL-3.0 source; retain Refael Avramashvili credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Bounded release-specific parser redesign; still derived from the fork parser, not a clean-room claim. Retain Refael Avramashvili credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette code was integrated into `include/papernx/theme.hpp`; file deletion did not erase fork provenance. Refael Bar / AGPL-3.0 attribution remains appropriate. |

At that historical `e791394` snapshot, **zero existing paths were byte-identical**: five existing files had changed blobs and the sixth was deleted. That is a file-identity statement only; all six retain historical fork provenance. The feature-branch Git history does not contain the fork commits as ancestors; the import commit copied their snapshots. The separate `src/page_cache.cpp` and `include/papernx/page_curl.hpp` remain shared v0.9.1 code already present at the comparison base, so their fork match is not a new v0.9.2 contribution.

**Corrected wording for the e791 snapshot:** “At the initial audited feature revision `a35aa7b0f51aee58f44d524616a2aa55477d670c`, these six paths were byte-identical to `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`. This is a historical snapshot, not a claim about current file identity. At feature HEAD `e79139468a59b4b42287a2994a8e4ce7d602f01b`, the five paths that still exist have different blobs and `include/papernx/interface_theme.hpp` is deleted. Their implementations remain fork-derived or adapted by Git history and code lineage; the palette was moved into PaperNX's existing theme tokens. Preserve Refael Bar's credit for the scanner/index/theme contribution and Refael Avramashvili's credit for updater/parser source, alongside existing PaperNX authorship and AGPL-3.0 notices. Changed hashes, refactoring, file movement or redesign do not establish independent authorship.”

The initial claim is therefore **VALID historically, OUTDATED if presented as current identity**. Keep the historical record and attribution; update any wording that omits its snapshot date. Attribution is still appropriate because changed files remain derivative adaptations and the theme implementation was integrated rather than clean-room rewritten.

The scan cancellation change at `e791394` now publishes a completed/cancelled result so the owner can join the worker, discards cancelled snapshots in the SDL application, and supports a queued rescan. `tests/library_index_tests.cpp` and `tests/modern_library_tests.cpp` passed after this change.

## Provenance snapshot at cancellation checkpoint — `faecf30eb4fb12fd143d3c1666d91f0b690b484c` (historical)

At the time of this verification the branch HEAD was `ed9e3b69c184de1b488c9726c660dacdef1635b2`; it is a report-only commit after source-bearing commit `faecf30eb4fb12fd143d3c1666d91f0b690b484c`. Source commit `faecf30` follows the queued-rescan test commit `8c38513` and adds cancellation checks while staging inventory records. The report-only commit leaves these source blobs unchanged. The historical six-path byte-identity statement remains limited to `a35aa7b`. At branch HEAD `ed9e3b6`, no existing path is byte-identical to the pinned fork: five have different blobs, and `include/papernx/interface_theme.hpp` is deleted.

| Path | Current HEAD blob | Fork blob | Current origin classification |
|---|---|---|---|
| `src/library_scan.inc` | `7e45b42f84fa84d3e20f2a4c209b0cc91d34b52b` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Fork-derived application integration, extended for cancellation and queued rescans; not independent. Refael Bar / AGPL-3.0 credit retained. |
| `src/library_index.cpp` | `e991d669da322528accce694923c226b753bb06f` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Core traversal reuses PaperNX `scanRoots()`; cancellation is checked while staging cache records; index/worker behavior remains fork-derived. Refael Bar / AGPL-3.0 credit retained. |
| `include/papernx/library_index.hpp` | `bfbc9980a93d5f95e93f89e9a117f62e85d4b860` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Fork-derived scanner contract with a cancellation result/API; changed blob is not independent authorship. Refael Bar / AGPL-3.0 credit retained. |
| `src/self_update.cpp` | `afa55e04b9c04d6a7269d6b2f6ea0a15875136e8` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | PaperNX-specific updater redesign plus missing-marker recovery; still fork-derived by source history. Refael Avramashvili / AGPL-3.0 credit retained. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Bounded release parser redesign retaining fork lineage. Refael Avramashvili / AGPL-3.0 credit retained. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette was integrated into existing PaperNX theme tokens; Refael Bar / AGPL-3.0 origin remains documented. |

**Corrected wording at the prior documentation snapshot:** “At the initial audited feature revision `a35aa7b0f51aee58f44d524616a2aa55477d670c`, these six paths were byte-identical to `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`. This is a historical snapshot, not a claim about current file identity. At feature branch HEAD `ed9e3b69c184de1b488c9726c660dacdef1635b2`, the five paths that still exist have different blobs and `include/papernx/interface_theme.hpp` is deleted; the source blobs match the table at code-bearing commit `faecf30eb4fb12fd143d3c1666d91f0b690b484c`. Their implementations remain fork-derived or adapted by Git history and code lineage; the palette was integrated into PaperNX's existing theme tokens. Preserve Refael Bar's credit for scanner/index/theme work and Refael Avramashvili's credit for updater/parser source, alongside existing PaperNX authorship and AGPL-3.0 notices. Changed hashes, refactoring, file movement or redesign do not establish independent authorship.”

The updater code commit `b9a22bc` adds missing-marker startup recovery and removes a stale `.swap` after successful backup restoration. `8c38513` tests queued rescans; `faecf30` avoids persisting a cancelled cache snapshot if cancellation arrives during record staging. The updater change does not eliminate the power-loss interval where the canonical NRO cannot launch; see [`UPDATER.md`](UPDATER.md). The targeted updater unit and local HTTPS tests passed 2/2; library unit and application tests passed 2/2 after `faecf30`; the Switch build/artifact check confirmed the `-lcurl` link and embedded CA bundle. This does not prove physical-console recovery behavior. Refael Bar's attribution remains appropriate because the scanner/index/theme implementations retain derivative lineage; it is not inferred from current blob identity alone.

## Current source provenance snapshot — `ffb6d7047b3874746ae5a120209ab608f2590e4c`

This is the current source-bearing feature HEAD for this refresh. The G2 commit changes comic reader code and tests, not the six provenance paths. The table records each current feature blob against the same pinned fork blob; file identity and source lineage remain separate claims.

| Path | Feature blob at `ffb6d70` | Pinned fork blob | Current finding and history evidence |
|---|---|---|---|
| `src/library_scan.inc` | `7e45b42f84fa84d3e20f2a4c209b0cc91d34b52b` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Different blob; `e791394` changed cancellation/rerun behavior. Scanner integration remains derived from Refael Bar's fork contribution; not a clean-room implementation. AGPL-3.0 credit remains appropriate. |
| `src/library_index.cpp` | `e991d669da322528accce694923c226b753bb06f` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Different blob; PaperNX `scanRoots()` integration and later cancellation-safe staging are real changes (`8204f77`, `e791394`, `faecf30`), while worker/inventory implementation retains fork lineage. Adapted AGPL-3.0 source; retain Refael Bar credit. |
| `include/papernx/library_index.hpp` | `bfbc9980a93d5f95e93f89e9a117f62e85d4b860` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Different blob; `e791394` added cancellation to the fork-derived scanner contract. Not an independent API design. Refael Bar / AGPL-3.0 attribution remains appropriate. |
| `src/self_update.cpp` | `afa55e04b9c04d6a7269d6b2f6ea0a15875136e8` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Different blob; PaperNX trust, TLS, transfer, install and startup-recovery changes (`913fb8f`, `b9a22bc`) are substantive, but the implementation descends from the fork updater. Classify as adapted AGPL-3.0 code and retain Refael Avramashvili credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Different blob; `9eb613a` replaced the generic value tree with a release-specific bounded parser. Material redesign, still fork-derived by history; retain Refael Avramashvili / AGPL-3.0 credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Deleted by `913fb8f`; palette resolution was integrated into PaperNX `include/papernx/theme.hpp`. File removal did not erase Refael Bar's source contribution or AGPL provenance. |

The historical statement that all six listed paths were byte-identical is accurate for `a35aa7b0f51aee58f44d524616a2aa55477d670c` only. At current source HEAD `ffb6d7047b3874746ae5a120209ab608f2590e4c`, all five paths that still exist have different blobs and the theme header is deleted. Their fork-derived lineage remains; neither changed blobs nor redesigns establish independent authorship. Refael Bar should continue to be credited for scanner/index/theme work, and Refael Avramashvili for updater/parser work, alongside existing PaperNX authorship and AGPL-3.0 notices.

The separate v0.9.1 files remain historically shared: at this snapshot `src/page_cache.cpp` is `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a` and `include/papernx/page_curl.hpp` is `e95f2d392b6e00a6d8d2b4c3d86b06b32b08d9ad`, matching the fork blobs. Both paths existed at the main comparison base and are not v0.9.2 imports. G2 did not change either file.

## Current provenance snapshot — G3 code commit `37d978f0f34ba86f0f1bda95088d061cae946723`

The six-path byte-identity statement remains historical: it was accurate at initial audited feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`, and does **not** describe current file identity. G3 did not edit any of those six paths. Their current feature blobs and pinned-fork blobs are:

| Path | Feature blob at `37d978f` | Pinned fork blob | Finding |
|---|---|---|---|
| `src/library_scan.inc` | `7e45b42f84fa84d3e20f2a4c209b0cc91d34b52b` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Different blob; cancellation/rescan changes do not erase fork-derived scanner integration. Refael Bar / AGPL-3.0 credit remains appropriate. |
| `src/library_index.cpp` | `e991d669da322528accce694923c226b753bb06f` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Different blob; PaperNX scan-root and cancellation changes coexist with fork-derived worker/inventory lineage. Not a clean-room implementation. |
| `include/papernx/library_index.hpp` | `bfbc9980a93d5f95e93f89e9a117f62e85d4b860` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Different blob; cancellation extends the fork-derived contract. Retain Refael Bar / AGPL-3.0 credit. |
| `src/self_update.cpp` | `afa55e04b9c04d6a7269d6b2f6ea0a15875136e8` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Different blob; PaperNX trust/install recovery work remains a fork-derived adaptation. Retain Refael Avramashvili / AGPL-3.0 credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Different blob; bounded parser redesign retains fork lineage. Retain Refael Avramashvili / AGPL-3.0 credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette code is integrated in PaperNX theme tokens; deletion does not erase Refael Bar's source contribution or AGPL-3.0 provenance. |

The earlier shared cache/curl distinction remains important. `src/page_cache.cpp` is still `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a`, unchanged from the prior snapshot. `include/papernx/page_curl.hpp` was shared v0.9.1 code already present at the main comparison base; its blob changed in G3 from `e95f2d392b6e00a6d8d2b4c3d86b06b32b08d9ad` to `abd656bb92e594e59588531a996d526184749234`. The G3 change adds an optional comic reverse-surface input to the existing spread renderer and preserves the full-color target endpoint. The paper surface/caption integration uses PaperNX's existing palette, snapshot and overlay code. This commit did not import fork source; it also does not relabel the pre-existing curl algorithm as independently authored or remove any prior attribution.

Behavioral coverage for the G3 extension is in `tests/reader_input_tests.cpp`, `tests/reader_spread_tests.cpp`, `tests/comic_app_tests.cpp`, and the normal/fallback `tests/page_curl_tests.cpp` targets. Those tests establish behavior, not copyright origin. No fork-v0.17.3–v0.17.5 module was copied into the G3 commit.

**Correct current wording:** “At feature HEAD `37d978f0f34ba86f0f1bda95088d061cae946723`, none of the five extant paths among the six originally listed is byte-identical to the pinned fork commit, and `include/papernx/interface_theme.hpp` is deleted. The original all-six byte-identity statement is true only for historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. The scanner/index/theme and updater/parser implementations retain their documented fork-derived/adapted lineage and Refael Bar / Refael Avramashvili AGPL-3.0 attribution. The page cache and page curl were shared v0.9.1 code already in the main comparison base; G3's small PageCurl API extension does not make the underlying algorithm independent or alter its provenance.”

## Current provenance snapshot — G4 code commit `81a3b8c389e7b79ee018343223719cba67992675`

G4 removes the PaperNX self-install transaction from the updater, but it does not erase the updater's fork-derived history or make the remaining parser/network code a clean-room implementation. The current blobs are:

| Path | G4 feature blob | Pinned fork blob | Current classification |
|---|---|---|---|
| `src/self_update.cpp` | `066ae641bdaf66c36cb23b6e2a65d4c9ea03cf22` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Different after PaperNX trust/staging/recovery work and G4 deletion of risky self-install. Remaining updater lineage is adapted/fork-derived; keep Refael Avramashvili and AGPL-3.0 credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Unchanged from prior snapshot, still fork-derived parser; not independently authored. Keep Refael Avramashvili / AGPL-3.0 credit. |
| `src/library_scan.inc` | `7e45b42f84fa84d3e20f2a4c209b0cc91d34b52b` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Unchanged in G4; fork-derived scanner integration, not clean-room. Keep Refael Bar / AGPL-3.0 credit. |
| `src/library_index.cpp` | `e991d669da322528accce694923c226b753bb06f` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Unchanged in G4; PaperNX extensions do not erase fork lineage. Keep Refael Bar / AGPL-3.0 credit. |
| `include/papernx/library_index.hpp` | `bfbc9980a93d5f95e93f89e9a117f62e85d4b860` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Unchanged in G4; fork-derived API. Keep Refael Bar / AGPL-3.0 credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette integration remains in PaperNX theme code; deletion is not removal of source credit. Keep Refael Bar / AGPL-3.0 credit. |

`src/page_cache.cpp` remains the prior shared v0.9.1 blob `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a`; `include/papernx/page_curl.hpp` remains the prior shared v0.9.1 code with G3 extension blob `abd656bb92e594e59588531a996d526184749234`. Neither is a G4 import or independently authored claim.

G4 updater behavior is tested in `tests/self_update_tests.cpp` and `tests/self_update_integration.py`; tests establish verified-manual staging and canonical/user-data preservation, not provenance. `tests/reader_input_tests.cpp` separately verifies touch and controller `+` share the existing menu state. Attribution and AGPL notices remain present; no old release history was edited.

**Correct current wording:** “At feature HEAD `81a3b8c389e7b79ee018343223719cba67992675`, the updater has been redesigned to stage a verified NRO for external manual replacement and no longer performs an in-app canonical-NRO swap. Its parser and remaining network/update lineage remain derived/adapted from the cited AGPL-3.0 fork contribution; Refael Avramashvili attribution remains appropriate. The scanner/index/theme remain fork-derived and attributed to Refael Bar. Historical byte identity remains true only at `a35aa7b`; changed blobs and deleted paths do not prove independent authorship. Page cache and curl remain shared v0.9.1 code, with G3's small curl API extension preserving that provenance.”

## Historical G5 provenance snapshot — `2411111bce1c23c3dde54d0b300e81eb42432ccd`

The original six-file byte-identity claim remains accurate at initial feature revision `a35aa7b0f51aee58f44d524616a2aa55477d670c` only. It does **not** describe later file identity. Blobs at this G5 source commit and lineage:

| Path | G5 feature blob | Pinned fork blob | Finding and attribution |
|---|---|---|---|
| `src/library_scan.inc` | `dbbbcb3ad4a0be96057bdff76d568a8f9e9dfb24` | `c8087dc4defa88af950bed947e6d50962bc7799f` | PaperNX publication now rejects incomplete scans and skips a repeated progress-file scan after publication. Fork-derived application integration remains adapted source. Preserve Refael Bar / AGPL-3.0 credit. |
| `src/library_index.cpp` | `6a9ba2b760cc7a933a395ba784c9fd09209a6d76` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Uses PaperNX `scanRoots()` and distinguishes incomplete traversal from cache-save failure. Worker/inventory lineage remains fork-derived/adapted, not clean-room. Preserve Refael Bar / AGPL-3.0 credit. |
| `include/papernx/library_index.hpp` | `61d0226cd5106fed5002820868b187eb81bb1980` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Adds scan completion to the fork-derived API; a changed blob does not establish independent authorship. Preserve Refael Bar / AGPL-3.0 credit. |
| `src/self_update.cpp` | `066ae641bdaf66c36cb23b6e2a65d4c9ea03cf22` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Unchanged from G4. Verified-manual staging is a PaperNX redesign, while updater lineage remains adapted/fork-derived. Preserve Refael Avramashvili / AGPL-3.0 credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Parser source is unchanged and remains derived/adapted. G5 added tests for duplicate keys in ignored nested objects and node limits; behavior tests do not prove origin. Preserve Refael Avramashvili / AGPL-3.0 credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette remains integrated into PaperNX theme tokens. Deletion does not erase fork origin. Preserve Refael Bar / AGPL-3.0 credit. |

G5 also modified PaperNX's pre-existing `src/core.cpp` walker: `scanRoots()` now reports traversal failures through an optional completion output and stores scanned paths/titles as UTF-8. This is a house-core change, not a copied fork module. `tests/library_index_tests.cpp` covers missing/partial roots, exact inventory-byte preservation through incomplete/cancelled scans, and Unicode inventory round-trip; `tests/modern_library_tests.cpp` verifies that an unavailable configured root does not replace the visible library. All seven targeted G5 CTests passed.

Attribution to Refael Bar for scanner/index/theme and Refael Avramashvili for updater/parser remains appropriate alongside PaperNX authorship. AGPL-3.0 notices remain required. No clean-room origin is claimed. The page cache/curl distinction remains the shared-v0.9.1 case documented above, not a G5 import.

**Wording valid at this historical snapshot:** “At feature HEAD `2411111bce1c23c3dde54d0b300e81eb42432ccd`, none of the five extant paths among the six originally listed is byte-identical to the pinned fork; `include/papernx/interface_theme.hpp` is deleted. The six-path byte-identity statement applies only to historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. Current scanner/index changes improve incomplete-scan safety and UTF-8 handling but retain fork-derived/adapted lineage; updater/parser lineage also remains derived/adapted. Preserve Refael Bar and Refael Avramashvili attribution with AGPL-3.0 notices. Changed blobs, tests, restructuring or redesign do not establish independent authorship.”

## Current provenance snapshot — G5 follow-up source commit `7aa78bc4da9714227a8ea3a9c48ae7d1552b881a`

The follow-up moved validation of progress-only book paths onto the scanner worker. It changed two of the six originally listed source files; the other file identities remain as shown in the previous snapshot or are deleted:

| Path | Current feature blob | Pinned fork blob | Current classification |
|---|---|---|---|
| `src/library_scan.inc` | `c740ad3397b3b3909312e20f0b97ae5d29dc36cd` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Passes historical progress-only paths to the async worker and avoids startup `stat` when scan-on-launch is enabled. This further adapts the fork-derived app integration; retain Refael Bar / AGPL-3.0 credit. |
| `src/library_index.cpp` | `25a792c02954f9da1b0441952f9cd2117f614971` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Worker validates candidates as supported regular files before retaining them. The worker/inventory implementation remains fork-derived/adapted, not clean-room. Retain Refael Bar / AGPL-3.0 credit. |
| `include/papernx/library_index.hpp` | `61d0226cd5106fed5002820868b187eb81bb1980` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Unchanged from the preceding G5 snapshot; its completion result extends a fork-derived API. Retain Refael Bar / AGPL-3.0 credit. |
| `src/self_update.cpp` | `066ae641bdaf66c36cb23b6e2a65d4c9ea03cf22` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Unchanged from G4; verified-manual staging is a PaperNX redesign but historical updater lineage remains adapted/fork-derived. Retain Refael Avramashvili / AGPL-3.0 credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Unchanged; bounded parser remains fork-derived/adapted. Retain Refael Avramashvili / AGPL-3.0 credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette stays integrated in PaperNX theme tokens. Retain Refael Bar / AGPL-3.0 origin attribution. |

`tests/modern_library_tests.cpp::runHistoryRestore` starts from a progress file and no inventory, asserts that `init()` returns with no synchronous historical book publication while the scan worker is pending, then verifies title, author, page and page count are restored. The full G5 targeted CTest selection passed 7/7 after this follow-up. This is functional evidence, not evidence of independent code origin.

**Correct current wording:** “At feature HEAD `7aa78bc4da9714227a8ea3a9c48ae7d1552b881a`, none of the five extant paths among the six originally listed is byte-identical to the pinned fork; `include/papernx/interface_theme.hpp` is deleted. The original six-path byte-identity statement is true only at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. Scanner/index and updater/parser implementations retain fork-derived/adapted lineage. Preserve Refael Bar's scanner/index/theme credit and Refael Avramashvili's updater/parser credit with AGPL-3.0 notices. Changed hashes, functional tests, restructuring or redesign do not establish independent authorship.”

## Current provenance snapshot — G8 source commit `c81145c9208455008db91bac56d9be4b920d8364`

G8 changed the existing PaperNX core walker and added a completion-boundary fix to the fork-derived library index worker. When a bounded scan reaches its limit while entries remain, or later configured roots cannot be reached because the aggregate cap is exhausted, `scanRoots()` now reports an incomplete snapshot. `LibraryScanner` consequently leaves the previous inventory untouched. The worker/inventory architecture remains derived/adapted; this is not a clean-room reimplementation.

| Path | Feature blob at G8 | Pinned fork blob | Finding |
|---|---|---|---|
| `src/library_scan.inc` | `c740ad3397b3b3909312e20f0b97ae5d29dc36cd` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Unchanged from G5; delegates to the shared core walker and preserves the user's visible inventory on incomplete results. Fork-derived PaperNX integration; retain Refael Bar / AGPL-3.0 credit. |
| `src/library_index.cpp` | `5319a7f6b66fd764b694be7fb7f770e4e9ebe5f7` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Adds incomplete-snapshot reporting on top of fork-derived worker/inventory code. Changed blob does not establish independent authorship; retain Refael Bar / AGPL-3.0 credit. |
| `include/papernx/library_index.hpp` | `d70a7c58985ec8f77e53e54c495d332401124a60` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Comment clarifies that `complete` means a full bounded snapshot. API lineage remains fork-derived; retain Refael Bar / AGPL-3.0 credit. |
| `src/self_update.cpp` | `066ae641bdaf66c36cb23b6e2a65d4c9ea03cf22` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Unchanged; verified-manual staging is a PaperNX redesign but updater lineage remains fork-derived/adapted. Retain Refael Avramashvili / AGPL-3.0 credit. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Unchanged bounded parser; still fork-derived/adapted. Retain Refael Avramashvili / AGPL-3.0 credit. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette was integrated into PaperNX theme tokens; deletion does not remove source attribution. Retain Refael Bar / AGPL-3.0 credit. |
| `src/page_cache.cpp` | `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a` | `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a` | Existing v0.9.1 shared code; not introduced during v0.9.2. No hash-only rewrite. |
| `include/papernx/page_curl.hpp` | `abd656bb92e594e59588531a996d526184749234` | `e95f2d392b6e00a6d8d2b4c3d86b06b32b08d9ad` | Existing v0.9.1 shared algorithm with the G3 optional comic reverse-surface API extension; the underlying algorithm is not independent. |

G8 also changed pre-existing `src/core.cpp` and `include/papernx/core.hpp`; this is PaperNX-owned core code, not an imported fork module. `tests/library_index_tests.cpp` covers a truncated single root, an exact-limit complete root, and an aggregate limit across roots. `tests/modern_library_tests.cpp` covers the async caller path; behavior tests establish the safety contract, not authorship.

The original six-path byte-identity statement remains true only at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`; it must not be worded as current identity. Refael Bar's scanner/index/theme credit, Refael Avramashvili's updater/parser credit, existing PaperNX authorship and AGPL-3.0 notices remain appropriate. No clean-room claim is made.

**Correct current wording:** “At feature HEAD `c81145c9208455008db91bac56d9be4b920d8364`, none of the five extant paths among the six originally listed is byte-identical to the pinned fork; `src/self_update.cpp` and `src/update_json.hpp` retain their G4/G5 project blobs, which already differ from the fork, and `include/papernx/interface_theme.hpp` is deleted. The original all-six identity statement applies only to historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. The scanner/index/theme and updater/parser lineage remains fork-derived/adapted; preserve Refael Bar and Refael Avramashvili attribution with AGPL-3.0 notices. `page_cache.cpp` and `page_curl.hpp` are prior v0.9.1 shared code, not v0.9.2 imports.”

## Current provenance snapshot — G9 updater error-output contract `ee429ec197a06a3287652985789e53abfcab178c`

G9 changes only the failure contract of `selectUpdateRelease()`: the caller-provided `UpdateRelease` is cleared before JSON or installed-version validation, so a failed call cannot leave a previous candidate in the output. This is a PaperNX hardening change within the existing updater; it does not change the updater's fork-derived ancestry or establish independent authorship.

| Path | Feature blob at G9 | Pinned fork blob | Finding |
|---|---|---|---|
| `src/self_update.cpp` | `1e5068f0d518e57c804e862c5ef5c91c39af6345` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | New PaperNX failure-output fix; the wider updater remains fork-derived/adapted. Retain Refael Avramashvili / AGPL-3.0 attribution. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Unchanged from G8; its release-specific bounded parser is a material redesign that retains fork lineage. Retain Refael Avramashvili / AGPL-3.0 attribution. |

The new assertions in `tests/self_update_tests.cpp` seed a prior release result, then verify that malformed JSON and an invalid installed version both leave the output empty. The focused `papernx_self_update_tests` CTest passed 1/1 after rebuilding `PaperNX` and the test target. The HTTPS integration target was not rerun for this small output-contract change. The historical six-path byte-identity statement still applies only to `a35aa7b0f51aee58f44d524616a2aa55477d670c`; none of the five extant paths is byte-identical to the pinned fork at G9, and `include/papernx/interface_theme.hpp` remains deleted. Preserve both fork contributor credits and AGPL-3.0 provenance.

## Current provenance snapshot — G10 scan depth/cancellation fix `1daedb63246384a73afb979978230f75716c2165`

G10 changes the existing PaperNX core walker so depth-limited and cancelled scans report an incomplete result. The library worker consequently preserves its prior inventory instead of saving a partial snapshot. This is a change to pre-existing PaperNX core code and its adapter integration; it does not rewrite any of the six originally audited paths.

| Path | Feature blob at G10 | Provenance |
|---|---|---|
| `src/core.cpp` | `1e107466b4e54c42b15136c82d28382b3d042361` | Pre-existing PaperNX core walker, extended to expose incomplete depth/cancellation outcomes; not a fork import. |
| `include/papernx/core.hpp` | `84671547db165325861b0fd027f7094e621fd32e` | Pre-existing PaperNX core API; `scan()` now has an optional completion output, with old call sites still compiling through its default. |
| `src/library_index.cpp` | `5319a7f6b66fd764b694be7fb7f770e4e9ebe5f7` | Unchanged from G8. The async worker still uses fork-derived/adapted architecture; Refael Bar / AGPL-3.0 attribution remains. |

`tests/library_index_tests.cpp` covers the depth boundary and verifies that a deeper incomplete scan leaves the previous inventory byte-identical. It also checks incomplete outcomes for cancellation in the single-root and multi-root APIs. The desktop targets `PaperNX`, `papernx_core_tests`, `papernx_library_index_tests`, `papernx_modern_library_tests` and `papernx_catalog_tests` built; focused CTest passed 4/4. A Switch `make all -j4` build produced the AArch64 ELF/NRO and validated RomFS CA/font inputs. No physical Switch behavior was tested.

**Correct wording at G10:** “At feature HEAD `1daedb63246384a73afb979978230f75716c2165`, the original six-path byte-identity statement remains true only at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. None of the five extant paths among those six is byte-identical to the pinned fork, and `include/papernx/interface_theme.hpp` is deleted. Scanner/index and updater/parser implementations retain fork-derived/adapted lineage; preserve Refael Bar and Refael Avramashvili attribution with AGPL-3.0 notices. `page_cache.cpp` and `page_curl.hpp` were shared v0.9.1 code already in the main base, not v0.9.2 imports. G10 changes only PaperNX-owned core scan behavior and adds no independent-origin claim.”

## Current provenance checkpoint — G1 comic worker and UI error handling (`d3265ddda21fd2ba5a1ba6fca0d72d95d8420e6b`)

The six originally audited paths were not changed after G10 or in G1. Their current feature and pinned-fork blobs at `d3265dd` are:

| Path at `a6e01b3` | Current feature blob | Pinned fork blob | Current byte identity |
|---|---|---|---|
| `src/library_scan.inc` | `c740ad3397b3b3909312e20f0b97ae5d29dc36cd` | `c8087dc4defa88af950bed947e6d50962bc7799f` | No |
| `src/library_index.cpp` | `5319a7f6b66fd764b694be7fb7f770e4e9ebe5f7` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | No |
| `include/papernx/library_index.hpp` | `d70a7c58985ec8f77e53e54c495d332401124a60` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | No |
| `src/self_update.cpp` | `1e5068f0d518e57c804e862c5ef5c91c39af6345` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | No |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | No |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | No |

All six were byte-identical at initial feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`; at current HEAD `d3265ddda21fd2ba5a1ba6fca0d72d95d8420e6b`, five extant files have different blobs and the interface-theme path is deleted. This is current file identity, not source independence. Scanner/index/theme and updater/parser still have fork-derived/adapted lineage; preserve Refael Bar and Refael Avramashvili attribution under AGPL-3.0 alongside existing PaperNX authorship.

G1 also touched other comic paths. Their histories show why path existence, blob equality and implementation origin must be reported separately:

| Path | v0.9.1 tag | Pinned fork / initial feature import | G1 blob at `d3265dd` | Provenance finding |
|---|---|---|---|---|
| `include/papernx/comic_pages.hpp` | Absent | Fork blob `14305d6c44b6960a222759e236926678091973f4`; initial feature blob `4845029d41e94b5c400e2eb59542c013f1ff3502` (different, but same `ComicPages` class and worker contract) | `38944ac7391356af88aa4266ef4cdf79cae2d9c1` | Added at feature import `a35aa7b`, with structural lineage to the fork worker; whitespace/restructuring and later queue/lifecycle changes do not establish clean-room origin. Treat as fork-derived/adapted and retain Refael Bar / AGPL-3.0 attribution. |
| `src/comic_document.c` | Present as `366dc97fb1b3aba7567e6b9dd205d476054ea48a` | Fork and initial feature blob both `1acbcf651d82956f33130bd01ad07e143c7e2b83` | `1911aa532002a6c333993f33fc198770532fa0b6` | The path existed in v0.9.1, but its v0.9.2 import replaced that blob with the pinned fork version. G1 only adds worker-visible counters to that descendant. Preserve fork contribution attribution; do not describe the current implementation as independently rewritten. |
| `include/papernx/mupdf_bridge.h` | Present as `b62800880e48db008cf637dbdd196191a01850bf` | Fork and initial feature blob both `d794ee94d4ecf88cd4d7a5b196127278131d5fcc` | `b89a6e380dd6a2872758efb8112b6ded54d512ad` | Existing v0.9.1 path adopted the fork revision at initial feature import; G1 adds the comic metrics API. This remains derived integration, not independent authorship. |
| `src/mupdf_bridge.c` | Present as `3ab20d43d90332a288c3e4b3bffe9f11c1b03343` | Fork blob `bd27e7b09714fae6fff5bf5a06c1b44a49ff4754`; initial feature blob `27a921687729e5c1ab625e557acb2ae0f46a2bb6` (different) | `021341827a2cc37a786a5ee0d48407a8786c61c2` | Existing PaperNX bridge extended around the fork-derived comic API; the G1 metrics wrapper is a PaperNX change. No clean-room claim is made for the larger comic integration. |

`src/main.cpp` also integrates the worker into PaperNX's SDL reader and owns texture creation/upload. G1 adds a visible-priority cold request path, continuous-comic worker rendering, retry handling and focused counters. The `ComicPages`/`ComicDocument` ancestry remains as described above; these changes do not erase or replace Refael Bar's credit. Focused behavior evidence is `tests/comic_performance_tests.cpp`, `tests/comic_app_tests.cpp`, `tests/reader_spread_tests.cpp` and `tests/comic_document_tests.cpp`; the current selected CTest passed 7/7 and the Switch ELF/NRO build passed. This evidence validates behavior, not independent source origin.

**Correct current wording:** “At initial audited feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`, the six named scanner/index/updater/theme paths were byte-identical to `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`; that is historical identity, not a claim that they remain byte-identical at current HEAD `d3265ddda21fd2ba5a1ba6fca0d72d95d8420e6b`. Five still-existing paths now have different blobs and `include/papernx/interface_theme.hpp` is deleted. The implementations remain fork-derived or adapted by history and code structure. Preserve Refael Bar's scanner/index/theme and comic-worker/archive credit, Refael Avramashvili's updater/parser credit, existing PaperNX authorship and AGPL-3.0 notices. `src/page_cache.cpp` and `include/papernx/page_curl.hpp` remain the separate prior-v0.9.1 shared-code case; do not conflate them with the comic worker/document files imported or updated in v0.9.2. File moves, changed blobs and tests do not establish independent authorship.”

Commit `d3265dd` changes only `src/main.cpp` error-state presentation and its app regression test. It does not change any of the six audited paths or the comic worker/archive source identities in the table above.

## Current provenance snapshot — updater release-schema hardening `50b9d60023c284fdadc28b92c23f524733920b08`

| Path | Feature blob at `50b9d60` | Pinned fork blob | Finding |
|---|---|---|---|
| `src/update_json.hpp` | `d36d44850270941d4a06cb38d7d038cec2691527` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Rejects mistyped `draft`/`prerelease` and limits the response to the configured 30 releases. This is a PaperNX security hardening change in the fork-derived/adapted parser; it is not an independent implementation. Preserve Refael Avramashvili credit and AGPL-3.0 provenance. |

`tests/self_update_tests.cpp` verifies both wrong-typed flags fail closed and the `per_page=30` boundary accepts exactly 30 while rejecting 31. The updater unit and local HTTPS integration CTest records passed **2/2** after rebuilding `PaperNX` and the test executable. This validates behavior at `50b9d60`, not source independence or Switch TLS/link behavior. The historical six-path byte-identity claim remains true only at `a35aa7b`; at `50b9d60` the five extant paths still have different blobs from the pinned fork and `include/papernx/interface_theme.hpp` remains deleted. Retain the previously documented Refael Bar and Refael Avramashvili credits with original PaperNX authorship and AGPL-3.0 notices.

## Current provenance snapshot — library inventory schema boundary `99839eb8e336a76649f433a11e7804263221661a`

| Path | Feature blob at `99839eb` | Pinned fork blob | Finding |
|---|---|---|---|
| `src/library_index.cpp` | `00b940e6ee65a0733c79d958d06a02bb66029e7b` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Adds a `PAPERNX-LIBRARY-1` schema marker to the existing ProgressStore-backed disposable inventory, accepts markerless legacy caches, and rejects a marked unknown version. This is a PaperNX-specific persistence boundary change within fork-derived/adapted scanner/index code; it is not a clean-room reimplementation. Retain Refael Bar credit and AGPL-3.0 provenance. |

`tests/library_index_tests.cpp` covers marker writing, markerless cache reads, future-schema rejection and reading-progress isolation; `tests/modern_library_tests.cpp` verifies the caller still restores the library and user position. The focused desktop CTest passed **2/2**. The other five current paths among the six historical provenance set are unchanged from the preceding snapshot; all six were byte-identical only at `a35aa7b`, and the interface-theme header remains deleted. This is behavioral evidence, not evidence of independent source origin.

## Current provenance snapshot — bounded updater asset retention `cb5eb7ac51d6681eab43928f6a7687140656d6cf`

| Path | Feature blob at `cb5eb7a` | Pinned fork blob | Finding |
|---|---|---|---|
| `src/update_json.hpp` | `dc81698b3684b21b1ce0c48d1791780b9b632163` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Retains only the two named updater assets per release; unrelated assets are discarded and duplicate install assets are unusable. This is a bounded PaperNX hardening change to the fork-derived/adapted parser, not an independent reimplementation. Retain Refael Avramashvili credit and AGPL-3.0 provenance. |

`tests/self_update_tests.cpp` parses 10,000 unrelated assets plus the two required assets and verifies only two records are retained and the update remains installable; a duplicate NRO asset is rejected for installation. Desktop `PaperNX` and updater test targets built, and unit plus local HTTPS CTest passed **2/2** in **1.67 seconds**. This validates parser behavior and retained-record bounds, not independent authorship or Switch TLS. The historical six-path byte-identity claim remains true only at `a35aa7b`; changes at `cb5eb7a` do not erase fork-derived lineage. Preserve the existing Refael Bar and Refael Avramashvili credits, PaperNX authorship and AGPL-3.0 notices.

## Six-file provenance re-audit — current source `cb5eb7ac51d6681eab43928f6a7687140656d6cf`

Each initial feature blob below matches the corresponding path at the cited fork commit exactly. At the current source commit, no extant path remains byte-identical; the interface-theme header has been deleted. Git history and retained structure still identify the changed scanner/index/updater files as fork-derived or adapted. Hash changes are not evidence of independent authorship.

| Path | Initial feature blob at `a35aa7b` | Current feature blob at `cb5eb7a` | Pinned fork blob | Current finding |
|---|---|---|---|---|
| `src/library_scan.inc` | `c8087dc4defa88af950bed947e6d50962bc7799f` | `c740ad3397b3b3909312e20f0b97ae5d29dc36cd` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Fork-derived/adapted app scan integration; last changed by `7aa78bc` to restore history on the worker. Retain Refael Bar / AGPL-3.0 attribution. |
| `src/library_index.cpp` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | `00b940e6ee65a0733c79d958d06a02bb66029e7b` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Fork-derived/adapted worker and inventory; PaperNX-specific incomplete-scan and schema boundaries do not make it clean-room. Last changed by `99839eb`. Retain Refael Bar / AGPL-3.0 attribution. |
| `include/papernx/library_index.hpp` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | `d70a7c58985ec8f77e53e54c495d332401124a60` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Fork-derived API with completion semantics clarified by `c81145c`. Retain Refael Bar / AGPL-3.0 attribution. |
| `src/self_update.cpp` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | `1e5068f0d518e57c804e862c5ef5c91c39af6345` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Substantially adapted for verified manual staging and PaperNX identity checks, but historically fork-derived. Retain Refael Avramashvili / AGPL-3.0 attribution. |
| `src/update_json.hpp` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | `dc81698b3684b21b1ce0c48d1791780b9b632163` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Fork-derived parser with PaperNX schema, allocation and metadata hardening. Retain Refael Avramashvili / AGPL-3.0 attribution. |
| `include/papernx/interface_theme.hpp` | `be58dc293354a4e38e31eb9faa08284367845c53` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette tokens were folded into PaperNX theme infrastructure in `913fb8f`; deletion does not remove the contribution's provenance. Retain Refael Bar / AGPL-3.0 attribution. |

**Correct statement:** “The six files were byte-identical to `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d` at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. At current source HEAD `cb5eb7ac51d6681eab43928f6a7687140656d6cf`, five of the six paths exist with different blobs and `include/papernx/interface_theme.hpp` is deleted. Their Git history and implementation structure remain fork-derived/adapted, so preserve Refael Bar's scanner/index/theme credit and Refael Avramashvili's updater/parser credit alongside PaperNX authorship and AGPL-3.0 notices. Do not describe them as independent implementations based on hash changes, refactoring or tests.”

## Current updater lineage after cancellation hardening — `ebf655526e44b44ee0c3f3f731ae8f8beef7d43d`

| Path | Current feature blob | Pinned fork blob | Finding |
|---|---|---|---|
| `src/self_update.cpp` | `26990ab0ebd4cb5ea48d85085f4cb03126249893` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Cancellation is observed through verification/staging and terminal publication is synchronized. This is a PaperNX-specific safety adaptation in historically fork-derived updater code; it is not independent. Preserve Refael Avramashvili / AGPL-3.0 provenance. |

At `ebf6555`, the other five paths retain the blobs in the preceding six-file snapshot: `src/library_scan.inc` `c740ad3397b3b3909312e20f0b97ae5d29dc36cd`, `src/library_index.cpp` `00b940e6ee65a0733c79d958d06a02bb66029e7b`, `include/papernx/library_index.hpp` `d70a7c58985ec8f77e53e54c495d332401124a60`, `src/update_json.hpp` `dc81698b3684b21b1ce0c48d1791780b9b632163`, and deleted `include/papernx/interface_theme.hpp`. No extant path is byte-identical to the pinned fork; all six historical import matches and all contributor notices remain as documented above.

## Current provenance snapshot — completed-scan cancellation guard `a9a014f790bd98541876f82947c36eb7b1f6782c`

| Path | Current feature blob | Pinned fork blob | Finding |
|---|---|---|---|
| `include/papernx/library_index.hpp` | `7ddb5b8a91fcc5d2306c9e4d766d74288e06b580` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | `cancel()` now ignores cancellation after a completed scan has published `ready_`, preventing a ready result from being discarded before `take()`. This is a PaperNX-specific lifecycle adaptation in the historically fork-derived API, not an independent implementation. Preserve Refael Bar credit and AGPL-3.0 provenance. |

The original six paths remain byte-identical to the pinned fork only at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. At `a9a014f`, the five extant paths have different blobs and `include/papernx/interface_theme.hpp` remains deleted. The cancellation test in `tests/library_index_tests.cpp` verifies behavior, not authorship. Existing Refael Bar and Refael Avramashvili attribution remains appropriate.

## Current provenance snapshot — scan result merge allocation `761f9b910045d7485a6904d6b96b6bc9d7340257`

| Path | Current feature blob | Pinned fork blob | Finding |
|---|---|---|---|
| `src/library_scan.inc` | `c6c950712c7b0d34431c8f3994bc4f960e136950` | `c8087dc4defa88af950bed947e6d50962bc7799f` | The SDL-thread result merge now stores pointers to existing `Book` records in its temporary lookup instead of copying complete records. This is a PaperNX-specific allocation/lifetime adjustment in the fork-derived application integration; it does not establish independent authorship. Preserve Refael Bar credit and AGPL-3.0 provenance. |

The original six-path byte-identity statement remains historical at `a35aa7b0f51aee58f44d524616a2aa55477d670c`; no extant path is byte-identical at `761f9b9`, and the interface-theme header remains deleted. The scanner tests verify behavior and data preservation, not source independence.

## Current code-bearing source snapshot — `96d0d799eb834b9b13f860d23cd3d746df433b96`

The `96d0d79` cover-queue change only touches `src/main.cpp` and its application test. The six-file historical claim remains true only at `a35aa7b`; current file identities are:

| Path | Current feature blob | Pinned fork blob | Current classification |
|---|---|---|---|
| `src/library_scan.inc` | `c6c950712c7b0d34431c8f3994bc4f960e136950` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Changed; PaperNX scan integration with fork-derived structure. Refael Bar / AGPL-3.0 credit remains. |
| `src/library_index.cpp` | `00b940e6ee65a0733c79d958d06a02bb66029e7b` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Changed; versioned `ProgressStore` inventory and safer traversal adapt the fork-derived worker/index. Refael Bar / AGPL-3.0 credit remains. |
| `include/papernx/library_index.hpp` | `7ddb5b8a91fcc5d2306c9e4d766d74288e06b580` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Changed; cancellation/result API adapts the fork-derived scanner contract. Refael Bar / AGPL-3.0 credit remains. |
| `src/self_update.cpp` | `26990ab0ebd4cb5ea48d85085f4cb03126249893` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Changed; verified manual staging and cancellation hardening retain fork-derived updater lineage. Refael Avramashvili / AGPL-3.0 credit remains. |
| `src/update_json.hpp` | `dc81698b3684b21b1ce0c48d1791780b9b632163` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Changed; bounded release-specific parser hardening retains fork-derived lineage. Refael Avramashvili / AGPL-3.0 credit remains. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette tokens were integrated into PaperNX theme infrastructure; deletion does not remove Refael Bar / AGPL-3.0 provenance. |
| `src/page_cache.cpp` | `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a` | `10b7df91eb4f9762b69a08573c25a7a5f74d9e3a` | The PageCache system existed in the main base; feature commit `a35aa7b` added protected-key LRU behavior that matches the Refael Bar fork blob. Existing v0.9.1 system plus fork-derived enhancement, not a wholesale new module or independent rewrite. |
| `include/papernx/page_curl.hpp` | `abd656bb92e594e59588531a996d526184749234` | `e95f2d392b6e00a6d8d2b4c3d86b06b32b08d9ad` | The curl system existed in the main base; the initial feature snapshot matched the fork blob, followed by feature-branch changes. Existing v0.9.1 system plus fork-derived changes, not a wholly new module or clean-room implementation. |

**Corrected statement:** “The six named v0.9.2 scanner/index/updater/theme files were byte-identical to `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d` at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. At current code-bearing feature HEAD `96d0d799eb834b9b13f860d23cd3d746df433b96`, the five extant paths have different blobs and `include/papernx/interface_theme.hpp` is deleted. Their Git history and current implementations remain fork-derived or adapted; preserve Refael Bar’s scanner/index/theme attribution and Refael Avramashvili’s updater/parser attribution alongside PaperNX authorship and AGPL-3.0 notices. `src/page_cache.cpp` and `include/papernx/page_curl.hpp` are the separate prior-v0.9.1 systems, with fork-derived v0.9.2 changes; do not classify them as new wholly introduced modules or clean-room rewrites. Changed hashes, redesigns, moves and tests do not establish independent authorship.”

## Current provenance snapshot — malformed inventory marker guard `6f030a2a50918cca1301ce6fa6d7a8d708d77895`

The original six-path byte-identity statement remains historical: it was true at feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. At current source commit `6f030a2`, five extant paths still have blobs different from the pinned fork and `include/papernx/interface_theme.hpp` remains deleted. The new inventory marker guard only changes `src/library_index.cpp`; it does not erase or reset that file's fork lineage.

| Path | Current feature blob at `6f030a2` | Pinned fork blob | Finding |
|---|---|---|---|
| `src/library_scan.inc` | `c6c950712c7b0d34431c8f3994bc4f960e136950` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Fork-derived/adapted PaperNX scan integration; retain Refael Bar / AGPL-3.0 attribution. |
| `src/library_index.cpp` | `e6335585607a0396791634c1e4491a8ed17aa959` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Fork-derived/adapted worker and inventory with PaperNX-specific traversal, schema and cancellation boundaries. The empty-marker rejection is a small safety fix, not an independent implementation. Retain Refael Bar / AGPL-3.0 attribution. |
| `include/papernx/library_index.hpp` | `7ddb5b8a91fcc5d2306c9e4d766d74288e06b580` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Fork-derived/adapted scan API; retain Refael Bar / AGPL-3.0 attribution. |
| `src/self_update.cpp` | `26990ab0ebd4cb5ea48d85085f4cb03126249893` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Adapted verified-manual updater with fork lineage; retain Refael Avramashvili / AGPL-3.0 attribution. |
| `src/update_json.hpp` | `dc81698b3684b21b1ce0c48d1791780b9b632163` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Adapted bounded release parser with fork lineage; retain Refael Avramashvili / AGPL-3.0 attribution. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette tokens were folded into PaperNX theme infrastructure; deletion does not remove Refael Bar / AGPL-3.0 provenance. |

**Current wording:** “These six files matched the pinned fork byte-for-byte at historical feature HEAD `a35aa7b0f51aee58f44d524616a2aa55477d670c`. At current source HEAD `6f030a2a50918cca1301ce6fa6d7a8d708d77895`, five extant paths have different blobs and the interface-theme header is deleted. Their Git history and current code retain fork-derived/adapted lineage; keep Refael Bar's scanner/index/theme attribution and Refael Avramashvili's updater/parser attribution alongside PaperNX authorship and AGPL-3.0 notices. Do not call these implementations independent based on later edits or changed hashes.”
