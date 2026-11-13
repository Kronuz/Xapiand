# RESTORE chore cap

Question (loop-scheduling, after fix (c)): with RESTORE's writes in chores and the light/heavy lanes, loaded light
queries run 2.69x slower than master at p99 (job 261112-225448-4dfa5e: 60.7-86.9 ms against 21.7-33.5 ms) while
RESTORE alone runs 8,471 docs/s against master's 4,603 (job 261112-225454-7d59c5; pre-7c recorded 6,076). Suspect:
RESTORE's chores (`children = min(shards, workers/2)`, prepare `width = workers - children`, `workers` = the work
pool's threads - 1) take every work-pool worker and the HTTP loops lose their CPUs.

Knob (this branch only): `XAPIAND_RESTORE_CHORES=N` caps `workers` at N, so children + width <= N
(src/database/handler.cc, `DocIndexer::index`). Uncapped on this host: 16 pool threads, 5 shards: 5 children +
10 prepares.

Host: macOS, Intel i9-9980HK, 8 cores / 16 logical CPUs; 16 HTTP reactors, a 16-thread work pool. Harness:
[restore-cap/](restore-cap/) (rc.sh, sampler.py, report.py), from loop-scheduling's `.scratch/ab/`; binaries
`rel-4738337a3` (master) and `rel-eef7a92e9` (this branch, Xapiand:eef7a92e9). All jobs exclusive, at most 10 minutes.

## Answer

No cap does it. The cause is not RESTORE's chores taking the CPUs: the work pool is parked through the loaded
phase in every arm, and the CPU goes to the HTTP reactors. The only cap that brings loaded light p99 near master
(cap 2: 1.4x master instead of 2.3x) cuts RESTORE to 3,323 docs/s, 0.55x pre-7c and 0.76x master. Every cap that
keeps RESTORE at or above pre-7c (6, 8, uncapped) leaves light p99 at 62-68 ms, 2.2-2.5x master. Recommendation:
keep it uncapped; the chore cap is the wrong lever.

## Census (job 261113-001854-f59ba1)

One T4/T5 run per arm, with `ps -M` at 2 Hz (runnable threads) and a 1 s `sample` every 6 s (what each thread
does). The `sample` pauses distort that job's latencies (master's p99 read 460 ms), so it gives only the thread
picture; the sweep jobs give the latencies.

| Loaded phase | master | uncapped | cap 4 |
|---|---|---|---|
| node runnable threads, mean (p90) | 14.2 (16) | 17.8 (27) | 14.6 (20) |
| other runnable threads on the host, mean | 19.6 | 16.7 | 15.1 |
| node CPU, mean | 1,046% | 1,182% | 1,102% |
| work pool (Xapiand:W, 16): busy threads | 0.8 | 1.3 | 0.7 |
| HTTP reactors (16, unnamed): busy threads | 7.9 | 11.6 | 7.1 |
| `strata::docids::reader::docid` top-of-stack samples per 1 s sample | 15-94 | 743-887 | 22-43 |

1. The host is oversubscribed in every arm, master too: about 32-35 runnable threads on 16 logical CPUs, about
   half of them the load generator and the rest of the host.
2. The work pool is parked in `WorkPool::park` (`__ulock_wait2`) for 92-96% of the loaded phase in every arm.
   RESTORE's chores use about one of its threads, not fifteen, so capping them cannot give the reactors CPU that
   they are not losing to the pool.
3. Uncapped, the reactors run about 3.7 threads busier than master. About 1.7 threads of that sit in
   `docids::reader::docid` under `prospect::exhaustive_search::scan`: heavy searches on `accounts` decoding monotone
   docid blocks (the LEB delta walk at strata docids.hh:173). That is about ten times master's share, and light
   requests share those reactors.

Hypothesis, not tested: the RESTORE that loads `accounts` (20,000 documents with explicit ids, through the same
path and cap) leaves segments whose docids are monotone rather than contiguous when several children write shards
at once, so every exhaustive scan pays the delta walk. That would also explain the sweep's step at cap 2, the only
arm with one child.

## Sweep

