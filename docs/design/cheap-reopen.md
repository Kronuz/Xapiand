# Cheap reopen: an evicted index reopens near the cost of finding it open

Status: design, measured at main 4adce97ce (strata 397c02d5). D2 is built (Xapiand:bd6e5d50c); D1, D3 and D4 wait
for the file sets and quests named under "What is left".

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

H1 confirms the lane: concurrent pins queue on the catalog's one group. H2 gives the lane eight slots: the same job's
main arm ran 1,536-2,291 ms beside 6 light holds, the lanes 383-415 ms, 3.0x the pool-5000 run. The rest is CPU:
2,300 reopens x 300 us is 0.69 CPU-seconds a search, which the strict hold's share of a loaded host does not
parallelize eight ways. The first search at pool 5000 (2,500 cold opens) went from 974 to 400 ms with lanes.

## Decisions

- **D1. Lanes: a catalog per pool partition.** strata's cold-tier design already says it (D4, "the catalog, one per
  pool partition"): a partition is the unit of one lane, and one budget (`resource_budget`) serves the process.
  DatabasePool holds P catalogs, picks one by the key's hash, and splits soft_open between them; P is the work
  pool's thread count, capped at 8 until a measurement says otherwise. This replaces H2's probe, which breaks the
  catalog's single-lane assumption. Rejected: a multi-lane group inside one catalog (strata would have to make
  every transition safe beside another, which D4 avoids by partitioning).
- **D2. The directory's open is its existence check** (built, Xapiand:bd6e5d50c): resolve opens `<index>/strata`
  once and maps ENOENT or ENOTDIR to `index_not_created`, instead of a stat and then the open. About 7% of a
  reopen's CPU. A permission error now fails the pin as itself, not as a missing index.
- **D3. A search pins its batch concurrently**: H1's `when_all` in `search_batch`. Alone it gains nothing; with D1
  it is how the lanes fill.
- **D4. Fewer path walks per reopen** (CPU, about 40% of a reopen):
  - strata, its own quest: `recover` takes each file's size from one `fstatat` on the directory descriptor (or the
    scan's own attributes: `getattrlistbulk` on macOS) instead of `open`+`fstat`+`close`; and the catalog keeps a
    closed index's recovered log state in its ghost entry, so a reopen whose manifest log has the same inode and
    size skips the scan and the parse (about 25% of a reopen; bounded by the ghost, half of soft_open).
  - Xapiand: the WAL and DataStorage scan the same directory once per open, together (about 20%). Both are code
    the plan replaces, so this is a single shared scan, not an optimization of either.
- **Not chosen: keeping evicted indexes warm.** Warm keeps the index and its directory lock, so it is bounded by
  descriptors like open (soft_open already counts warm); it does not help a loop wider than the pool. Sizing the
  pool from memory is quest pool-from-memory.

Expected: D1 + D3 measured 3.0x (H2); D2 and D4 cut a reopen's CPU by about half, which brings the lanes under the
2x target when the host has cores to give. A2 decides with an exclusive job.

## Durability

Nothing here changes what a reopen reads: strata's `open` and its recovery, the WAL replay and the commit chores are
the same calls, in a different lane. D4's remembered log state is trusted only when the log file is the one the
close left (same inode and size, under the directory lock), otherwise the reopen recovers from the files.
`tests/test_cheap_reopen.cc`: an evicted index and an index a crashed process left reopen at exactly their last
commit, and a missing index is not_found and leaves no directory.

## Memory

An open index costs 40.1 KiB resident at 2,500 open (job 261118-060120-5b74f3, pool 5000). D1 adds P catalogs'
fixed state (slot chunks, S3-FIFO queues, a ghost of soft_open/2/P hashes each); D4's remembered state is one
manifest log state per ghost entry. Both are to be measured per index before landing.
