# PaperNX v0.9.1 — Physical Switch acceptance checklist

Target the **v0.9.1 NRO release binary** (`PaperNX.nro`, SHA256: `2e376f54500497d3fe8e06319af53238dd34c2d01a774df40c97747213e530b5`). Record Switch model, firmware, Homebrew Menu launch mode (Applet vs Title Override), installed NRO hash, language, and sample books. Every check below remains **UNVERIFIED** for v0.9.1 pending actual physical device execution. Automated desktop tests and cross-compilation passes do not constitute physical hardware pass evidence.

| # | Check | Result |
|---|---|---|
| 1 | Launch with Joy-Con detached; complete library → reader → settings → exit by touch | UNVERIFIED |
| 2 | Professional library layout, long Turkish titles, empty state | UNVERIFIED |
| 3 | Button font weights, baseline alignment, readable labels/descenders | UNVERIFIED |
| 4 | Visible focus on every menu, tab, list and confirmation | UNVERIFIED |
| 5 | D-pad navigation; A edit/adjust/confirm; B cancel restores; no accidental changes | UNVERIFIED |
| 6 | D-pad Left/Right paginated EPUB turns | UNVERIFIED |
| 7 | D-pad Left/Right PDF turns while zoomed | UNVERIFIED |
| 8 | Slow zoomed PDF drag pans without changing page | UNVERIFIED |
| 9 | Deliberate fast horizontal PDF swipe changes page while zoomed | UNVERIFIED |
| 10 | Pinch, zoom limits, viewport boundaries and fit modes | UNVERIFIED |
| 11 | Dark PDF with matching canvas, toolbar and modal | UNVERIFIED |
| 12 | Sepia PDF with matching canvas and modal | UNVERIFIED |
| 13 | Sepia EPUB settings, Dark TOC, OLED quick menu match | UNVERIFIED |
| 14 | Immediate pressed feedback; rapid taps and menu transition timings | UNVERIFIED |
| 15 | Long press context; movement cancels hold; no duplicate mouse action | UNVERIFIED |
| 16 | X context on focused book | UNVERIFIED |
| 17 | Favorite and read/unread/completed state survive restart without losing position | UNVERIFIED |
| 18 | Edit author/genre; Turkish keyboard; metadata leaves source unchanged | UNVERIFIED |
| 19 | Author/genre groups, search and sort within filter | UNVERIFIED |
| 20 | Create/rename/delete shelves, multi-membership, remove membership | UNVERIFIED |
| 21 | Hide/remove preserves file; rescan does not re-add; restore works | UNVERIFIED |
| 22 | Cancel permanent deletion; file remains | UNVERIFIED |
| 23 | Confirm deletion of a deliberately disposable copy inside books root; test failure error | UNVERIFIED |
| 24 | Stable fallback cover and aspect-fitted real artwork | UNVERIFIED |
| 25 | Global/per-book preferences survive restart; reset inheritance | UNVERIFIED |
| 26 | Restore progress/bookmarks; Page Flip returns exact logical location | UNVERIFIED |
| 27 | Stats exclude idle/menu/sleep; goal persists; ETA absent until real evidence | UNVERIFIED |
| 28 | Continuous EPUB vertical scroll and chapter boundaries | UNVERIFIED |
| 29 | Large EPUB/PDF, image-heavy chapter, cold/warm covers and memory | UNVERIFIED |
| 30 | Clean exit, sleep/resume, disconnect/reconnect; no white flash/data loss | UNVERIFIED |
| 31 | Clockwise portrait: correct rotation, all touch targets, menus and PDF/EPUB fit | UNVERIFIED |
| 32 | Counterclockwise portrait: inverse touch alignment, pagination, continuous scroll | UNVERIFIED |
| 33 | Orientation restart persistence and explicit return to landscape, docked resolution | UNVERIFIED |
| 34 | All nine paper presets, background/text swatches, cancel/defaults, contrast warning | UNVERIFIED |
| 35 | Off/Soft/Natural texture readability; images unaffected | UNVERIFIED |
| 36 | CBZ Comic Reader: natural page ordering, original RGB rendering, thumbnail extraction, 16 MiB memory bounds | UNVERIFIED |
| 37 | Controller Ergonomics: D-pad Left/Right direct page turns in portrait mode; right stick analog pan | UNVERIFIED |
| 38 | Natural Numeric Sorting: Volumes 1, 2... 10 sort in true numeric order across library and series | UNVERIFIED |

Never use a valuable original for deletion testing. First back up `books/` and `data/`; copy a disposable EPUB/PDF/CBZ into the allowed books root. Test cancel first, then explicit confirmation. Shelf deletion must leave book files intact. The public v0.9.1 Release exists, but **real-device acceptance has not been demonstrated**; do not describe it as hardware-validated until the matrix and any P0 failures have been resolved.