RESTORE docs/s: T12 loadtest, one run per arm in mirrored order (job 261113-001857-72429c; master ran 4,095 and
4,681 in the same job). T4/T5: 64 heavy clients, mirrored pairs (job 261113-002535-f5df4d for caps 15/8/4, job
261113-002537-7e5eae for caps 15/6/2). The T4/T5 master reference is job 261112-225448-4dfa5e (median 27.6 ms,
heavy 29,540): the same harness and master binary, but not this session. The census job's master run was
distorted by `sample`, so this round has no clean same-session master run of T4/T5.

| Cap (children + prepares) | RESTORE docs/s | vs pre-7c 6,076 | vs master 4,388 | Loaded light p99 ms | vs master 27.6 | Heavy searches | vs master 29,540 | Jobs |
|---|---|---|---|---|---|---|---|---|
| uncapped, 15 (5 + 10) | 8,316, 8,086 | 1.35x | 1.87x | 69.6, 65.7, 66.0, 59.4 | 2.4x | 21.5k, 21.6k, 21.7k, 20.8k | 0.73x | 72429c, f5df4d, 7e5eae |
| 8 (4 + 4) | 7,059, 7,019 | 1.16x | 1.60x | 73.3, 62.2 | 2.5x | 22.0k, 22.0k | 0.75x | 72429c, f5df4d |
| 6 (3 + 3) | 5,948, 6,335 | 1.01x | 1.40x | 61.6, 62.3 | 2.2x | 22.0k, 22.4k | 0.75x | 72429c, 7e5eae |
| 4 (2 + 2) | 4,911, 4,806 | 0.80x | 1.11x | 52.3, 59.3 | 2.0x | 22.5k, 21.8k | 0.75x | 72429c, f5df4d |
| 2 (1 + 1) | 3,314, 3,331 | 0.55x | 0.76x | 41.9, 37.9 | 1.4x | 30.7k, 29.6k | 1.02x | 72429c, 7e5eae |

Ratios use each arm's median. Light p99 falls slowly from 15 chores to 4, about 1 ms for each chore removed, then
drops by a third at 2, where heavy searches also return to master's level. That step and the census point at the
number of children writing shards at once, not at total chore concurrency.

## Next

1. One arm with one child and wide prepares (children 1, width 14). If light p99 and heavy searches match cap 2
   while RESTORE keeps most of its speed, the lever is the children, not the cap.
2. Read the docids kind of `accounts`' segments after the setup RESTORE, master against uncapped. Contiguous
   against monotone would confirm the hypothesis; the fix then belongs in how the parallel writes assign shard
   docids or how segments merge, not in a cap.

## Follow-up: docids layout and one child

At Xapiand:15572ff5f, `XAPIAND_RESTORE_CHILDREN` caps the children alone; arm `k1` is one child and 14 prepares
(build 261113-025233-b5f5ae, `rel-15572ff5f`).

Docids layout (job 261113-025243-f6caf4, [restore-cap/layout.py](restore-cap/layout.py) reading each segment's
footer, table of contents and docids kind byte): after stall_load's setup RESTORE of `accounts` (20,000 documents,
`?commit`, 8 s settle), every arm leaves the same layout: master, uncapped, cap 2 and one child, twice each, all
5 shards hold 2 contiguous segments and 4,000 documents, span/docs 1.00. Nothing is monotone. The hypothesis is
refuted: the setup RESTORE does not leave `accounts` monotone under any cap. The monotone segments the census
caught (`docid` busy in heavy scans) must form during the loaded phase, when the light pairs PUT documents with ids
from 10,000,001 up into `accounts` and merges join them with the contiguous base; master pays that walk too, at a
tenth of the uncapped share. Why uncapped pays more is not pinned down: stall_load deletes its workdir on exit, so
this round has no post-phase layout.

RESTORE docs/s, T12 loadtest, mirrored (job 261113-025246-e60e91):

| Arm | Runs | Median | vs pre-7c 6,076 | vs master |
|---|---|---|---|---|
| master | 4,548, 4,612 | 4,580 | 0.75x | 1.00x |
| uncapped (5 children + 10) | 8,280, 8,312 | 8,296 | 1.37x | 1.81x |
| one child + 14 prepares | 5,803, 5,667 | 5,735 | 0.94x | 1.25x |

