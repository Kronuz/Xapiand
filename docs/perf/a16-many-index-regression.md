# A16 many-index search regression

State: the bisect and the pool probe are done; the strata-main arm (jobs at the end) is pending.

## Problem

x7-gate's A16 (`harness/wide_search_check.py gate`, 2,500 indexes, 10 searches over `/w*/`, strict) went from
p50 108 ms with 1,382 indexes open after the first search (job 261111-200215-2567dc) to p50 23,136 ms with 200
open (job 261113-125946-4ec27a).

The two runs' binaries were not the commits their logs print. Both used the `x7-timing` slot's `build-cur`:
the first was built at x7-gate 49ebb4481d (src equal to master 27d45f101), the second at 0a0321691e (src equal to
master 6ede7a0ce). The harness change between them (77d649648 to 6dcc770ff) only adds one `/:metrics` read
between searches and the SR3 lag lines; it does not touch what a search does.

## Method

Each arm: a RelWithDebInfo build (LTO off, frame pointers, as `x7-timing`) in its own slot `a16-<commit>`, then
the same strict gate command with master 6400f0390's harness (`--ref H=6400f0390`). Seven first-parent landings
between 27d45f101 and 6ede7a0ce touch src or the build: 7d9afe612, 5c0738825, 55741956f (xapiand-strata-bump),
60f5a34be, 88139a40e, 08a85cca9, 05d36bdbc, 46edfb23e.

| commit | build | gate | p50 | p99 |
|---|---|---|---|---|
| 27d45f101 (good end) | 261113-150220-5a803a | 261113-150222-cf367d | 98 ms | 577 ms |
| 55741956f (xapiand-strata-bump, first bad) | 261113-151340-20f3c2 | 261113-151345-b7a965 | 19,977 ms | 29,575 ms |
| 08a85cca9 | 261113-150235-fc7ccc | 261113-150237-caacd8 | 24,147 ms | 29,571 ms |
| 6400f0390 (master) | 261113-150224-2942a6 | 261113-150227-eab426 | 23,507 ms | 48,566 ms |

55741956f is the first bad landing measured. The landing before it, 5c0738825 (RESTORE integer typing plus a
master merge that moved prospect to b20e7c5; strata stayed at 5d742b24, which Xapiand declares first), is
being measured to close the step: build 261113-155403-506346, gate 261113-155405-bc89da (pending).
55741956f's first build (261113-150229-5be953) failed on a clang segfault compiling handler.cc while four
builds ran at once, not on a fetch; the identical rebuild passed.

The pool probe (`docs/perf/a16_probe.sh`, 3 searches; its `sample` step never ran because master's harness
does not print the open count it waited for):

| build | job | default pool (soft_open 200) p50 | `--database-pool-size 5000` p50 |
|---|---|---|---|
| 27d45f101 | 261113-151517-52d2dc | 1,018 ms | 134 ms |
| 55741956f | 261113-151512-1f89a2 | 19,009 ms | 156 ms |

With every index kept open the bump costs 16% (134 to 156 ms); the 200x is all in reopening.

## Mechanism

xapiand-strata-bump moved strata from 5d742b24 to 86fe7c47. Two strata changes in that range meet here:

1. The index runs on the manifest log (strata f2089f9f, c78f8702, c35ce7e0): a writable open syncs the log and
   the directory under one device flush (`sync_files_and_directory`, F_FULLFSYNC on macOS) before it reports
   the head.
2. The catalog posts a sweep pass as soon as its own soft caps are a pass over (strata 5d3c948a, 062182b2),
   instead of only on its 1 s period. `soft_open` is `--database-pool-size`, default 200 (src/opts.cc), and
   a search's batch is soft_open / 2 reactors = 100 (the "peak pinned 100").

A search over 2,500 indexes through a catalog held near 200 misses on about 2,300 of them every time; at
55741956f each miss costs 19,977 ms / 2,300, about 8.7 ms, against 0.43 ms an open in the good build's first
search (1,080 ms for 2,500, job 261111-200215-2567dc). At 27d45f101 the period-only sweep let the open count
pile to 1,382, so later searches mostly hit (98 ms p50 over 10 searches, 1,018 ms over 3). With the pool at
5,000 nothing is evicted and both builds search in 134-156 ms: the flush is paid only on reopen.

Strata main has sync-at-first-commit (strata 34aa3691, landed cb5294b8): an open leaves the inherited log and
directory sync owed to its first commit, so a read never flushes. Xapiand master still pins 86fe7c47. The
fix arm pins strata main 397c02d5 (CMakeLists only, branch quick/strata-pin 0d97b2da7): build
261113-155315-e7c6f7 (with harness/pins_check.py), gate at the default pool 261113-155318-cb2a87, gate at
`--database-pool-size 5000` 261113-155320-bf8f29 (pending).

## Recommendation

1. Land quick/strata-pin once its gate shows the default-pool search back near the 27d45f101 numbers: it
   removes the per-open device flush, the 20x part (8.7 ms to about 0.4 ms an open).
2. Size the pool from resources, not 200. An open index costs 35-42 KiB resident (35.1 KiB at 10,000 open,
   job 261111-165347-d979c7; 42.1 KiB at 1,382, job 261111-200215-2567dc), so 1 GiB holds about 25,000 open
   indexes, and the catalog already caps descriptors from RLIMIT_NOFILE. A default of
   min(descriptor soft cap, a memory share / 40 KiB) keeps a 2,500-index search all hits (134 ms) on any
   host with a few hundred MiB to spare; 200 makes it reopen 92% of the indexes on every search.
3. Eviction eagerness: the eager soft-cap pass is right for a bounded footprint, but at millions of indexes
   per host any search wider than soft_open is a loop larger than the cache, which S3-FIFO cannot hold, so
   every such search pays the full reopen cost (2,300 x 0.43 ms = about 1 s at 2,500 even without the flush).
   The cheap open matters more than the cap: measure the reopen cost after the pin bump and make it the
   target (a read-only open that touches no log, the lazy segments already in strata), rather than relaxing
   the sweep back to a 1 s pile-up that bounds nothing.
