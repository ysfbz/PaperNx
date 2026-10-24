# PaperNX product roadmap — 2026

Updated to describe the publicly available v0.9.1 source and future candidates. This is an offline native C++17/SDL2/MuPDF reader. Inspiration denotes possible capabilities, not licensed access to competitor assets, services or a delivery promise. Source implementation and physical acceptance are separate; see [current status](STATUS.md), [known limitations](KNOWN_ISSUES.md) and [device plan](DEVICE_TEST_PLAN.md).

Work originating in v0.4 (shared reader chrome, settings/focus, PDF/EPUB gestures, portrait output, themes, catalog and reading journal) is integrated into the published v0.9.1 source. Automated tests are reported in [TEST_RESULTS.md](TEST_RESULTS.md); physical device checks remain UNVERIFIED. Some table rows retain historical v0.4 scope terminology.

| Inspiration / candidate | Status | Technical acceptance gate |
|---|---|---|
| Kindle: Page Flip/history | Implemented page curl and existing navigation/history, physical acceptance pending | Paginated EPUB/PDF/CBZ/CBR curl supports touch commit/cancel; 3-tab navigator and progress preview are in source; validate on hardware |
| Kindle: Time to Read | Basic whole-book estimate; chapter ETA DEFERRED | Monotonic active time, idle/suspend exclusion, minimum actual pace evidence; no fabricated values |
| Kindle: in-book search | DEFERRED | Unicode normalized text index, bounded CPU/RAM, cancellable MuPDF extraction and real result navigation |
| Kindle: highlights and notes | DEFERRED | Selection/hit testing, stable layout-independent anchors, Unicode graphemes, export/versioned persistence |
| Kindle: word lookup / saved vocabulary | DEFERRED | Offline licensed dictionary format, morphology/Unicode, bounded lookup/index cache, source attribution |
| Kindle: Word Wise-style hints | DEFERRED | Licensed lexical frequency data, reader opt-in, text layout stability, CPU cost and spoiler-safe context |
| Kindle: X-Ray-style character/concept index | DEFERRED | Local streaming entity index, ambiguity correction, spoiler boundaries, bounded RAM and dataset license |
| Kindle: Story So Far recap | DEFERRED | Reliable read-location boundaries; offline retrieval first; any model must prove license, storage, RAM, CPU latency and factual/spoiler safety |
| Kobo: statistics/history/ergonomic controls | Basic implementation subject to gates | Sessions, days and active duration persist; D-pad/default touch regression; richer reports after data evidence |
| Kobo: collections/series | Catalog shelves and series text overlay | Shelf lifecycle/multi-membership; bounded Calibre/EPUB3 series/index extraction implemented; independently normalized entity indexing DEFERRED |
| KOReader: typography/publisher policy | Existing and v0.4 UI | Fonts/spacing/margins independent of app theme; relayout anchor, malformed EPUB tests |
| KOReader: external fonts | DEFERRED | Explicit microSD directory, bounded font loading, malformed-file handling, redistribution/license guidance |
| KOReader / Moon+: configurable gestures | DEFERRED full editor | Conflict-free mandatory touch routes, remapping persistence and controller-free recovery |
| KOReader / BOOX: PDF margin crop | DEFERRED | Non-destructive viewport crop, page transforms, bookmarks/history and illustration preservation |
| KOReader / BOOX: PDF reflow | DEFERRED | Text extraction/layout prototype, order/columns/tables validation, CPU/RAM benchmarks; never call image crop OCR/reflow |
| KOReader: offline dictionary | DEFERRED | Open/licensed dataset and parser, bounded archive/index handling, Turkish Unicode and maintainability |
| ReadEra: SD discovery/metadata/grouping/shelves | Existing discovery + v0.4 catalog | Hidden rescan persistence, sidecar editing, exact source unchanged; OPF subject/PDF Subject cache is populated on book open; independent multiple-entity indexing remains limited |
| ReadEra: duplicates/moved files | DEFERRED | Incremental bounded content fingerprints, explicit conflict merge, preserve old progress, no title-based guessing |
| ReadEra: safe file management | v0.4 guarded single-book deletion | Explicit confirmation, regular scanned EPUB/PDF/CBZ/CBR in root, no links/traversal, permission failure and disposable device tests |
| Moon+: page preferences / auto-scroll | Page defaults existing; auto-scroll DEFERRED | Configurable speed, pause on modal/touch/suspend, stable continuous anchors, bounded frame work |
| BOOX: OCR | DEFERRED | Offline engine/language data licensing, CPU/RAM/storage/latency benchmark on real Switch, cancellation and results quality |
| BOOX: column-aware PDF navigation | DEFERRED | Bounded geometric analysis/manual columns, no false text reflow claim, portrait/history consistency |
| BOOX: CBZ manga/spreads/direction | CBZ implemented; per-book reading direction implemented; dual-page spreads DEFERRED | ZIP archive parsing, natural ordering, ComicInfo, image protection; right-to-left manga page direction supported; true two-page spreads not shipped |
| BOOX: CBR | Implemented in v0.9.1 (RAR4, RAR5 and solid archives); hardware acceptance pending | libarchive/unarr decode, bounded image/archive handling, ComicInfo and real fixture-based tests; physical latency/memory not yet measured |
| Apple Books: goals/calendar/completion timeline | Basic daily goal/date model; richer reports DEFERRED | Real active minutes, local date handling, timezone changes, restart recovery; no invented streaks/history |
| Apple Books: annual statistics | DEFERRED | Real retained dates/sessions, clear corrections/export; avoid unnecessary reader clutter |
| Calibre: metadata tags/series compatibility/export | Text overlays and natural numeric sorting implemented; interchange DEFERRED | Versioned bounded schema, natural numeric sorting for titles/series/volumes, Unicode roundtrip, import preview/backup |
| Calibre: OPDS/wireless | DEFERRED | Optional explicit opt-in; bounded requests, transport security, no mandatory server/account; SD remains usable offline |

Performance policy: renderer-thread SDL texture work only; MuPDF ownership explicit; bounded page/cover queues, 48 MiB/512-entry cover disk cache; never rasterize a complete book. Dictionary/OCR/AI/TTS datasets are separate license, CPU, RAM and storage gates. Generative inference is not assumed feasible on Switch. Benchmark p50/p95/worst on the target hardware before promotion.

Safety policy: preserve books and progress; metadata remains a sidecar. Keep migration backups. Recoverable flushed replacement reduces interruption risk but is not a guarantee of FAT power-loss durability. No telemetry, cloud login, browser engine or mandatory network dependency. The v0.9.1 codebase incorporates community-contributed implementation; see [licenses and source provenance](LICENSES.md). v0.9.1 is publicly available, but any claim of hardware-validated stability still requires physical acceptance.
