# PaperNX v0.9.2 cover-cache measurements

## G6 exploratory measurement — 2026-10-24

**Result:** synthetic repository I/O only; `COVER-01` remains **NOT PASSED**.

The run used `tests/cover_performance_tests.cpp` inside the pinned desktop Docker service. It creates 7-byte placeholder files with `.epub` names and supplies the same generated 150×210 RGB image for every cache entry. It does not open EPUB/PDF/CBZ/CBR documents, call MuPDF, create SDL textures, or measure UI frames. The times below describe `CoverRepository` lookup and write behavior only; they are not book-opening latency or a speedup comparison.

Each of five runs measured an empty-cache lookup pass, cache population, then a read pass in book order. Median is the middle of five samples; p95 uses nearest-rank, which is the maximum for this sample size. Raw output is preserved in [`verification/v092/cover-benchmark/synthetic-container-runs.txt`](../verification/v092/cover-benchmark/synthetic-container-runs.txt).

| Placeholder books | Cold lookup median / p95 | Populate median / p95 | Warm lookup median / p95 | Warm hits / misses | Retained |
|---:|---:|---:|---:|---:|---:|
| 100 | 4 / 5 ms | 187 / 232 ms | 72 / 125 ms | 100 / 0 (100%) | 100 / 9,455,600 bytes |
| 500 | 19 / 26 ms | 923 / 1,355 ms | 290 / 556 ms | 354 / 146 (70.8%) | 354 / 33,472,824 bytes |
| 1000 | 37 / 73 ms | 2,106 / 2,667 ms | 379 / 430 ms | 354 / 646 (35.4%) | 354 / 33,472,824 bytes |

The 32 MiB byte budget is the active limit for these samples: one stored thumbnail occupies 94,556 bytes including the cache header, so 354 fit and 355 exceed 32 MiB. The configured 512-entry limit is not reached. These figures confirm the existing bound; they do not establish that it is suitable for real Switch storage or user collections. No cache budget was changed.

## Code and test evidence

- `src/main.cpp` now refreshes the existing disk hit/miss/size counters after both cache reads (including misses) and writes. The settings row shows the disk-only hit/miss counts in Turkish and English; it does not conflate them with SDL texture-cache hits.
- `tests/cover_repository_tests.cpp` checks cold miss, warm hit, and populated entry/byte statistics. Its existing cases cover cache corruption, budget eviction, safe clear, preservation of user files, and regeneration after clear.
- `tests/localization_tests.cpp` checks both directions for the new disk hit/miss label.
- Targeted desktop build completed for `PaperNX`, cover repository, synthetic cover performance, and localization targets. Targeted CTest passed **3/3**: `papernx_cover_tests`, `papernx_cover_performance_tests`, `papernx_localization_tests`.

## Still unverified

This run does not satisfy the requested 100/500/1000 real-document workload. Actual cold/warm decode time, first visible cover, scroll-fill latency, archive opens, metadata reopens, repeated decode count, upload queue delay, texture/RAM use, UI frame time, baseline comparison, and physical Switch SD performance were not measured. `COVER-01` and any cover speedup claim remain **UNVERIFIED**.

## Fixture-backed document decode workload — 2026-10-24

**Status:** exploratory host measurement; `COVER-01` remains **NOT PASSED**.

Added `tests/cover_document_benchmark.cpp`, registered as `papernx_cover_document_benchmark`. Each workload cycles through the real `sample_book.epub`, `sample_doc.pdf`, `comics/sample.cbz`, and `comics/sample-rar4.cbr` fixtures, copying those four documents to distinct paths until the set reaches 100, 500, or 1,000 entries. Each path gets its own cache identity. The cold pass opens each document, reads title/author from that same handle, renders a thumbnail and writes it through `CoverRepository`; the warm pass reloads the disk index and follows cache read → decode/write on misses, as the app does.

The benchmark was executed three times in the desktop Docker environment; all three exited successfully. The CTest registration also passed **1/1** in **19.85 seconds**. Times below are median / nearest-rank p95 (the maximum for three runs):

| Fixture copies | Cold first cover (ms) | Cold fill (ms) | Warm first cover (ms) | Warm full pass (ms) | Warm hits / misses per run | Retained cache |
|---:|---:|---:|---:|---:|---|---:|
| 100 | 25 / 25 | 521 / 640 | 2 / 2 | 94 / 96 | 100 / 0; 100 / 0; 100 / 0 | 100 / 8,526,350 bytes |
| 500 | 7 / 10 | 2,264 / 2,511 | 14 / 15 | 1,659 / 1,946 | 147 / 353; 156 / 344; 145 / 355 | 393 / 33,502,728 bytes |
| 1,000 | 11 / 15 | 4,761 / 5,831 | 12 / 13 | 4,411 / 5,010 | 0 / 1,000; 0 / 1,000; 0 / 1,000 | 393 / 33,502,728 bytes |

