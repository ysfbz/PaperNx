<p align="center">
  <img src="assets/branding/PaperNX-icon-512.png" alt="PaperNX Logo" width="160">
</p>

<h1 align="center">PaperNX</h1>

<p align="center">
  <strong>Native, offline EPUB, PDF, CBZ and CBR reader for the Nintendo Switch.</strong>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-AGPL_3.0-blue.svg" alt="AGPL-3.0 License"></a>
  <a href="https://github.com/ysfbz/PaperNx/releases/tag/v0.9.1"><img src="https://img.shields.io/badge/Release-v0.9.1-brightgreen.svg" alt="Release v0.9.1"></a>
  <img src="https://img.shields.io/badge/Platform-Nintendo_Switch-e60012.svg" alt="Platform Nintendo Switch">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20C11-00599C.svg" alt="Language C++17 / C11">
  <img src="https://img.shields.io/badge/GUI-SDL2-black.svg" alt="GUI SDL2">
  <img src="https://img.shields.io/badge/Engine-MuPDF_1.23.11-red.svg" alt="Engine MuPDF">
</p>

---

## Overview

**PaperNX** is a native, lightweight, and fully offline document and comic reader built specifically for the Nintendo Switch using modern C++17, C11, SDL2, and MuPDF. Designed from the ground up for comfortable handheld reading with Joy-Con controllers detached, PaperNX delivers a fluid touchscreen-first reading experience.

All books, comic archives, cover art caches, reading progress, and personal reading journal statistics stay strictly on your microSD card. No user account, no cloud telemetry, and no browser engines are required.

> [!NOTE]
> **Default Language:** PaperNX starts in **English** by default. Full **Turkish** localization is built-in and can be selected at any time via **Settings → Application → Language** (`+` button or <kbd>Y</kbd>). Your choice is saved and persists across app restarts.

---

## Key Features

- **Touchscreen-First Ergonomics:** Every navigation and reading action has a direct, responsive touch target (56px minimum touch targets with visual feedback). Joy-Con and Pro Controller buttons remain supported as optional shortcuts.
- **CBZ & CBR Comic Book Support:** Built-in reader engine for `.cbz` (ZIP) and `.cbr` (RAR4, RAR5, and solid archives via vendored `unarr`) comic book archives. Renders high-fidelity original RGB imagery without destructive paper/ink recoloring or synthetic textures, with optional reversible reading palette. Magic-byte container detection gracefully handles misnamed archives.
- **Interactive 3D Page Curl:** Authentic, physics-driven developable cylinder page curl (`PageCurlRenderer`) tracking finger position, grip height (`anchorY`), vertical drag, paper underside tint, specular highlights, and contact shadows. Works for paginated EPUB, PDF, CBZ, and CBR (including RTL manga and zoomed comic sheets), toggleable via **Settings → Navigation**.
- **Comic Series Grouping & Multi-Tab Navigation:** Automatically collapses multi-issue comic series (`#1`, `01`, `v01`, `Vol 01`, `ComicInfo.xml`) into unified series shelf cards. 3-tab contents modal (`Chapters`, `All Pages` grid, `Bookmarks`) with background thumbnail rendering, and an interactive footer scrubber with live preview bubble.
- **Continuous Vertical EPUB Scrolling:** Smooth pixel-by-pixel vertical scrolling with momentum, sensitivity adjustments, and seamless chapter transitions with logical anchor tracking. Traditional paginated horizontal reading mode is also fully supported.
- **Robust PDF Engine:** Powered by MuPDF 1.23.11 with fit-to-width, fit-to-page, touch drag-to-pan, zoom, and non-destructive document color filters (Original, Dark, Sepia, Invert).
- **Overflow-Safe Natural Sorting:** Deterministic numeric ordering for book titles, volumes, authors, and filenames (e.g., Volume 2 before Volume 10) regardless of number length.
- **Display Orientation & Controller Ergonomics:** Left and right analog sticks have independent deadzones and repeat rates. Trigger axes (ZL/ZR) provide smooth continuous zooming, while analog panning dynamically rotates to match portrait screen orientations. Visible on-screen "Fit page" pill restores 1.0x view instantly.
- **Unified Paper Appearance:** Choose from nine natural paper color presets (e.g., Pure White, Warm Book, Classic Sepia, Cream, Charcoal, Midnight Dark). The library, navigation chrome, menus, and comic viewport surround share a coordinated palette.
- **Bilingual UI:** Native English and Turkish interface translations. Book titles, table of contents, and document contents remain in their authentic original language (Hebrew/BiDi text shaping excluded per project design).
- **Persistent Bounded Cover Cache:** 150×210 RGB24 thumbnail cache bounded to 32 MiB / 512 entries with file stat validation, CRC integrity checks, automatic recovery, and immediate vector placeholder fallbacks.
- **Reading Journal & Offline Stats:** Tracks reading minutes, sessions, completion percentages, streaks, and period stats entirely offline on SD card.
- **Data Protection & Recovery:** Settings and reading records use verified temporary writes, backup/recovery paths and retries to reduce data-loss risk. The installer does not ship user databases. **Back up `books/` and `data/` before upgrading:** sudden power loss and SD failure can still cause loss.

