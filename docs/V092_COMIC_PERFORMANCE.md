# PaperNX v0.9.2 comic worker measurements

**Source:** `1daedb63246384a73afb979978230f75716c2165` (G10 scan fix; no comic source changed after it).  
**Measured branch HEAD:** `b4e7e2794bf7c29be8b4dd9cfc6436cf98323bbe`.  
**Environment:** desktop Docker container, SDL software renderer. These are host observations, not Switch results.

## Large RAR5 reader path

The existing `tests/fixtures/comics/performance-37.cbr` fixture is 43,484,977 bytes and contains 37 pages. Its generator documents RAR5's stored-file method; this is a large archive-I/O exercise, not a solid-RAR benchmark. The fixture was not modified by the run (SHA-256 before and after: `EB9EF2C91FEF3AED592F4A14EBF4524BAC13548E94D1BB9C717ED2B8E79B0E5A`).

Command:

```sh
docker compose run --rm desktop bash -lc 'SDL_VIDEODRIVER=dummy build-desktop/papernx_comic_performance_tests tests/fixtures/comics/performance-37.cbr'
```

Observed in one run:

| Operation | Result | UI rasterizations |
|---|---:|---:|
| Four idle/toolbar cycles | 43 ms | 0 |
| 30 comic curl frames | 347 ms | 0 |
| 30 spread-pan frames | 154 ms | 0 |
| Worker idle at 1/3/10/30 seconds | Opens stayed `2`, closes stayed `1` | — |
| Suspend → resume → stop | Counts ended at `3` opens / `3` closes | — |

The direct test invocation passed. It copies the source archive into a temporary app root, then removes that temporary root. The source fixture remained byte-identical.

## Solid archive and CTest worker coverage

`papernx_comic_document_tests` passed **1/1** in 1.25 seconds. It exercises existing CBZ, RAR4, RAR5, RAR4-solid and RAR5-solid fixtures, including random page access and concurrent document budgets. This validates format and ownership behavior; it does not measure a large solid archive's decode count or latency.

The no-argument `papernx_comic_performance_tests` CTest passed **1/1** in 32.91 seconds on an 8-page / 4,435-byte CBZ fixture. The CTest log records 54 ms for four idle/toolbar cycles, 499 ms for 30 curl frames, and 199 ms for 30 spread-pan frames; UI rasterizations were zero in each loop. Its 30-second idle and suspend/resume open/close counters matched the large RAR5 run.

## Limits

- These are single-run software-renderer timings; there is no before/after baseline, median, or p95. They do not establish a performance gain.
- No Switch FPS, frame-time, SD latency, peak RAM, texture high-water, or large solid-RAR decode counter was measured.
- The existing 64 MiB page-cache limit was not increased. `PageCache::currentBytes()` stayed below the test's 36 MiB assertion for the exercised view, which is not total process memory.
- Full CTest, ASan/UBSan and physical Switch acceptance remain open.
