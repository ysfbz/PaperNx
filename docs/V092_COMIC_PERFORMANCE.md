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

## Current source checkpoint — G1 cold pages and archive counters (`a6e01b3`)

**Source:** `a6e01b3b937a8d350346d0d14accc971e60af594`. Desktop build of `PaperNX` and the five reader/comic targets passed. Focused CTest passed **7/7** in 129.42 seconds: comic performance, its fixture setup, 37-page RAR performance, reader input, reader spread, comic app and comic document tests. The run included cold paginated and continuous comic loads; `comicTextureBuilds_` did not increase on those paths, and SDL texture uploads remain in `processComicPages()` on the UI thread.

Current single-run `SDL_VIDEODRIVER=dummy` measurements:

| Fixture | Four idle/toolbar cycles | 30 curl frames | 30 pan frames | Worker/archive counters |
|---|---:|---:|---:|---|
| 8-page CBZ, 4,435 bytes | 35 ms | 521 ms | 180 ms | 3 page-entry loads; cursor 3 opens / 0 reopens; 4 skipped entries; idle 2 opens / 1 close; stop 3/3 |
| 37-page RAR5 stored, 43,484,977 bytes | 31 ms | 406 ms | 163 ms | 3 page-entry loads; cursor 3 opens / 0 reopens; 4 skipped entries; idle 2 opens / 1 close; stop 3/3 |
| 3-page RAR5 solid, 954 bytes | 38 ms | 500 ms | 171 ms | 3 page-entry loads; cursor 3 opens / 0 reopens; 2 skipped entries; idle 2 opens / 1 close; stop 3/3 |

All measured render loops reported **0 UI rasterizations**. The worker test requests the same page again and verifies its worker-owned document's `pageEntryLoads` does not increase. These are host software-renderer observations from single runs, not a before/after comparison. `pageEntryLoads` counts comic archive payload loads observed by `load_image`; cursor metrics count archive traversal/reopen behavior. They do **not** measure codec-internal decompression CPU time or archive reads performed by the active UI-owned document during an unwarmed dimension lookup. RAR4 solid/random access remains covered by `papernx_comic_document_tests`, not by this new cursor counter.

The current Switch cross-build `docker compose run --rm switch bash -lc 'export MUPDF_PREFIX=/work/prefix-mupdf-switch; make all -j4'` linked `PaperNX.elf`, produced `PaperNX.nro`, and ran branding/RomFS validation (118 CA certificates, 200,116 bytes, and fonts). No artifact receipt/hash verification or physical-device test was run at this checkpoint. The untracked 37-page fixture retained SHA-256 `EB9EF2C91FEF3AED592F4A14EBF4524BAC13548E94D1BB9C717ED2B8E79B0E5A`.

## G1 error-visibility follow-up — source `d3265dd`

`d3265ddda21fd2ba5a1ba6fca0d72d95d8420e6b` keeps a failed page's message visible when the other sheet in the spread succeeds or is still loading. `papernx_comic_performance_tests`, `papernx_reader_spread_tests` and the corrected `papernx_comic_app_tests` each passed **1/1**; Switch `make all -j4` passed and produced the NRO. This follow-up changes error presentation only; the timing/counter rows above remain the measurements from `a6e01b3`, not new performance numbers. Full suite, sanitizer, artifact receipt and hardware checks were not run.
