# Cheap reopen: an evicted index reopens near the cost of finding it open

Status: measured at main 4adce97ce (strata 397c02d5). D1, D2 and D3 are built on quest/xapiand/cheap-reopen
(D3 at Xapiand:3b8d41c39); D4's strata half is a follow-up, its Xapiand half is refused. A2 (within 2x) is not met:
the best arm measured is 2.0x (H5).

## Problem

A search over more indexes than the pool keeps reopens most of them every time. At 2,500 indexes and the default
pool (soft_open 200, a batch of 100 per search), `harness/wide_search_check.py gate` takes p50 1,079 and 979 ms; at
`--database-pool-size 5000`, where nothing is evicted, 129 and 134 ms (job 261118-060120-5b74f3, strict, ABAB,
10 searches each, macOS x86_64, 16 CPUs). The target (D89, quest A2) is the pool-200 run within 2x of the pool-5000
run at the same commit. The workload is millions of indexes per host, so any search wider than the pool is a loop
larger than the cache and pays the reopen on every index it names.

## Where a reopen goes

`sample` over 8.7 s of pool-200 searches at main (job 261118-060438-5fb87b, 1 ms interval; each frame's inclusive samples, outermost match
per stack). The process is busy 7,241 thread-ms; the catalog's opens are
5,208 of them (72%), its closes 438 (6%), the search itself a few hundred. About 7.5 searches ran in the window,
each reopening about 2,300 indexes, so a reopen costs about 300 us of CPU against about 410 us of wall
((1,079 - 129) ms / 2,300):

