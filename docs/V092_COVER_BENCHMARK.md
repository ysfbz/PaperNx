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
