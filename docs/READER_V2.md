# Reader v2 — historical implementation notes (pre-v0.9.0)

Baseline main: `338fa80aab0cbeb5f27d3d04dce2c9efacd3c566` (fetched 2026-10-17).
Historical branch: `feat/touch-reader-settings-v2`. The no-merge instruction applied to that earlier delivery only and is **not** current guidance; the development branch has since been retired. Current status: [STATUS.md](STATUS.md), [BUILD_SWITCH.md](BUILD_SWITCH.md), [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## Persistence

The old implementation required POSIX rename-over-existing semantics on Switch. libnx `fsdev_rename` delegates directly to Horizon `fsFsRenameFile`; it does not perform POSIX destination replacement. This is a source-based explanation of the reported repeated-save failure, not a hardware-confirmed diagnosis. New Switch/Windows promotion removes an existing primary only after flushing and verifying the new temp and an independent backup. Tests exercise that path on Linux via `noReplaceRename`. Existing `.bak`, `.bak.old`, `.tmp` recovery order is retained. A temp-only recovery is copied to backup before another temp write. Backup writes are flushed and read back, not assumed successful.

`fsync` is supported by current libnx and maps to `fsFileFlush`; there is no blanket ignore of ENOSYS/EINVAL. Unsupported/failed flush remains an explicit failure with retry and diagnostic. Linux directory flush is checked; Switch does not attempt POSIX directory fsync. No unsupported SD filesystem commit operation is silently used as a substitute.

Failures record operation, path, errno/error_code/message, native fsdev Result (when available), validity of primary/backup/temp and retry capability. Read-only/missing media and short writes remain failures. Actual media recovery may require reinsertion/restart; retry_possible means an application retry is retained, not a guarantee storage will recover. Backoff grows from 6 to 60 seconds; identical failures are logged/displayed once per failure episode. Closing a book, backgrounding and exit attempt a verified save; dirty state clears only on success.

PAPERNX-4 extends V3 rows with eight bounded integers: independent text/background RGB24 (-1 means preset), paragraph spacing, indentation, touch-zone percent, publisher option, PDF filter and PDF view mode. Existing V1/2/3 readers remain in place. `PAPERNX_DEFAULTS` is a reserved 16-character settings record with no book path or progress. Existing books retain their migrated preferences; newly opened books inherit explicit defaults. Downgrading to old binaries requires the pre-upgrade save backup.

## Touch

Coordinates stay normalized until hit testing against renderer dimensions. Finger and mouse use press/motion/release semantics; a recently observed native touch suppresses its synthesized mouse stream. A mouse-only backend may use SDL_TOUCH_MOUSEID. Release activates UI, avoiding button-down activation while scrolling. Multi-finger sequences cancel tap/page actions. Background/foreground resets pending input. Same-ID new press recovers an interrupted press. SDL Switch motion-ID mismatch is corrected only when exactly one finger is active.

The reviewed devkitPro SDL2 Switch backend normalizes by 1280/720, uses native finger IDs for down/up and a touch-array index for motion. That mismatch matters for pan and motion tracking. Slow swipes and stationary long presses have no arbitrary speed cutoff. Page zones default to 30% per edge; controls can choose 20/30/40. A toolbar press cannot become a horizontal page swipe. PDF drag uses a movement threshold; menus receive taps rather than PDF pan.

Sources: [libnx fsdev](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/devices/fs_dev.c), [devkitPro SDL Switch touch](https://github.com/devkitPro/SDL/blob/switch-sdl-2.28/src/video/switch/SDL_switchtouch.c). Source snapshots are under verification; physical event capture is still UNVERIFIED.

## Rendering and budgets

Reader preferences are a shared model; EPUB CSS receives colors independently of the curated theme index. Interface colors use curated palettes so arbitrary page colors cannot make settings unreadable. Reader overrides target body/prose/headings and preserve images; publisher mode retains publisher typography. Serif/Sans/Publisher are available; existing Noto fonts and Turkish coverage remain bundled.

Relayout opens and paginates a replacement document before dropping the old valid one. Failure retains the old document and restores the app's previous saved style. Success invalidates textures, rebuilds TOC and resolves the logical MuPDF bookmark. Page color changes currently reopen/relayout EPUB too: correctness is prioritized; a paint-only MuPDF path is not implemented. Reader caches remain 64 MiB and cover disk cache is bounded to 48 MiB/512 entries; the in-memory thumbnail texture cache remains 32 MiB. MuPDF budget stays 96 MiB per document context. Transactional relayout shares that budget; complex books can fail safely under pressure.

PDF renders viewport-sized surfaces, capped at 4096 per dimension by the bridge; maximum zoom never allocates a full giant page. Stable 72px top/bottom reading space keeps header/footer away from document content without repagination when toggling tools. Fit-width and 72 dpi reference view share bounded pan. Zoom preserves the focal point when bounds permit. Filters are explicitly whole-page and reversible; photos/diagrams are affected. Original is default. Page changes reset pan and cache keys include page/view/filter.

## Remaining limits

This is a release candidate, not a Kindle parity claim. Physical save, touchscreen, pinch, suspend/resume, docked transitions, applet memory and hbmenu appearance require device acceptance. No physical screenshots/video were accessible in this workspace; reported findings drove regression tests. Automated viewport checks cover 640×480, 960×540, 1280×720 and 1920×1080; these do not establish physical legibility. Long card titles use two lines with the complete selected title in the footer. Large-library initial SD scanning remains synchronous. Production hardware latency and relayout memory high-water marks are UNVERIFIED.