| Part | thread-ms | share of the open | per reopen |
|---|---|---|---|
| resolve (pool.cc): `fs::exists` stat of `<index>/strata` | 356 | 7% | ~20 us |
| resolve: `PosixIO` constructor, `open(2)` of the directory by path | 633 | 12% | ~37 us |
| strata `recover`: per-file `open`+`fstat`+`close` for sizes | 753 | 14% | ~44 us |
| strata `recover`: `scan_directory` | 606 | 12% | ~35 us |
| strata `load_files` (eager maps: `defer_io` sets `eager_segments`) | 430 | 8% | ~25 us |
| strata `read_log` (open, read, parse the manifest log) | 350 | 7% | ~20 us |
| strata: the log's own `open_existing`, `start_merges`, `lock_directory` | 397 | 8% | ~23 us |
| Xapiand payload: `DatabaseWAL::execute` (a directory scan, the volume's header and slots) | 621 | 12% | ~36 us |
| Xapiand payload: DataStorage `get_volumes_range` (a second scan of the index directory) | 391 | 8% | ~23 us |
| the rest of the payload (`borrow`, metadata read) | ~150 | 3% | ~9 us |
| close (strata `close`, `~basic_index`, `~ShardEndpoint`) | 438 | | ~25 us |

Nearly all of it is path walks and directory scans: the index directory is scanned three times per reopen (strata,
the WAL, DataStorage) and every strata file is opened once for its size and again to read or map it.

The wall is a serial chain, not the CPU. The catalog runs every transition of its partition (opens, closes, the
sweep) in one executor group, and Xapiand's executor runs one item per group (`work_pool_executor(..., 1)`), so the
pool opens one index at a time; and `search_batch` (src/database/handler.cc) awaits each pin before it asks for the
next. Sixteen work threads sit idle (191,801 of the window's samples are waits) while one opens.

## Experiments

| Arm | p50 at pool 200 | at pool 5000 | job |
|---|---|---|---|
| main | 1,079, 979 ms (quiet); 1,536-2,291 ms beside 6 light holds | 129, 134 ms | 060120-5b74f3; 060951-f40471 |
| H1: `search_batch` pins its batch concurrently (`task::when_all`) | 1,105-1,945 ms: no gain | 128 ms | 060440-baf657 |
| H2: H1 plus 8 items per group (probe only: the catalog assumes one lane) | 383, 415 ms | 136 ms | 060951-f40471 |
| D1 (lanes) alone, against the merge-base f722ff939 | 884, 888 ms (base 948, 977) | 131, 143 ms (base 125, 153) | 131125-16ca52 |
| H3: D1 plus D3 (concurrent pins), against D1 alone | 291, 284 ms (D1 869, 843) | 118, 126 ms (D1 128, 129) | 150608-5ad670 |
| D1 + D3 at 3b8d41c39, against the merge-base f722ff939 | 285, 283 ms (base 959, 881) | 128, 129 ms (base 122, 123) | 184757-7f0046 |
| H4: up to 16 partitions (12 at pool 200), against D1 + D3 | 264, 262 ms (head 283, 281) | 125, 124 ms (head 125, 126) | 213949-011031 |
| H5: `recover`'s sizes by `fstatat` (strata probe) | 262, 262 ms (head 281, 282) | 123, 124 ms (head 124, 123) | 033144-b85265 |
| H4 + H5 | 251, 249 ms (H4 alone 266, 268) | 122, 124 ms (H4 128, 125) | 033144-b85265 |

H1 confirms the lane: concurrent pins queue on the catalog's one group. H2 gives the lane eight slots: the same job's
main arm ran 1,536-2,291 ms beside 6 light holds, the lanes 383-415 ms, 3.0x the pool-5000 run. The rest is CPU:
2,300 reopens x 300 us is 0.69 CPU-seconds a search, which the strict hold's share of a loaded host does not
parallelize eight ways. The first search at pool 5000 (2,500 cold opens) went from 974 to 400 ms with lanes.

D1 alone gains 8%: a profile of it (job 261118-145656-a26428) holds 0.76 opens in flight on 8 lanes, because
`search_batch` still pins one index at a time. With D3 the lanes fill: 3.0x faster at pool 200, 2.4x the pool-5000
run (2.2x at 3b8d41c39, job 184757-7f0046: A2 is not yet met). In that profile `open(2)` and `close(2)` are over half the busy time; strata `recover`'s per-file
open-for-its-size is the largest single caller, about a quarter of each open (D4's strata half).

H4 and H5 each take about 7% off the pool-200 run, and together 11%: head is 2.25x its pool-5000 run, H4 2.1x, H5
2.1x, both 2.0x (250 against 123 ms, still over). H5 replaces `open_existing(n)->size()` with the IO's
`entry_footprint(n)`, one `fstatat` on the directory descriptor PosixIO holds. A profile of head beside six light holds
(job 261119-033151-f32bdd, p50 692 ms) puts `close(2)` of those size probes at 72% of `run_open`, but removing them
in the exclusive run gains only 7%, so the profile mostly measures the other holds' file-system load.
What remains per reopen without them is `scan_directory`, `read_log` and `load_files` in strata (about 27% of a
reopen at main), the directory's open, and the payload's WAL and DataStorage scans (about 20%).

## Decisions

- **D1. Lanes: a catalog per pool partition.** strata's cold-tier design already says it (D4, "the catalog, one per
  pool partition"): a partition is the unit of one lane, and one budget (`resource_budget`) serves the process.
  DatabasePool holds P catalogs, picks one by the high bits of the key's multiplied hash (the catalog shards its
  slots by the key's own hash, which the pick must not correlate with), and splits soft_open between them. P is
  the work pool's threads, at most 8, and at most one partition per 16 indexes of soft_open, so a small pool is
  one partition and behaves as before; the default pool (200) on 8 or more threads is 8 partitions of 25.
  The budget is the process's: each partition's sweep hears its soft-cap crossings, and a descriptor overage
  is relieved by every partition's pass, not split (soft_open sits under the descriptors' cap, so it is rare). This replaces H2's probe, which breaks the
  catalog's single-lane assumption. Rejected: a multi-lane group inside one catalog (strata would have to make
  every transition safe beside another, which D4 avoids by partitioning).
- **D2. The directory's open is its existence check** (built, Xapiand:bd6e5d50c): resolve opens `<index>/strata`
  once and maps ENOENT or ENOTDIR to `index_not_created`, instead of a stat and then the open. About 7% of a
  reopen's CPU. A permission error now fails the pin as itself, not as a missing index.
- **D3. A search pins its batch concurrently** (built, Xapiand:3b8d41c39): `search_batch` pins its batch with
  `when_all`, which rethrows the first error after every pin ends, so a pin's failure never leaves a sibling
  running. Alone (H1) it gains nothing; with D1 it is how the lanes fill (H3). wide-search-loop changes the same
  loop (a budget checkpoint per index); its merge keeps the checkpoint in the loop and the pins after it.
- **D4. Fewer path walks per reopen** (CPU, about 40% of a reopen):
  - strata, its own quest: `recover` takes each file's size from one `fstatat` on the directory descriptor (or the
    scan's own attributes: `getattrlistbulk` on macOS) instead of `open`+`fstat`+`close`; and the catalog keeps a
    closed index's recovered log state in its ghost entry, so a reopen whose manifest log has the same inode and
    size skips the scan and the parse (about 25% of a reopen; bounded by the ghost, half of soft_open).
  - Xapiand: the WAL and DataStorage scan the same directory once per open (about 20%). Refused by the lead: both
    are code the plan replaces.
- **Not chosen: keeping evicted indexes warm.** Warm keeps the index and its directory lock, so it is bounded by
  descriptors like open (soft_open already counts warm); it does not help a loop wider than the pool. Sizing the
  pool from memory is quest pool-from-memory.

Measured: D1 + D3 make pool 200 3.4x faster (959 to 283 ms) and leave it 2.2x the pool-5000 run. D4's `fstatat`
half and more lanes (H4) reach 2.0x together. Only the remembered log state skips the scan and the parse that
remain, and it is the one mechanism left that could reach 2x; it is unmeasured.

## Durability

Nothing here changes what a reopen reads: strata's `open` and its recovery, the WAL replay and the commit chores are
the same calls, in a different lane. D4's remembered log state is trusted only when the log file is the one the
close left (same inode and size, under the directory lock), otherwise the reopen recovers from the files.
`tests/test_cheap_reopen.cc`: an evicted index and an index a crashed process left reopen at exactly their last
commit, and a missing index is not_found and leaves no directory.

## Memory

An open index costs 40.1 KiB resident at 2,500 open at main (job 261118-060120-5b74f3, pool 5000). D1 + D3 cost
41.1-41.9 KiB against the merge-base's 40.0-40.3 in the same job (184757-7f0046), about 1.3 KiB (3%) more: P
catalogs' fixed state (slot chunks, S3-FIFO queues, a ghost of soft_open/2/P hashes each), spread over the open
indexes. H4 and H5 change neither (41.2-41.5 KiB, job 033144-b85265). D4's remembered state is one manifest log
state per ghost entry, to be measured when it is built.