The 32 MiB cap is active at 500 and 1,000 copies; the 512-entry cap is not. In this sequential full-library walk, cache misses evict later entries before the scan reaches them, resulting in repeated document opens at 1,000 items. This is useful evidence of the current budget's limit under this workload; it does not prove the same hit rate for PaperNX's viewport-driven SDL queue.

**Limits:** These are four repeated fixture contents, not 1,000 distinct user books. The harness does not instantiate `PaperNxApp`, measure SDL queue wait/texture upload/frame time, or reproduce interactive scrolling. Cold title/author reads share the thumbnail document handle; the warm pass assumes metadata is already persisted and therefore does not measure catalog metadata-only reopen. It measures no Switch SD latency, memory high-water, solid-RAR decode cost, or before/after improvement. MuPDF emitted an `unknown epub version: 3.0` warning for EPUB fixture opens; the process and CTest still passed. Do not claim a speedup or mark `COVER-01` complete from these figures.

## Application queue follow-up — `96d0d79`

Source review found that `prioritizeCovers()` already removed pending, off-screen requests, but completed results remained in `completedThumbs_` and could consume the two-per-frame SDL upload budget after a scroll. `processCompletedThumbnails()` now applies the same visible-ID set before selecting upload candidates. It clears the stale pending ID/state but leaves the already-written disk cache entry, so revisiting the book can use the normal warm read. Placeholder state is read under `queueMutex_`, matching worker writes and removing that map read/write race.

`tests/modern_library_tests.cpp::runCoverQueuePriority()` passed within the targeted `papernx_modern_library_tests` CTest (**1/1**, 10.65 sec). The test proves stale completed surfaces are not uploaded, the visible surface is uploaded, and the stale ID/state is released. It does not time queue latency or model fast interactive scrolling; it supplies no FPS, memory or Switch SD measurement. `COVER-01` remains **NOT PASSED** and no speedup is claimed.

## Metadata reorder and catalog ownership — `a52fa06` / `a9da5dd` / `4ab2c02`

The visible ID set is now calculated after metadata updates the catalog and reorders the library. The app-level regression moves a completed thumbnail from outside to inside the visible list through a metadata title change and confirms that it is uploaded. The thumbnail worker no longer reads `CatalogStore` on cache hits; stale inventory metadata is already scheduled through the bounded metadata queue at startup and after successful scans. A failed metadata open releases its deduplication key for retry on a later scan.

The `papernx_modern_library_tests` build passed and CTest passed **1/1** (13.13 sec). A missing-file then valid-EPUB scenario verifies retry publication. Switch ELF/NRO and RomFS artifact verification also passed at source `4ab2c02`. These are correctness checks, not performance measurements. Queue wait, interactive scroll fill, frame time, RAM and Switch SD remain unmeasured; `COVER-01` remains **NOT PASSED** and no speedup is claimed.

## Disk budget comparison — source `d7f3fd3`

The fixture-backed benchmark was extended to compare 32 MiB and 48 MiB disk budgets for the same 100/500/1,000 repeated-document workloads. Based on this comparison, `CoverRepository`'s default disk budget is now 48 MiB. The maximum remains 512 entries; the separate in-memory thumbnail texture cache remains 32 MiB.

| Paths | Budget | Cold fill | Retained | Sequential revisit hits / reopens | Revisit time |
|---:|---:|---:|---:|---:|---:|
| 100 | 32 MiB | 574 ms | 100 / 8,526,350 bytes | 100 / 0 | 103 ms |
| 100 | 48 MiB | 454 ms | 100 / 8,526,350 bytes | 100 / 0 | 90 ms |
| 500 | 32 MiB | 2,575 ms | 393 / 33,502,728 bytes | 136 / 364 | 1,973 ms |
| 500 | 48 MiB | 2,468 ms | 500 / 42,631,750 bytes | 500 / 0 | 411 ms |
| 1,000 | 32 MiB | 5,055 ms | 393 / 33,502,728 bytes | 0 / 1,000 | 5,029 ms |
| 1,000 | 48 MiB | 4,610 ms | 512 / 43,654,912 bytes | 0 / 1,000 | 4,062 ms |

