# PaperNX v0.9.1 — Verified test and validation results

Automated test gates executed and verified for PaperNX v0.9.1:

| Gate | Verified Result | Script / Target | Environment |
|---|---|---|---|
| Portable C++17 Core | 7/7 PASS | `cmake --build build-core && ctest` | Linux container (`ubuntu:24.04`) |
| Desktop Native Suite | 12/12 PASS | `scripts/verify-desktop.sh` | Linux container (`papernx-desktop`) |
| Desktop Release Mode | 12/12 PASS | `cmake --build build-desktop -DCMAKE_BUILD_TYPE=Release` | Linux container (`papernx-desktop`) |
| ASan/UBSan Sanitizers | 12/12 PASS (0 leaks, 0 UB, 142s) | `scripts/verify-sanitizers.sh` | Linux container (`papernx-desktop`) |
| Large Document & Fixtures | PASS (EPUB/PDF/CBZ) | `tests/v04_catalog_workflow.hpp`, `v03_stress_fixtures` | Desktop SDL software runner |
| AArch64 NRO / NACP / RomFS | PASS (0 bytes delta vs v0.9.0) | `scripts/verify-switch.sh` | Docker devkitPro AArch64 container |
| Package & License Verification | PASS | `scripts/verify_distribution.py` | Python 3 packaging validator |
| Physical Nintendo Switch acceptance | **UNVERIFIED** (35/35 checks) | [35-item checklist](DEVICE_TEST_PLAN.md) | Physical hardware acceptance pending |

### Verification Evidence Summary (v0.9.1)
- **Native & Sanitizers:** Total 12 test suites executed natively on Linux container with AddressSanitizer and UndefinedBehaviorSanitizer active: `core_tests`, `catalog_tests`, `sanitizer_smoke_tests`, `utf8_tests`, `mupdf_render_tests`, `large_document_tests`, `gesture_tests`, `cache_eviction_tests`, `continuous_scroll_tests`, `app_state_tests`, `display_profile_tests`, `v04_ui_workflow`. All passed with zero memory leaks and zero runtime undefined behaviors.
- **Switch Cross-Compilation:** Cross-compiled targeting Nintendo Switch AArch64 (`-mcpu=cortex-a57+fp+simd`, PIE, stripped). Output binary `PaperNX.nro` size: 48,090,067 bytes.
- **Physical Hardware Status:** In accordance with our verification policy, real-device execution on physical Nintendo Switch hardware remains **UNVERIFIED**. No simulated passes are claimed as physical hardware tests.

See [Release Report](RELEASE_REPORT.md), [Switch Build Guide](BUILD_SWITCH.md), and [Known Limitations](KNOWN_ISSUES.md).
