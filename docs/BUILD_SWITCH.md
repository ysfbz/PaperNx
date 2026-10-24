# PaperNX v0.9.3 — Nintendo Switch build and installation

This guide uses the checked-in [Dockerfile](../Dockerfile), [Compose service](../compose.yaml), `Makefile`, and `scripts/verify-switch.sh`. The Switch target is AArch64/libnx; desktop builds are for development and automated checks.

## Build and package

Requirements: Docker with Compose, Git, network access for the configured devkitPro packages, and enough disk space for the pinned MuPDF build and artifacts.

```bash
git clone https://github.com/ysfbz/PaperNx.git
cd PaperNx
docker compose build switch
docker compose run --rm -e PAPERNX_RELEASE_VERSION=v0.9.3 switch bash -lc 'scripts/verify-switch.sh'
```

The default release version is `v0.9.3`. Set `PAPERNX_BUILD_ONLY=1` to build and verify without packaging. A full release run creates `dist/v0.9.3/` with `PaperNX.nro`, its SHA-256 sidecar, the SD ZIP and its sidecar, the separate license ZIP and its sidecar, and `manifest.json`. The receipt is written under `verification/switch-build-receipt.json`. `PAPERNX_LICENSES_ROOT` can point to an isolated staging directory for collected dependency notices.

The script verifies the MuPDF source checksum, the AArch64 ELF/NRO, NACP identity/version, embedded icon, RomFS fonts and CA bundle, package contents, source/build receipt, archive integrity, license notices and checksums. This is a cross-build and package check; it does not prove behavior on a physical console.

## NACP and embedded assets

The Switch Makefile defines title `PaperNX`, author `Yusuf Buzrul`, version `0.9.3`, the project icon and `romfs/`. RomFS includes Noto Sans Regular/SemiBold, Noto Serif, `OFL.txt` and `cacert.pem` used for updater TLS verification. The Switch build links libcurl using the libnx TLS backend.

## Install and upgrade

Extract `PaperNX-switch-v0.9.3.zip` to the microSD root (`sdmc:/`). The archive installs:

```text
switch/PaperNX/PaperNX.nro
switch/PaperNX/books/                 (empty placeholder)
switch/PaperNX/fonts/                 (bundled fonts and OFL notice)
```

Place books in `sdmc:/switch/PaperNX/books/` or use the in-app file browser. Before upgrading, back up `books/` and `data/`, close PaperNX, and replace the NRO with the release copy. Keep the previous NRO until the new version has launched successfully. Neither the install ZIP nor the updater writes books, settings, progress, bookmarks or Reading Journal data.

The in-app updater stages a validated candidate but does not install it or restart into it. See [UPDATER.md](UPDATER.md) for the manual flow and recovery limits. The separate `PaperNX-licenses-v0.9.3.zip` contains project attribution and collected dependency notices.

## Desktop checks

```bash
docker compose run --rm desktop bash -lc 'scripts/verify-desktop.sh'
docker compose run --rm desktop bash -lc 'scripts/verify-sanitizers.sh'
```

Automated desktop and cross-build checks do not substitute for direct testing on physical Nintendo Switch hardware, including Joy-Con input, touch accuracy, sleep/resume, and SD-card recovery. See [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for release limits.