---

## Installation & Upgrade

### Clean Install

1. Download the release archive [`PaperNX-switch-v0.9.1.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX-switch-v0.9.1.zip).
2. Extract the archive directly to the root of your Switch microSD card (`sdmc:/`).
3. Place your `.epub`, `.pdf`, `.cbz`, and `.cbr` files into `sdmc:/switch/PaperNX/books/` (or use the built-in file browser to access any directory).
4. Launch **PaperNX** from the Homebrew Menu.

#### Verified Release Artifacts (v0.9.1)

| Artifact | Size | SHA256 Checksum |
|---|---:|---|
| `PaperNX.nro` | 48.8 MB | `f411daf2e02f5483ab49563295ea7832ca3e9b0455f433b7c8bfe6db909fced1` |
| `PaperNX-switch-v0.9.1.zip` | 33.6 MB | `898a1f07e4898c164a3e57f17e7af52d7eda553c92f85892aac9bda8eaf249e9` |
| `PaperNX-licenses-v0.9.1.zip` | 207 KB | `2890920a31aac7293e92c45ae90ef5d18e274df844daf74ef42db3539df8f005` |

```text
sdmc:/switch/PaperNX/
├── PaperNX.nro          # Application binary with embedded fonts & assets
├── books/               # Place your EPUB, PDF, CBZ and CBR files here
└── fonts/               # Bundled fonts and OFL notice (also embedded in the NRO)
```

### Upgrading from a Previous Version

Simply extract the new `PaperNX.nro` into `sdmc:/switch/PaperNX/PaperNX.nro`.

> [!IMPORTANT]
> The official release package contains only application files, fonts, and an empty `books/` placeholder. It **never contains** `data/`, `logs/`, or user databases, reducing the risk of overwriting existing reading records, bookmarks, collections and custom themes. Back up `books/` and `data/` before upgrading.

---

## Touch & Controller Controls

All essential features are fully operable via touchscreen. Physical controller bindings are available as optional shortcuts (see [docs/TOUCH_CONTROLS.md](docs/TOUCH_CONTROLS.md) for full gesture reference):

| Action | Touch Gesture | Controller Button |
|---|---|---|
| **Quick Menu / Options** | Tap top-right `+` button | <kbd>+</kbd> (Plus) |
| **Open Book** | Tap book card in library or file browser | <kbd>A</kbd> |
| **Page Turns (Paginated EPUB)** | Tap left/right 30% screen edges or swipe horizontally | <kbd>L</kbd> / <kbd>R</kbd> or D-Pad Left / Right |
| **Continuous Scroll (EPUB)** | Drag vertically up / down | Right Stick or D-Pad Up / Down |
| **Toggle Reading Toolbar** | Tap screen center | <kbd>A</kbd> |
| **Reading Settings (Aa)** | Tap `+` → *Reading settings* | <kbd>X</kbd> |
| **Appearance & Theme** | Tap `+` → *Appearance & Language* | <kbd>Y</kbd> |
| **Table of Contents** | Tap `+` → *Table of Contents* | <kbd>-</kbd> (Minus) |
| **Bookmarks** | Tap `+` → *Bookmarks* | <kbd>ZL</kbd> / Left Bumper |
| **PDF Zoom & Pan** | Tap −/+ on toolbar; drag zoomed page | <kbd>ZL</kbd> / <kbd>ZR</kbd>; Left Stick |
| **Favorite Book** | Tap star icon on library card | Left Stick Click (<kbd>L3</kbd>) |
| **Return to Library** | Tap `+` → *Return to library* | <kbd>B</kbd> |
| **Exit PaperNX** | Tap `+` → *Exit PaperNX* | <kbd>Home</kbd> / <kbd>B</kbd> (in root) |

---

## Building from Source

PaperNX provides reproducible containerized builds using Docker.

### Prerequisites

- [Docker](https://www.docker.com/) and Docker Compose
- Git

### Build for Nintendo Switch (Target AArch64)

```bash
git clone https://github.com/ysfbz/PaperNx.git
cd PaperNx

# Build the Switch NRO binary and release distribution
docker compose run --rm switch bash scripts/verify-switch.sh
```

The resulting artifacts are written to `dist/v0.9.1/`:
- `PaperNX.nro`: Nintendo Switch executable with embedded RomFS assets
- `PaperNX-switch-v0.9.1.zip`: Clean microSD distribution package
- `PaperNX-licenses-v0.9.1.zip`: Open-source license documentation

### Build & Test on Desktop (Linux / macOS / WSL)

```bash
# Run unit, integration, and UI component tests natively
docker compose run --rm desktop bash scripts/verify-v04.sh native

# Run memory sanitizer checks (ASan / UBSan)
docker compose run --rm desktop bash scripts/verify-v04.sh sanitizers
```

---

## Open Source Compliance & Licensing

PaperNX is free software developed by **Yusuf Buzrul with AI assistance** and licensed under the **[GNU Affero General Public License v3.0 (AGPL-3.0)](LICENSE)**.

Code origins and redistribution obligations, including components incorporated from community contributions, are documented in [docs/LICENSES.md](docs/LICENSES.md).

This software includes third-party libraries and fonts licensed under permissive and copyleft open-source terms (MuPDF, libnx, SDL2, SDL2_ttf, FreeType, HarfBuzz, libjpeg-turbo, libpng, zlib, and Noto fonts). Third-party license notices are archived in [`PaperNX-licenses-v0.9.1.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX-licenses-v0.9.1.zip) and under `third-party-licenses/`.

