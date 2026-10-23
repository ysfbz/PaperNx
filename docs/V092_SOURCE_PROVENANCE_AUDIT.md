# PaperNX v0.9.2 source provenance audit

**Audit date:** 2026-10-23  
**Feature branch at audit start:** `feature/v0.9.2-complete-reader-enhancements`  
**Starting HEAD:** `a35aa7b0f51aee58f44d524616a2aa55477d670c`  
**Comparison base:** `main@8db85384a1b6c277c74766601ef46e9a672328df`  
**Fork reference:** `rtk19/PaperNx@d2b4bcfd10b1a66043c5b8cd36da08340cffec6d`

## Method

The audit compared each Git blob at the feature HEAD with the fork reference, checked whether the path existed at the comparison base, inspected the path's Git history, and read its production callers and focused tests. A matching blob establishes byte identity at those revisions; it does not, by itself, establish the authorship or license of every earlier line. AGPL-derived or shared code is therefore not called independently implemented.

## New v0.9.2 modules that matched the fork

All six paths below were absent from `main` at the comparison base and had exactly the same Git blob at the feature HEAD and fork reference. Their common origin is the fork history; the table records the exact introducing history visible in this repository. These are **adapted fork code under the project's AGPL-3.0 terms**, not independent implementations. The feature branch already documents the fork and contributor Refael Bar in `docs/LICENSES.md`; this audit adds path-level attribution so these additions are not hidden under a general statement.

| Path | Feature history / feature blob | Initial design decision | Behavior contract and focused test |
|---|---|---|---|
| `src/library_scan.inc` | `9bb1c95`; `c8087dc4defa88af950bed947e6d50962bc7799f` | Rework the application integration around the existing book catalog and settings; retain the UI-thread publication boundary and do not discard progress for missing roots. | Multi-root import/removal, active-reader continuity, duplicate/nested roots and UI publication in `tests/modern_library_tests.cpp`; scan persistence/cancellation in `tests/library_index_tests.cpp`. |
| `src/library_index.cpp` | `9bb1c95`, adjusted at `699ef34`; `e4b3e83e2ddadaa55913b56f7a0a0e31b3f5dd1a` | Replace the lossy `ProgressStore` field overloading for disposable inventory with an explicit inventory record, bounded traversal and cancellation-safe publication. | Cache reload, path changes, roots, cancellation, damaged inventory and preservation of independent progress in `tests/library_index_tests.cpp`. |
| `include/papernx/library_index.hpp` | `9bb1c95`; `8a0ac82707cf5b402d0cbcebd790d751758b0003` | Keep a small scanner result contract in the existing core layer; remove interfaces not needed by the application. | Compile-time/public contract exercised by `tests/library_index_tests.cpp` and the application integration test. |
| `src/self_update.cpp` | `6cec7a7`; `e7e5c52c1b4b2137d92d1c6902a011d1d3b71b6c` | Redesign around fixed PaperNX release trust, TLS CA material, explicit user approval/cancellation, validated staging and recoverable installation. Do not claim rollback until a new boot is confirmed. | SemVer, release selection, NRO/hash validation, cancellation and interrupted-install behavior in `tests/self_update_tests.cpp`. |
| `src/update_json.hpp` | `6cec7a7`; `d4e3bb96dcb2d363e23d40a83775c02768fc5394` | Keep parsing bounded to the small release response contract and test malformed/truncated input; avoid a second general-purpose JSON framework. | Malformed, escaped, truncated, oversized and valid release JSON cases in `tests/self_update_tests.cpp`. |
| `include/papernx/interface_theme.hpp` | `9bb1c95`; `be58dc293354a4e38e31eb9faa08284367845c53` | Fold interface colors into PaperNX's existing `ThemePalette` source so reading and interface palettes share one token owner; keep Light, Dark, Follow-reading and seven accents. | Palette selection, bounds, contrast and shared screen use in `tests/modern_library_tests.cpp` and `tests/ui_component_tests.cpp`. |

The listed decisions are the review targets for the implementation pass. Their final status and resulting blob IDs will be updated after changes and tests; a decision is not evidence that the work is complete.

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

Update this section after implementation with the final blob IDs, whether each new module remains an AGPL adaptation or was materially redesigned, the tests actually run and their results, and any remaining unverified platform behavior. Do not describe a superficial rewrite as independent work.

### 2026-10-23 final hardening pass

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