The current-source `papernx_cover_performance_tests` synthetic pass separately reports 500/500 hits at the new default and 512 hits / 488 misses for 1,000 entries. The real-fixture sequential full pass still thrashes at 1,000 because it exceeds both the retained working set and traversal order. Four source contents are repeated across unique paths; these results support a bounded 500-entry workload, not a universal speedup or a prediction for Switch SD/interactive scrolling. Larger byte or entry limits remain unchanged pending real-device and viewport measurements.

At `d7f3fd3`, `PaperNX` rebuilt and `papernx_cover_tests`, `papernx_cover_performance_tests`, and `papernx_cover_document_benchmark` passed **3/3** CTest in **48.32 sec**. The current-source Switch AArch64 ELF/NRO and RomFS verifier also passed; NRO SHA-256 `ddf77b04cdd23c24f89b9c73ae0951d0d20a713209c26905a8c1c3cae56533ef`, CA bundle 118 certificates / 200,116 bytes. Full native suite, UI frame timing and physical SD measurement were not run; `COVER-01` remains partial.

The focused ASan/UBSan build of `papernx_cover_tests` and `papernx_cover_performance_tests` passed **2/2** (11.99 sec). This covers repository safety and synthetic eviction; it does not instrument the full application or Switch runtime.

The app-level `papernx_modern_library_tests` rebuilt and passed **1/1** (13.83 sec) at the same source revision. It checks visible cover upload priority and stale queue cleanup after results arrive. It does not measure UI queue latency, scroll fill, FPS or Switch SD.

## Final app-level benchmark — source `0eaf3b38defa5a72b2895967f2c2bfb7390b8153`

This section supersedes the earlier four-content fixture benchmark as the current G6 evidence. The current benchmark runs the actual `PaperNxApp` SDL dummy/software path: library scan, metadata worker, thumbnail request/worker, completed queue, SDL texture upload, render, scroll, category changes, book open/return, cache clear and reload.

**Build identity:** app `v0.9.1` development version; source tree SHA-256 `8b2da6f807f74065830dec551933ca321c32de1e9eb78a37d0de623448a890ef`; renderer `software`. Desktop build receipt and every telemetry record use the same source identity. This does not represent Switch GPU or SD-card performance.

### Corpus

The deterministic, copyright-safe 1,000-document corpus contains generated EPUB, PDF and CBZ content, plus five checked-in synthetic CBR sources reused in the 40 CBR paths. It has 965 unique SHA-256 contents overall (3.5% repeated content). The 100 and 500 runs use ordinal subsets of the same corpus. EPUB titles/body metadata vary in length; one EPUB in every ten has no cover. Covers vary from small images to 640×900; PDFs use varied embedded image dimensions; CBZ files contain one to five pages and nested entries.

| Format | Files | Unique contents | File size min / median / max bytes |
|---|---:|---:|---:|
| EPUB | 320 | 320 | 1,464 / 49,986 / 1,394,907 |
| PDF | 320 | 320 | 29,430 / 66,250.5 / 1,392,324 |
| CBZ | 320 | 320 | 462,005 / 571,004.5 / 2,077,542 |
| CBR | 40 | 5 | 894 / 966 / 1,108 |

Corpus total: 319,840,078 bytes. CBR repetition is explicit and is the principal corpus limitation. The first viewport has five book rows but four cover-eligible items because the first item is a coverless EPUB; telemetry reports four requested/four loaded. Fill percentages use eligible cover requests, and decode/upload failures stay in the denominator.

Each size/mode has three separate application runs. Each repetition runs cold then warm using the same app root, so warm starts with the persisted cache state from its cold run. Distributions use nearest-rank. For per-run metrics, `n=3`, so p95 and p99 equal the largest of the three values. Decode, frame and upload-wait distributions pool the bounded telemetry rings; the JSON report records each metric's `n` and method.

### Startup, scan, viewport, sweep and memory

All times are milliseconds; RSS is KiB. `First viewport wait` is the harness wait-loop duration. `First visible/fill50/fill95` are telemetry times from the first cover request and are shown separately because a warm cache can resolve before the harness enters its wait loop.