---

<details>
<summary><strong>🇹🇷 Türkçe Açıklama / Turkish Documentation (Genişletmek için tıklayın)</strong></summary>

<br>

### Genel Bakış

**PaperNX**, Nintendo Switch için modern C++17, C11, SDL2 ve MuPDF kullanılarak geliştirilmiş, **yerel ve tamamen çevrimdışı bir EPUB, PDF, CBZ ve CBR okuyucudur**. Joy-Con kumandaları konsoldan ayrılmış durumdayken rahatça kitap ve çizgi roman okuyabilmeniz amacıyla dokunmatik odaklı olarak tasarlanmıştır.

Tüm kitaplarınız, çizgi roman arşivleriniz, kapak önbelleğiniz, okuma geçmişiniz ve okuma günlüğü istatistikleriniz yalnızca microSD kartınızda saklanır. Kullanıcı hesabı, bulut bağlantısı veya web tarayıcı motoru gerektirmez.

> [!NOTE]
> **Varsayılan Dil:** PaperNX temiz kurulumda varsayılan olarak **İngilizce** başlar. Eksiksiz **Türkçe** arayüz desteği dahili olarak mevcuttur ve **Ayarlar → Uygulama → Dil** (`+` butonu veya <kbd>Y</kbd>) üzerinden istendiğinde seçilebilir. Tercihiniz kaydedilir ve yeniden başlatmalarda korunur.

