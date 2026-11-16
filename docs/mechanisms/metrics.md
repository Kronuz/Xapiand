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

RESULTS