| Books | Mode | Startup median / p95 / p99 | Scan median | First visible | Fill 50% | Fill 95% | Full sweep median | Peak RSS median KiB |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 100 | cold | 387.2 / 435.8 / 435.8 | 126.0 | 199.6 | 305.6 | 554.0 | 5,598.3 | 60,912 |
| 100 | warm | 741.7 / 890.2 / 890.2 | 155.7 | 226.0 | 226.1 | 411.3 | 6,453.4 | 60,084 |
| 500 | cold | 414.2 / 457.0 / 457.0 | 1,215.5 | 182.8 | 295.3 | 496.6 | 32,401.8 | 89,264 |
| 500 | warm | 2,225.6 / 2,260.6 / 2,260.6 | 1,221.4 | 1,292.0 | 1,292.0 | 1,294.4 | 33,046.8 | 88,392 |
| 1,000 | cold | 357.9 / 446.6 / 446.6 | 2,322.4 | 166.7 | 263.3 | 462.5 | 65,747.4 | 94,320 |
| 1,000 | warm | 4,419.5 / 4,839.7 / 4,839.7 | 2,630.1 | 2,712.0 | 2,712.1 | 2,714.6 | 65,020.2 | 95,688 |

### Interaction, decode, queue, frame and counters

Per-run interaction/counter values below are medians. Decode, frame and upload-wait percentiles are pooled from the 3× bounded rings; their exact sample counts are in `summary.json`. The benchmark's upload wait is completion-queue arrival to SDL upload attempt.

| Books | Mode | First viewport wait | Fast scroll | Category change | Open/return | Clear/reload | Decode median / p95 / p99 | Upload wait median / p95 / p99 | Frame median / p95 / p99 |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | cold | 514.8 | 646.0 | 364.8 | 138.5 | 277.2 | 9.04 / 59.87 / 158.25 | 1.62 / 3.45 / 6.92 | 2.65 / 3.89 / 8.35 |
| 100 | warm | 185.0 | 593.4 | 434.1 | 234.7 | 332.5 | 9.59 / 60.96 / 189.07 | 1.90 / 4.75 / 123.94 | 2.97 / 5.24 / 10.27 |
| 500 | cold | 450.5 | 606.2 | 403.7 | 202.2 | 1,088.6 | 9.79 / 62.39 / 172.80 | 1.84 / 4.00 / 5.48 | 2.73 / 5.21 / 10.78 |
| 500 | warm | 2.6 | 536.5 | 455.4 | 258.5 | 1,260.0 | 9.67 / 57.92 / 166.98 | 1.85 / 3.71 / 4.44 | 3.02 / 4.71 / 7.58 |
| 1,000 | cold | 418.5 | 528.2 | 1,404.6 | 404.8 | 2,440.3 | 11.88 / 70.60 / 180.76 | 1.99 / 4.22 / 6.54 | 3.40 / 9.18 / 12.76 |
| 1,000 | warm | 2.7 | 642.4 | 961.1 | 308.1 | 1,806.3 | 11.21 / 61.51 / 166.78 | 1.65 / 3.91 / 5.41 | 2.78 / 3.93 / 6.09 |

| Books | Mode | Cover requests | Metadata opens / avoided | Archive opens | Disk cache hits / misses | Successful uploads | Upload failures | Stale discards |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 100 | cold | 183 | 100 / 70 | 42 | 4 / 70 | 66 | 0 | 113 |
| 100 | warm | 184 | 0 / 0 | 6 | 4 / 70 | 66 | 0 | 114 |
| 500 | cold | 856 | 500 / 326 | 186 | 5 / 326 | 309 | 0 | 530 |
| 500 | warm | 857 | 0 / 0 | 6 | 4 / 326 | 309 | 0 | 531 |
| 1,000 | cold | 1,182 | 1,000 / 651 | 366 | 4 / 651 | 616 | 0 | 532 |
| 1,000 | warm | 1,182 | 0 / 5 | 6 | 4 / 651 | 616 | 0 | 532 |

At 1,000 books the observed cache reuse is low (4 hits / 651 misses); it is not hidden or presented as a successful high-hit workload. The 48 MiB disk / 512-entry limits and 32 MiB texture cache were not increased. No production performance optimization was made during this final benchmark, so a before/after comparison is **N/A**; older repeated-fixture results are not comparable to this distinct-content corpus.

### Raw evidence and limits

- Summary: `build-p1/final-0eaf3b3/cover-benchmark/summary.json` and `summary.md`.
- Raw application outputs: the 18 `raw-<100|500|1000>-<cold|warm>-<1|2|3>.json` files in that directory.
- Generated corpus and content-hash manifest: `build-p1/final-0eaf3b3/cover-benchmark/corpus/`.
- All measured runs used SDL dummy/software renderer in the desktop Docker container. No physical Switch, GPU renderer, SD-card scan, Joy-Con, real TLS or hardware frame-time test ran. Do not label these results Switch hardware PASS.