## After the loaded phase (job 261113-030545-b38c8b)

T4/T5 in mirrored order, master, one child and uncapped. Each run keeps its data (`stall_load.py --keep-workdir
--seg-snapshots`, added on this branch), and `layout.py` reads `accounts` as the loaded phase left it.

| Arm | Loaded light p99 ms | Heavy searches | `accounts` after the phase (all 5 shards) |
|---|---|---|---|
| master | 34.3, 27.1 | 29,018, 28,659 | 20,015 docs contiguous; 400 monotone in 5 segments, span/docs 9.7 |
| one child + 14 prepares | 26.5, 44.3 | 24,470, 24,449 | 23 contiguous; 20,435 monotone in 10 segments, 165 blocks, span/docs 539.5 |
| uncapped | 44.5, 56.4 | 22,395, 22,218 | 16-24 contiguous; 20,435-20,437 monotone in 10-11 segments, span/docs 539.5 |

The difference comes from merges, not docid assignment:

1. The light pairs PUT ids from 11,000,001 up (`pair_base = accounts + 10,000,000 + seed * 1,000,000`). Each takes
   the shard docid its id names, `args.new_did = (did - 1) / n_shards + 1` (src/database/shard.cc:852-853,
   `ShardEndpoint::do_replace_term`), so about 2.2M in every shard. That line is the same on master, and fix (c)'s
   `apply_slice` goes through the same `do_replace_term`. Master's pair segments show it: 70-95 docs at
   span/docs 8-11, the pairs' own id gaps.
2. The setup RESTORE leaves two base segments per shard (4,000 contiguous docs, docids 1-4,000) in every arm. The
   split differs. On master it is skewed: shard 1 holds seg-1 at 1.27 MB and seg-2 at 3.44 MB. On the heads it is
   balanced: 1.69/3.03 MB for one child, 2.56/2.16 MB uncapped.
3. On the heads, the merge policy joins the balanced pair during the unloaded phase. That merge also takes the
   small pair segments already flushed, so seg-10 holds 4.5 MB: docids 1-4,000 and a tail near 2.2M. The whole base
   becomes monotone, about 540x its count in span. On master the skewed base pair is never merged; the pair
   segments merge only among themselves, and the base stays contiguous. Master was lucky, not immune: once enough
   pair data builds up, a merge with the base would make it monotone too.

Every heavy exhaustive scan over `accounts` therefore pays the monotone delta walk on all 20,000 base docs on the
heads, and on 400 tail docs on master. That matches the census's 10x `docid` share. One child and uncapped share
the layout, which is why the cap does not change it. One child's better heavy throughput (24.5k against 22.3k)
comes from the CPU the uncapped writes leave free.

Where a fix belongs:

1. Not in handler.cc's docid assignment, and not in shard.cc:852 as a standalone change. Appending these writes at
   the shard's next docid would keep the base dense (expected: one contiguous segment of about 4,080 docs per
   shard). But the named docid is load-bearing: a `NEW_NUMERIC_KEY` document derives its id from the docid the
   shard assigns (`NumericId`, shard.cc:845-847). An explicit id that did not hold its docid would let a later
   auto id equal it, and a later PUT of that id would replace the wrong document.
2. In strata's merge selection (preferred, the smallest change that removes the cause). Do not merge segments
   whose docid ranges are far apart, for example by refusing a merge whose span/docs exceeds a bound such as 4.
   Expected layout: what master shows by luck, the base contiguous (span/docs 1.0) and the sparse tails merged
   among themselves (span/docs about 10, monotone, a few hundred docs).
3. In strata's docids format or reader, which explore/monotone-scan is measuring. A run-aware kind, contiguous runs
   plus exceptions, would keep a base-plus-tail segment O(1) on the run. It is not repeated here.

Light p99 this round: master median 30.7 ms, one child 35.4 ms (1.15x, but its two runs straddle master's), and
uncapped 50.4 ms (1.64x).
