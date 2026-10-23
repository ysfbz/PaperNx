# PaperNX v0.9.2 — Physical Switch acceptance

**Status: NOT EXECUTED.** This is a test record template, not evidence of device acceptance. Desktop SDL tests and Switch cross-builds do not change any result below to PASS.

## Device and artifact record

| Field | Value |
|---|---|
| Console model / serial alias | NOT RECORDED |
| System firmware | NOT RECORDED |
| Homebrew launch mode (Applet / Title Override) | NOT RECORDED |
| PaperNX source commit | NOT RECORDED |
| Installed NRO SHA-256 | NOT RECORDED |
| Joy-Con configuration | NOT RECORDED |
| Display language | NOT RECORDED |
| SD filesystem / free space | NOT RECORDED |
| Test date / operator | NOT RECORDED |

## Inherited device checks

Repeat all 38 checks in [`DEVICE_TEST_PLAN.md`](DEVICE_TEST_PLAN.md) against the v0.9.2 candidate. Keep each original check UNVERIFIED until its specific physical steps and evidence are recorded. The v0.9.1 release hash in that document is historical and must not be copied into a v0.9.2 result.

## v0.9.2-specific checks

| ID | Physical check | Result | Evidence |
|---|---|---|---|
| V092-01 | Fresh settings start in single-page mode for EPUB/PDF and CBZ/CBR. | UNVERIFIED | |
| V092-02 | Change EPUB/PDF landscape spread, then comic spread; confirm independent state after app restart. | UNVERIFIED | |
| V092-03 | Migrate a backed-up v0.9.1 settings/progress/journal dataset; ambiguous old spread values default to single page and unrelated data remains intact. | UNVERIFIED | |
| V092-04 | Use `Aa → Sayfa düzeni` and `Aa → Sayfa görünümü`; verify per-format setting, reading anchor/page, bookmarks, progress, zoom and journal position. | UNVERIFIED | |
| V092-05 | Touch `+` opens the existing quick menu. The top row shows `[Tek / Çift Sayfa] [Yatay / Dikey]` without overlap in landscape and portrait. | UNVERIFIED | |
| V092-06 | Joy-Con `+` / SDL START opens and closes that same menu in library, series collection, EPUB, PDF, comic, file browser and reading journal. | UNVERIFIED | |
| V092-07 | Press `+` during save confirmation, text entry, update confirmation/download, and other modal work; no modal input or user data is lost. | UNVERIFIED | |
| V092-08 | D-pad/analog reaches both fixed shortcuts and the scrollable actions; A activates, B preserves its back behavior, one press toggles once. | UNVERIFIED | |
| V092-09 | Disconnect and reconnect the controller; START and other mapped controls continue to work without repeated toggles. | UNVERIFIED | |
| V092-10 | Touch hitboxes match both visible top shortcut buttons in landscape and portrait; long Turkish/English labels remain legible. | UNVERIFIED | |
| V092-11 | On handheld, record system brightness before launch, change it, then exit; confirm the original hardware brightness is restored. | UNVERIFIED | |
| V092-12 | Dock/undock and deny/unavailable backlight service; confirm software dimming remains usable and hardware status reports fallback. | UNVERIFIED | |
| V092-13 | HOME/background, suspend, wake/resume and normal exit restore brightness and return reader progress without a white/black flash or data loss. | UNVERIFIED | |
| V092-14 | Rotate landscape → portrait → landscape while using touch and controls; confirm one-page portrait layout, correct targets and no stale curl. | UNVERIFIED | |
| V092-15 | EPUB/PDF landscape 2-up uses the saved preference; portrait and continuous EPUB remain single-column. Verify final odd page and bookmarks/TOC. | UNVERIFIED | |
| V092-16 | CBZ/CBR spread preference preserves wide scans and ComicInfo `DoublePage` whole; switching to single page keeps deterministic navigation. | UNVERIFIED | |
| V092-17 | RAR4, RAR5 and solid CBR: rapid turns, reverse turns, zoom/pan, page curl cancel, 30-second idle, background/resume and memory pressure. | UNVERIFIED | |
| V092-18 | On a disposable SD copy, check only the official `ysfbz/PaperNx` update; inspect notes and confirmation, verify TLS/hash/NRO identity, then stage the `.new` package. | UNVERIFIED | |
| V092-19 | Offline check, canceled download, insufficient space, SD write error and restart after staging; current NRO and books/settings/progress/journal remain intact. | UNVERIFIED | |
| V092-20 | Cover cache cold/warm/reopen and scrolling on 100/500/1000 real books; record first visible cover, total fill, hit/miss, archive/metadata opens, memory and frame pacing. | UNVERIFIED | |
| V092-21 | Confirm cache-clear prompt; clear/rebuild covers and verify book files, progress, favorites, settings and Reading Journal remain unchanged. | UNVERIFIED | |
| V092-22 | Light, Dark, Follow Reading, all seven accents and Soft/Natural Paper in library, reader, quick menu and dialogs; inspect comic artwork colors. | UNVERIFIED | |
| V092-23 | Sleep/resume with queued covers, active reader, library scan and updater staged; verify workers, focus and persisted data recover safely. | UNVERIFIED | |

## Evidence required for PASS

For each test, record the ID, date, operator, device/firmware, installed NRO hash, sample file and SD details, exact steps, observed result, and a screenshot/video or relevant log. For brightness, record before/after system values. For performance, collect at least five runs and report median/p95, counters, RAM and frame observations; identify whether the result came from Switch hardware or a desktop renderer.

Use a disposable SD card or complete backup for update replacement, full-disk, write-fault, and any destructive scenario. Do not use valuable originals. Until evidence is attached and reviewed, every result remains UNVERIFIED and the device gate stays closed.
