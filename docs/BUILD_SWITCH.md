# PaperNX v0.9.1 — Nintendo Switch build and installation

This guide follows the checked-in [Dockerfile](../Dockerfile), [Compose service](../compose.yaml), `scripts/verify-switch.sh`, `Makefile` and `scripts/package_release.py`.

## Requirements

Windows 11 with Docker Desktop (WSL 2 backend, Linux containers) or Linux with Docker Engine/Compose; Git; available disk space and network access for devkitPro packages and pinned MuPDF 1.23.11 source. Container volumes map the repository root to `/work`.

## Build from Windows Terminal / PowerShell or Linux shell

```bash
git clone https://github.com/ysfbz/PaperNx.git
cd PaperNx
docker compose build switch
docker compose run --rm switch
```

The `switch` service's default command runs `bash scripts/verify-switch.sh`. Explicitly equivalent after building the image:

```bash
docker compose run --rm switch bash scripts/verify-switch.sh
```

The script validates/downloads MuPDF 1.23.11 with pinned source SHA256 `478f2a167feae2a291c8b8bc5205f2ce2f09f09b574a6eb0525bfad95a3cfe66`, verifies/reuses the AArch64 prefix, captures source/build receipts, runs `make all -B -j4`, inspects ELF/NRO/NACP/branding/RomFS, collects SDK/MuPDF license notices, packages `dist/v0.9.1/` and verifies ZIP integrity.

The Dockerfile pins its base-image digest (`devkitpro/devkita64:20240915`).

## NACP and assets

[Makefile](../Makefile) defines `APP_TITLE = PaperNX`, `APP_AUTHOR = Yusuf Buzrul`, `APP_VERSION = 0.9.1`, `ICON = assets/branding/PaperNX-icon-switch.jpg` and `ROMFS = romfs`. The RomFS bundles Noto Sans Regular/SemiBold, Noto Serif and its OFL notice.

## Distribution files

`dist/v0.9.1/` contains `PaperNX.nro`, `PaperNX-switch-v0.9.1.zip`, `PaperNX-licenses-v0.9.1.zip`, three `.sha256` files and `manifest.json` (local receipt). The microSD ZIP contains:

```text
switch/
  PaperNX/
    PaperNX.nro
    books/                  (empty)
    fonts/
      NotoSans-Regular.ttf
      NotoSans-SemiBold.ttf
      NotoSerif-Regular.ttf
      OFL.txt
```

Extract the Switch ZIP to the microSD root (`sdmc:/`), put EPUB, PDF, and CBZ comic books in `sdmc:/switch/PaperNX/books/`, and open PaperNX from Homebrew Menu. On upgrade, back up `books/` and `data/` first. The distribution does not overwrite existing user databases or bookmarks. License/SDK notices ship separately in `PaperNX-licenses-v0.9.1.zip`.

## Optional desktop regression runs

```bash
docker compose build desktop
docker compose run --rm desktop bash scripts/verify-desktop.sh
docker compose run --rm desktop bash scripts/verify-pinned-desktop.sh
docker compose run --rm desktop bash scripts/verify-v04.sh native
docker compose run --rm desktop bash scripts/verify-v04.sh sanitizers
```

Desktop tests are not a substitute for [35 real-device checks](DEVICE_TEST_PLAN.md). See [release artifact hashes](RELEASE_REPORT.md).
