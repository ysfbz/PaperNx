# PaperNX v0.9.1 — Verified test and validation results

Automated test gates executed and verified for PaperNX v0.9.1:

| Gate | Verified Result | Script / Target | Environment |
|---|---|---|---|
| Portable C++17 Core | 7/7 PASS | `cmake --build build-core && ctest` | Linux container (`ubuntu:24.04`) |
| Desktop Native Suite | 19/19 PASS | `scripts/verify-desktop.sh` | Linux container (`papernx-desktop`) |
| Desktop Pinned MuPDF 1.23.11 | 19/19 PASS | `scripts/verify-pinned-desktop.sh` | Linux container (`papernx-desktop`) |
| Desktop Release Mode | 7/7 PASS | `cmake --build build-core-release -DCMAKE_BUILD_TYPE=Release && ctest` | Linux container (`papernx-desktop`) |
| ASan/UBSan Sanitizers | 19/19 PASS (0 leaks, 0 UB, 653s) | `scripts/verify-sanitizers.sh` | Linux container (`papernx-desktop`) |
| Large Document & Fixtures | PASS (EPUB/PDF/CBZ/CBR) | `tests/comic_app_tests.cpp`, `page_curl_app_tests.cpp`, `v03_stress_fixtures` | Desktop SDL runner |
| AArch64 NRO / NACP / RomFS | PASS (NRO: 48,786,387 bytes) | `scripts/verify-switch.sh` | Docker devkitPro AArch64 container |
| Package & License Verification | PASS | `scripts/verify_distribution.py` | Python 3 packaging validator |
| Physical Nintendo Switch acceptance | **UNVERIFIED** (38/38 checks) | [38-item checklist](DEVICE_TEST_PLAN.md) | Physical hardware acceptance pending |

### Verification Evidence Summary (v0.9.1)
- **Native & Sanitizers:** Total 19 test suites executed natively on Linux container with AddressSanitizer and UndefinedBehaviorSanitizer active: `papernx_catalog_tests`, `papernx_foundations_tests`, `papernx_localization_tests`, `papernx_cover_tests`, `papernx_core_tests`, `papernx_reading_journal_tests`, `papernx_utf8_integrity`, `papernx_library_controls_tests`, `papernx_comic_app_tests`, `papernx_page_navigation_tests`, `papernx_comic_document_tests`, `papernx_page_curl_tests`, `papernx_page_curl_fallback_tests`, `papernx_page_curl_app_tests`, `papernx_ui_component_tests`, `generate_cbz_fixture`, `mupdf_render_tests`, `v03_stress_fixtures`, `papernx_app_tests`. All passed with zero memory leaks and zero runtime undefined behaviors.
- **Switch Cross-Compilation:** Cross-compiled targeting Nintendo Switch AArch64 (`-march=armv8-a+crc+crypto -mtune=cortex-a57`, PIE). Output binary `PaperNX.nro` size: 48,786,387 bytes, SHA256: `f411daf2e02f5483ab49563295ea7832ca3e9b0455f433b7c8bfe6db909fced1`.
- **Physical Hardware Status:** In accordance with our verification policy, real-device execution on physical Nintendo Switch hardware remains **UNVERIFIED**. No simulated passes are claimed as physical hardware tests.

See [Release Report](RELEASE_REPORT.md), [Switch Build Guide](BUILD_SWITCH.md), and [Known Limitations](KNOWN_ISSUES.md).

Documentation-only reconciliation: the linked hardware checklist contains **38** numbered scenarios (not 35). This corrects reporting only; none of the 38 has been promoted to a verified physical-device PASS by this update. Existing desktop and sanitizer outcomes above are previously recorded results, not tests rerun as part of documentation editing.