### Temel Özellikler

- **Dokunmatik Odaklı Ergonomi:** Tüm eylemler doğrudan ve hassas dokunmatik hedeflerle yönetilir (56px minimum dokunmatik alan). Kumandalar isteğe bağlı kısayol olarak desteklenir.
- **CBZ ve CBR Çizgi Roman Desteği:** CBZ (ZIP), CBR (RAR4/RAR5 ve solid arşivler) açma; doğal sayfa sıralaması, ComicInfo metadata ve orijinal çizim renklerinin korunması. RAR desteğinde libarchive/unarr kullanılır.
- **İnteraktif Sayfa Çevirme ve Seri Gezintisi:** Sayfalı EPUB/PDF/CBZ/CBR için dokunmaya duyarlı 3D page curl; çizgi romanları seri kartlarında toplama, üç sekmeli sayfa gezgini, küçük resim önizlemeleri ve hızlı sayfa seçimi.
- **Doğal Sayısal Sıralama:** Cilt ve sayı numaralarını doğru sıralayan taşma korumalı `compareNatural` algoritması (Örn: Cilt 2, Cilt 10'dan önce gelir).
- **Gelişmiş Kumanda ve Ekran Yönü Uyumu:** Dikey modlarda ekrana göre yönlenen analog kaydırma ve menü gezintisi; okuyucuda doğrudan sayfa çeviren D-pad kontrolleri; tetik tuşlarının kazara menü değiştirmesini önleyen koruma.
- **Gerçek Dikey Sürekli EPUB Kaydırma:** Sahte sayfa hissi olmadan akıcı dikey piksel kaydırma, ayarlanabilir momentum ve pürüzsüz bölüm geçişleri. Klasik sayfalı yatay okuma modu da eksiksiz korunur.
- **Güçlü PDF Motoru:** MuPDF 1.23.11 tabanlı; genişliğe/sayfaya sığdırma, serbest kaydırma, yakınlaştırma ve belge renk filtreleri (Orijinal, Koyu, Sepya, Ters).
- **Bütünleşik Kâğıt Görünümü:** Dokuz doğal kâğıt teması (Beyaz, Kitap Kâğıdı, Klasik Sepya, Krem, Koyu Kömür, Gece vb.). Kitaplık, araç çubukları ve okuma pencereleri ortak renk paletini paylaşır.
- **Çift Dilli Arayüz:** Doğal Türkçe ve İngilizce dil desteği. Kitap içerikleri ve içindekiler orijinal dilinde korunur.
- **Disk Üzerinde Kapak Önbelleği:** 150×210 RGB24 boyutunda, 32 MiB / 512 dosya ile sınırlandırılmış hızlı kapak önbelleği; sağlama toplamı doğrulaması ve anında beliren şık vektörel kapak yer tutucuları.
- **Okuma Günlüğü ve Çevrimdışı İstatistikler:** Okuma süresi, oturumlar, tamamlama oranları ve serileri tamamen yerel olarak takip eder.
- **Veri Koruma ve Kurtarma:** Geçici dosya, yedekleme ve kurtarma mekanizmaları veri kaybı riskini azaltır. Paket kullanıcı veritabanlarını içermez; ancak SD kart arızası veya güç kesintisinde kayıp mümkündür. Güncellemeden önce **`books/` ve `data/` klasörlerini yedekleyin**.

### Kurulum

1. Güncel sürüm arşivini [`PaperNX-switch-v0.9.1.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX-switch-v0.9.1.zip) indirin.
2. Arşivi Switch microSD kartınızın ana dizinine (`sdmc:/`) çıkartın.
3. `.epub`, `.pdf`, `.cbz` ve `.cbr` dosyalarınızı `sdmc:/switch/PaperNX/books/` dizinine yerleştirin.
4. Homebrew Menu üzerinden **PaperNX** uygulamasını başlatın.

### Dokunmatik ve Kumanda Kısayolları

Tüm temel özellikler dokunmatik ekran üzerinden doğrudan yönetilebilir. Fiziksel kumanda tuşları isteğe bağlı kısayol olarak desteklenmektedir (ayrıntılı jest kılavuzu için [docs/TOUCH_CONTROLS.md](docs/TOUCH_CONTROLS.md) belgesine bakın):

| İşlem | Dokunmatik Yol | Kumanda Kısayolu |
|---|---|---|
| **Hızlı Menü / İşlemler** | Sağ üstteki `+` butonuna dokunun | <kbd>+</kbd> (Plus) |
| **Kitap Açma** | Kitap kartına veya gezgindeki satıra dokunun | <kbd>A</kbd> |
| **Sayfa Çevirme (Sayfalı EPUB/PDF/CBZ/CBR)** | Sağ/sol %30 kenara dokunun veya kaydırın | <kbd>L</kbd> / <kbd>R</kbd> veya Yön Tuşları Sol / Sağ |
| **Sürekli EPUB Kaydırma** | Dikey olarak yukarı / aşağı sürükleyin | Sağ Çubuk veya Yön Tuşları Yukarı / Aşağı |
| **Araç Çubuğunu Göster/Gizle** | Ekranın ortasına dokunun | <kbd>A</kbd> |
| **Okuma Ayarları (Aa)** | `+` → *Okuma ayarları* | <kbd>X</kbd> |
| **Uygulama Görünümü ve Dil** | `+` → *Uygulama görünümü* | <kbd>Y</kbd> |
| **İçindekiler** | `+` → *İçindekiler* | <kbd>-</kbd> (Minus) |
| **Yer İmleri** | `+` → *Yer imleri* | <kbd>ZL</kbd> / Sol Omuz |
| **PDF/CBZ/CBR Yakınlaştırma ve Kaydırma** | Çubukta −/+, büyütülmüş sayfayı sürükleyin | <kbd>ZL</kbd> / <kbd>ZR</kbd>; Sol Çubuk |
| **Favorilere Ekleme** | Kitap kartındaki yıldıza dokunun | Sol Çubuk Tıklama (<kbd>L3</kbd>) |
| **Kitaplığa Dönüş** | `+` → *Kitaplığa dön* | <kbd>B</kbd> |
| **Uygulamadan Çıkış** | `+` → *PaperNX’ten çık* | <kbd>Home</kbd> / <kbd>B</kbd> (ana ekranda) |

### Lisans ve Telif Hakkı

PaperNX, **Yusuf Buzrul** tarafından yapay zekâ desteğiyle geliştirilmiştir ve **[GNU Affero General Public License v3.0 (AGPL-3.0)](LICENSE)** kapsamında lisanslanmıştır. Kullanılan üçüncü taraf kütüphanelerin lisansları `PaperNX-licenses-v0.9.1.zip` içinde sunulmaktadır.

- Yusuf Buzrul tarafından yapay zekâ desteğiyle geliştirilmiştir.
- Geliştirici profili: [https://github.com/ysfbz](https://github.com/ysfbz)

</details>

---

## Author & Maintainer

**Yusuf Buzrul**  
- Developed by Yusuf Buzrul with AI assistance.
- GitHub: [@ysfbz](https://github.com/ysfbz) ([https://github.com/ysfbz](https://github.com/ysfbz))  
- Repository: [https://github.com/ysfbz/PaperNx](https://github.com/ysfbz/PaperNx)

