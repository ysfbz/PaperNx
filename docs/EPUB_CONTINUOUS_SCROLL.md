# EPUB continuous scrolling — historical v0.3 development notes (feature included in v0.9.1)

Originally implemented on the historical `codex/epub-continuous-scroll` branch based on Reader v2. The continuous-scroll feature is part of v0.9.1. Below, old build output paths and benchmark values are archival, not current installation instructions; see [BUILD_SWITCH.md](BUILD_SWITCH.md) and [RELEASE_REPORT.md](RELEASE_REPORT.md).

## Touch controls

From the library, tap **Settings → Reading → Navigation**. The navigation button selects **Paginated Mode** or **Continuous Scroll Mode**. Set sensitivity between 50% and 200% and switch momentum on or off. These settings persist as global defaults.

Inside an EPUB, tap to show the toolbar, open **Aa → Navigation**, and select the mode. Changes here create a per-book navigation override. **Use global navigation settings** removes it. Existing records migrate with pagination selected and no override. PDFs retain their existing navigation.

In continuous mode, dragging moves content immediately; horizontal swipes cannot turn pages. A tap opens or dismisses the toolbar. Multitouch cancels the single-finger gesture. Momentum decays exponentially, stops at book bounds and on a new touch, and pauses for settings or app suspension.

## Rendering and position

MuPDF lays EPUB chapters out with height zero (its unpaginated HTML layout). The viewport draws clipped 512-pixel strips in one continuous coordinate space and joins successive chapters within the same frame. It preloads nearby strips, including the preceding chapter edge. It never creates a full-chapter raster texture. The existing LRU page texture budget remains 64 MiB, the document allocator remains limited to 96 MiB, and the MuPDF store remains 32 MiB within that allocator. Book-sized pixel allocations and caches are avoided; MuPDF still parses/layouts document content within its allocator budget. Exceptionally complex chapters can exceed that budget and fail rather than grow without bound.

Progress schema `PAPERNX-5` preserves schemas 1–4 on load. It stores a chapter-relative ordinal of non-whitespace text characters plus a normalized offset within the text line. It resolves that anchor after reflow, restart and mode changes. Identical layout restores subpixel offsets; changed wrapping restores the line containing the character, so precision is limited to that line. Image-only/end-of-chapter locations fall back to normalized chapter geometry. These are logical positions, not EPUB CFI identifiers; publisher CSS that changes document text or hides content can make an ordinal approximate. Existing chapter/fraction bookmarks remain compatible fallbacks.

## Automated evidence

- Pinned MuPDF 1.23.11 desktop: core, document rendering and application regression suites.
- ASan/UBSan with leak detection: all three suites.
- Tests cover global touchscreen selection, persistence, per-book override, immediate motion before release, horizontal-swipe suppression, optional momentum/interrupt, chapter crossing in both directions, bounds, reflow/theme/mode round trips, reopen in a new application instance, geometric fallback, old-settings migration and repeated strip traversal with cache eviction.
- Desktop SDL dummy benchmark: 180 motion-plus-render samples at 1280×720; font 30, viewport 1200×576, sensitivity 100%. A representative run measured median 0.616 ms, p95 1.542 ms and maximum 50.15 ms, with 9,830,400 cached texture bytes at the end. See `verification/scroll-responsiveness.log`. This measures CPU event handling/render submission on a desktop software backend, not display latency or physical Switch performance. Cold strip rendering accounts for occasional larger frames. The automated p95 ceiling of 100 ms is a broad regression guard, not a performance target for Switch.
- Switch cross-compiled NRO and validated SD package: `dist/v0.3.0-scroll-rc1/`. Branding, bundled fonts and license packaging are preserved. The Switch display version is shortened to `0.3.0-scr-rc1` to fit the NACP field.

Reproduce using the existing `papernx-desktop` image with `cmake --build build-pinned --parallel 4` and `ctest --test-dir build-pinned --output-on-failure`. Run the app suite under `SDL_VIDEODRIVER=dummy` for its benchmark line. Build the native candidate in `papernx-switch` with `MUPDF_PREFIX=/work/prefix-mupdf-switch` and the Switch portlibs bin directory on PATH.

## Physical Nintendo Switch acceptance — pending

No physical Switch was available in this session. Cross-compilation and desktop SDL simulation do not establish hardware responsiveness. Before hardware acceptance:

1. Copy the candidate NRO and fonts onto an SD card, retaining existing `data/` and `books/`. Confirm existing readers initially use pagination.
2. With Joy-Con detached, select continuous mode from library settings, then verify per-book override and inheritance through Aa. Exit/relaunch and check both preferences.
3. Drag slowly and quickly in both directions with momentum on/off and sensitivity 50%, 100%, 200%. Verify continuous finger tracking, touch interruption, no horizontal page turns, and no accidental toolbar opening while dragging.
4. Cross several chapter boundaries in both directions, including image-heavy and short chapters. Confirm images, alignment, margins, colors and typography, and check the first/last viewport clamps.
5. At a distinctive paragraph or image, change font size, theme, orientation/viewport where supported, switch modes repeatedly, suspend/resume and restart. Record the restored line or image offset and any drift.
6. Record high-frame-rate video of finger motion and display response. Measure input-to-visible-motion latency, frame-time median/p95/worst and cold chapter-crossing stalls separately from the desktop results. Run an extended scrolling session and record memory/high-water usage and failures.
7. Test both applet and title-override launch contexts and document the device, firmware, launch context, EPUB size/complexity and test outcomes. Hardware sign-off remains open until this evidence exists.
