# PaperNX v0.9.1 — Known limitations and validation gaps

Not every limitation here is a defect. This document distinguishes implemented behavior, deferred features, and unverified hardware-dependent paths.

## P1 — Real Nintendo Switch hardware validation

The published v0.9.1 NRO has **35/35 physical-device acceptance checks marked UNVERIFIED** in [DEVICE_TEST_PLAN.md](DEVICE_TEST_PLAN.md). Priority cases: Joy-Con-detached touch, both portrait modes and touch mapping, PDF/CBZ gestures, restart and sleep/resume, docked/handheld transitions, controller reconnect, SD card write resilience and Unicode glyph rendering. Passing native desktop tests and producing AArch64 NRO binaries do not substitute for real-device physical acceptance testing.

## Known implementation constraints

- **Storage:** Atomic temp writes, backup recovery, and retries lower corruption risk, but power loss, FAT/microSD limitations, and abruptly interrupted writes can still lose unsaved progress. Back up `books/` and `data/` before upgrading.
- **Book identity:** Primarily path and file-size based; moving or replacing files creates a new identity without automatically merging history records.
- **EPUB position restoration:** Malformed or image-only chapters use approximate geometric fallbacks after layout recalculations.
- **PDF & CBZ Rendering:** Whole-page pixel transformations (contrast/invert) affect images and illustrations. Built-in PDF crop, OCR, and true text reflow are not supported. Comic reader displays single pages in original RGB format within a 16 MiB memory safety limit.
- **File management:** Deletion uses explicit two-step confirmation and path/extension validation. There is no trash bin or undo. Test permanent deletion only on disposable copies.
- **Reading statistics:** Active reading time and finish estimates use interaction-derived heuristics, not eye tracking or proof the screen was actively read. Chapter ETA and advanced calendar reporting are not implemented.
- **Catalog metadata:** Automatic moved-file reconciliation and normalized multi-author/subject indexing remain limited.
- **Physical ergonomics:** Focus ring visibility, long title labels, portrait ergonomics, and sleep/restart behavior require physical on-device verification.

## Planned / not included in v0.9.1

- **CBR format:** Deferred due to RAR decompression licensing constraints and safety boundaries (CBZ is supported).
- **In-book full-text search:** Deferred.
- **Highlights and annotations:** Deferred.
- **Offline dictionary lookup:** Deferred.
- **PDF reflow / OCR:** Deferred.
- **Arbitrary gesture remapping:** Deferred.

See [Roadmap](PRODUCT_ROADMAP_2026.md) for future candidate milestones.

## Evidence

[Verified Automated Tests](TEST_RESULTS.md) · [Current Release Report](RELEASE_REPORT.md) · [Hardware Acceptance Checklist](DEVICE_TEST_PLAN.md).
