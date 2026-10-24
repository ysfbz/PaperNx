# PaperNX v0.9.3 — Known limitations

These are known scope or platform limits, not claims of failures observed in the release.

## Switch hardware and installation

- Broad v0.9.3 acceptance on a physical Nintendo Switch has not been completed. Touch feel, Joy-Con reconnect, sleep/resume, portrait mapping, system brightness, SD durability, real GPU frame time and device memory remain **UNVERIFIED**. See the [physical test plan](DEVICE_TEST_PLAN.md).
- Updates are downloaded, verified and staged; the user manually replaces the NRO. No automatic launcher/chainload install or power-loss-safe replacement is provided. Keep a known-good NRO and back up `books/` and `data/` before updating. See [updater behavior](UPDATER.md).
- First startup and cover-cache behavior for very large libraries varies with the document mix, console and SD card. Desktop software-renderer measurements are not Switch performance guarantees.
- Sudden power loss or SD-card failure can still lose unsaved data despite temporary writes and recovery copies.

## Reader scope

- PDF OCR, PDF text reflow, in-book full-text search, annotations and dictionary lookup are not included.
- Hebrew/BiDi shaping is outside the current localization scope; the interface is available in English and Turkish.
- EPUB position restoration can be approximate for malformed or image-only chapters after layout changes.
- Book identity uses file metadata/path characteristics; moving or replacing a file does not automatically merge its reading history.
- Permanent file deletion has confirmation but no trash/undo. Use disposable copies for deletion testing.
- Reading-time and completion statistics are interaction-based estimates, not evidence that the screen was actively read.

The release is a stable software release, not a hardware-certified build. Passing desktop tests and Switch cross-compilation do not change the **UNVERIFIED** physical-device status.
