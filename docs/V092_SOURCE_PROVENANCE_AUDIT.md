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

## Current feature branch provenance — `faecf30eb4fb12fd143d3c1666d91f0b690b484c`

The current branch HEAD is `faecf30`; it follows the queued-rescan test commit `8c38513` and adds cancellation checks while staging inventory records. The historical six-path identity statement remains limited to `a35aa7b`. At current feature HEAD `faecf30`, no existing path is byte-identical to the pinned fork: five have different blobs, and `include/papernx/interface_theme.hpp` is deleted.

| Path | Current HEAD blob | Fork blob | Current origin classification |
|---|---|---|---|
| `src/library_scan.inc` | `7e45b42f84fa84d3e20f2a4c209b0cc91d34b52b` | `c8087dc4defa88af950bed947e6d50962bc7799f` | Fork-derived application integration, extended for cancellation and queued rescans; not independent. Refael Bar / AGPL-3.0 credit retained. |
| `src/library_index.cpp` | `e991d669da322528accce694923c226b753bb06f` | `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Core traversal reuses PaperNX `scanRoots()`; cancellation is checked while staging cache records; index/worker behavior remains fork-derived. Refael Bar / AGPL-3.0 credit retained. |
| `include/papernx/library_index.hpp` | `bfbc9980a93d5f95e93f89e9a117f62e85d4b860` | `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Fork-derived scanner contract with a cancellation result/API; changed blob is not independent authorship. Refael Bar / AGPL-3.0 credit retained. |
| `src/self_update.cpp` | `afa55e04b9c04d6a7269d6b2f6ea0a15875136e8` | `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | PaperNX-specific updater redesign plus missing-marker recovery; still fork-derived by source history. Refael Avramashvili / AGPL-3.0 credit retained. |
| `src/update_json.hpp` | `677a6931a6fbbf583ba8388fff217dcaef5b091b` | `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Bounded release parser redesign retaining fork lineage. Refael Avramashvili / AGPL-3.0 credit retained. |
| `include/papernx/interface_theme.hpp` | Deleted | `be58dc293354a4e38e31eb9faa08284367845c53` | Palette was integrated into existing PaperNX theme tokens; Refael Bar / AGPL-3.0 origin remains documented. |

**Current corrected wording:** “At the initial audited feature revision `a35aa7b0f51aee58f44d524616a2aa55477d670c`, these six paths were byte-identical to `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`. This is a historical snapshot, not a claim about current file identity. At feature HEAD `faecf30eb4fb12fd143d3c1666d91f0b690b484c`, the five paths that still exist have different blobs and `include/papernx/interface_theme.hpp` is deleted. Their implementations remain fork-derived or adapted by Git history and code lineage; the palette was integrated into PaperNX's existing theme tokens. Preserve Refael Bar's credit for scanner/index/theme work and Refael Avramashvili's credit for updater/parser source, alongside existing PaperNX authorship and AGPL-3.0 notices. Changed hashes, refactoring, file movement or redesign do not establish independent authorship.”

The updater code commit `b9a22bc` adds missing-marker startup recovery and removes a stale `.swap` after successful backup restoration. `8c38513` tests queued rescans; `faecf30` avoids persisting a cancelled cache snapshot if cancellation arrives during record staging. The updater change does not eliminate the power-loss interval where the canonical NRO cannot launch; see [`UPDATER.md`](UPDATER.md). The targeted updater unit and local HTTPS tests passed 2/2; library unit and application tests passed 2/2 after `faecf30`; the Switch build/artifact check confirmed the `-lcurl` link and embedded CA bundle. This does not prove physical-console recovery behavior. Refael Bar's attribution remains appropriate because the scanner/index/theme implementations retain derivative lineage; it is not inferred from current blob identity alone.
