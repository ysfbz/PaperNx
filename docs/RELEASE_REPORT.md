# PaperNX v0.9.1 — Release verification report

**Target Release:** [v0.9.1](https://github.com/ysfbz/PaperNx/releases/tag/v0.9.1), 20 October 2026  
**Application / author:** PaperNX / Yusuf Buzrul  
**Distribution:** Nintendo Switch AArch64 homebrew. Linux/desktop targets exist for development and automated validation.

## Release artifacts (v0.9.1 Re-Release)

| File | Bytes | SHA256 (Artifact Digest) |
|---|---:|---|
| [`PaperNX.nro`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX.nro) | 48,786,387 | `f411daf2e02f5483ab49563295ea7832ca3e9b0455f433b7c8bfe6db909fced1` |
| [`PaperNX-switch-v0.9.1.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX-switch-v0.9.1.zip) | 33,641,244 | `898a1f07e4898c164a3e57f17e7af52d7eda553c92f85892aac9bda8eaf249e9` |
| [`PaperNX-licenses-v0.9.1.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX-licenses-v0.9.1.zip) | 207,462 | `2890920a31aac7293e92c45ae90ef5d18e274df844daf74ef42db3539df8f005` |

Three corresponding `.sha256` sidecar assets are generated alongside the binaries. Hashes were calculated and independently verified against local build outputs and distribution packages.

## Release Evolution & Digest History

| Artifact | Early v0.9.1 Build | Final v0.9.1 Re-Release (Verified) |
|---|---|---|
| `PaperNX.nro` | `2e376f54500497d3fe8e06319af53238dd34c2d01a774df40c97747213e530b5` | `f411daf2e02f5483ab49563295ea7832ca3e9b0455f433b7c8bfe6db909fced1` |
| `PaperNX-switch-v0.9.1.zip` | `70218c45812abb981048ec005f090c5a99e1ab01b34c04b8e21e79ec40e36dfa` | `898a1f07e4898c164a3e57f17e7af52d7eda553c92f85892aac9bda8eaf249e9` |
| `PaperNX-licenses-v0.9.1.zip` | `9c4c0d1f2042479c4723cbbaa3a83d0ce9c0584180a53d94127c575170fff859` | `2890920a31aac7293e92c45ae90ef5d18e274df844daf74ef42db3539df8f005` |

## Historical release artifacts (v0.9.0)

Published 19 October 2026 under git tag `v0.9.0`:
- `PaperNX.nro`: 48,090,067 bytes, SHA256: `9020d67962b5dda01b7da2405a5d023631e47e3a4e3be96662e4a48ad292b9ef`
- `PaperNX-switch-v0.9.0.zip`: 33,261,595 bytes, SHA256: `99b8ef910277a67992993112c73d0e5c2bfef651e481dfd97581a036baa18bd0`
- `PaperNX-licenses-v0.9.0.zip`: 190,942 bytes, SHA256: `9c4c0d1f2042479c4723cbbaa3a83d0ce9c0584180a53d94127c575170fff859`

## Build and automated quality gates (v0.9.1)

Fresh test executions in local Docker containers yielded the following results:

| Check | Result | Evidence / Command |
|---|---|---|
| Portable C++17 Core | PASS, 7/7 | `build-core` CTest test suite |
| Native SDL2/MuPDF | PASS, 19/19 | `scripts/verify-desktop.sh` |
| Pinned MuPDF 1.23.11 | PASS, 19/19 | `scripts/verify-pinned-desktop.sh` |
| Native ASan/UBSan + Leak Check | PASS, 19/19 | `scripts/verify-sanitizers.sh` |
| Core Release Suite | PASS, 7/7 | `build-core-release` CTest test suite |
| Switch AArch64 ELF, NRO, NACP, Icon, RomFS | PASS | `scripts/verify-switch.sh` (`PaperNX.elf`, `PaperNX.nro`) |
| Minimal ZIP & Separate License ZIP | PASS | `scripts/verify_distribution.py` |
| Physical Nintendo Switch (35 checks) | **UNVERIFIED** | [Device test plan](DEVICE_TEST_PLAN.md) |

The repository [Makefile](../Makefile) specifies NACP title `PaperNX`, author `Yusuf Buzrul`, version `0.9.1` and icon `assets/branding/PaperNX-icon-switch.jpg`. The verifier checks AArch64 ELF/NRO0 and embedded RomFS fonts (NotoSans Regular/SemiBold, NotoSerif and OFL notice). The Switch ZIP ships `switch/PaperNX/PaperNX.nro`, bundled fonts and an empty `books/` placeholder; the license notices are a **separate ZIP**. No user `data/` or `logs/` is installed, eliminating user data overwrite risk.

**Physical acceptance:** Real Switch operation, touch/portrait, sleep/resume, controller reconnection and media failure remain **UNVERIFIED**. See [BUILD_SWITCH.md](BUILD_SWITCH.md), [TEST_RESULTS.md](TEST_RESULTS.md), and [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
