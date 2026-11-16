> Status: CURRENT (2026-11-16) the open-descriptor gauge in `GET /:metrics`, as built on `quest/xapiand/metrics-fd-count`. Not reviewed.

# The open-descriptor gauge

## What

`xapiand_file_descriptors` is the number of descriptors the node has open, set on every scrape by
`XapiandManager::server_metrics_impl` (`src/manager.cc`) from `get_open_files_per_proc()` (Kronuz/system,
`system.cc`). The scrape runs on a serving loop, so what the call costs is a loop turn.

## How

1. **Listing.** Linux counts the entries of `/proc/self/fd`, macOS those of `/dev/fd`, skipping the directory's own
   descriptor (the listing holds one for itself). The cost follows the open descriptors (about 40), not
   `RLIMIT_NOFILE`.
2. **The scan.** FreeBSD, and any host where the listing cannot be opened, polls every descriptor up to the highest one
   (`poll` with `POLLNVAL`). FreeBSD's `/dev/fd` lists only 0 to 2 without `fdescfs`, so it is not used there;
   `F_MAXFD` bounds the scan to the highest open descriptor (that slot included).
3. **The old scan.** It polled up to the limit on every platform: 52-56 ms of loop time per scrape at a limit of
   1,048,576, 0.04 ms at 1,024 (`docs/perf/restore-stall.md` on `explore/restore-stall`).

## Tests

`tests/test_manager_fd_count.cc` opens 25 files and checks the count rises by exactly 25, agrees with an `fcntl`
probe of every descriptor, falls back on close, and counts a descriptor far above the rest. The same checks run in
Kronuz/system's `test/test.cc`.

## Measured

Job 261116-083148-c3780d (`harness/metrics_scrape_ab.sh`, `quiet strict`, macOS, `RLIMIT_NOFILE` 1,048,576, one HTTP
loop, 40 scrapes and 40 `GET /` per arm, ABBA over 3 rounds; the spin probe logs every loop turn over 3 ms):

| | before (limit scan) | after (listing) |
|---|---|---|
| `GET /:metrics` median | 69-94 ms | 0.67-1.18 ms |
| `GET /` median (floor) | 0.51-0.91 ms | 0.67-0.93 ms |
| loop turns over 3 ms, of 40 scrapes | 41-67 (max turn 85-130 ms) | 0 in 3 arms, 1 in one, 14 and 36 in the others (max turn 3.5-9.6 ms) |

The scrape costs about as much as `GET /` after the change. The after arms that show turns over 3 ms also show `GET /`
maxima of 11 ms in the same arm: the job ran beside about 10 light holds (strict shares macOS cores with them), so
those turns are host contention, not the scrape; the quiet arms show none. A quiet-host rerun (`quiet exclusive`)
has not been made.
