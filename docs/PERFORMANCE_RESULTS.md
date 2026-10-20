# PaperNX — Historical desktop performance measurements

**Not a v0.9.1 physical Switch benchmark.** These baseline values are retained for engineering comparison. The original measurement artifact `verification/v04-ui-latency.json` and older v0.3 reports were removed from the tracked repository; the raw samples are no longer independently reviewable here.

Reported 32-iteration Docker SDL software-renderer CPU event-to-present-return measurements:

| Metric | p50 | p95 | Worst | Notes |
|---|---|---|---|---|
| `tap_to_pressed_present` | 8.22 ms | 8.38 ms | 8.62 ms | Immediate visual feedback on `SDL_FINGERDOWN` |
| `tap_to_action_present` | 16.45 ms | 16.82 ms | 17.02 ms | Action dispatch and presentation on `SDL_FINGERUP` |
| `focus_to_present` | 8.22 ms | 8.40 ms | 8.40 ms | Directional navigation focus ring redraw |

Measured document and layout pipeline timings:
- **Large EPUB open:** 30.25 ms (24 chapters)
- **Illustrated EPUB open:** 3.82 ms (8 chapters, 2048×1536 illustrations)
- **PDF open to first page:** 2.39 ms (320-page document)
- **Cold page render (p95):** 4.05 ms
- **Warm / cached page render (p95):** 1.28 ms
- **Settings tab render (p95):** 27.78 ms
- **Menu open render (p95):** 20.35 ms
- **Asynchronous layout & atomic swap:** 233.24 ms (coalesced 3 rapid requests into 1 accepted layout)
- **Peak page texture cache:** 66.59 MB (within 64 MiB logical budget)
- **Process RSS high-water mark:** 148.66 MB

Current engineering improvements:
- Finger-down pressed feedback on primary controls.
- Cached text measurements and stable font metrics in `Typography`.
- Fast single-pass scroll and clipping in choice popup.
- PDF flick gesture defaults tested on desktop with displacement/duration and average-velocity thresholds; real Switch hardware tuning remains outstanding.
- Direct rotated target presentation without double copying.
- Book-scoped appearance: coordinated canvas and reader chrome updates during asynchronous reflow; frame-perfect hardware rendering has not been demonstrated.
- Background metadata extraction for cached covers without UI thread blocking (bounded to 2 updates per frame).

Note: Desktop SDL software renderer timings measure CPU event-to-present return and do not include physical Switch touchscreen scanning, physical display scanout, or micro-SD latency. Physical hardware latency and feel remain **UNVERIFIED** until on-device acceptance testing.

## v0.9.1 comic and page-curl performance evidence

The current v0.9.1 source adds RAR4/RAR5/solid archive reading, texture reuse for comic zoom/pan, page curl snapshots and background page-preview generation. **No comparable p50/p95/worst CBZ or CBR decode/open times, interactive curl frame-time distribution, or physical-Switch RAM/FPS measurements have been published in this file.** Accordingly these metrics remain **UNVERIFIED**, not zero-latency or an inferred performance improvement. The historic figures above refer to an earlier desktop software-renderer baseline; do not treat them as measurements of the v0.9.1 release. See [release verification](RELEASE_REPORT.md), [physical acceptance](DEVICE_TEST_PLAN.md) and [known limitations](KNOWN_ISSUES.md).
