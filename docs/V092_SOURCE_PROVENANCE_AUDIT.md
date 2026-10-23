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
