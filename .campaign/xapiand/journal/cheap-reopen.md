# cheap-reopen: journal

State: waiting on jobs 261119-112058-686d62 261119-112059-d1220b 261119-112102-8426f3
Quest: .campaign/xapiand/quests/cheap-reopen.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

11-19: D135 amended A2 to 2.2x (owner). wide-search-loop landed; master merged at Xapiand:0dc08322a. search_batch
keeps both: the wide-search pin hold and the per-index budget checkpoint first, then the batch's pins together
(`when_all`, D3), as the design note says. Syntax check (`.scratch/syntax.sh`, light hold) of handler.cc,
test_node.cc and test_pool.cc passed. Queued at 0dc08322a: A1 profile 261119-112058-686d62 (rerun of 060438-5fb87b),
A2 pair 261119-112059-d1220b (rerun of 184757-7f0046, base f722ff939), suite 261119-112102-8426f3.

For the correctness reviewer, probe:
- Cache staleness after a write: an index written (or replaced) while evicted, then reopened, serves its last
  commit; nothing kept past eviction (one directory open per reopen, D2; the per-partition catalog, D1) outlives a
  write it should see.
- A reopen racing a close: a pin's reopen in one partition while the same key is being evicted or closed, and
  `when_all`'s error path (a failing pin waits for its siblings, and a cancelled budget token mid-batch releases
  every pin and the wide-search hold).
- The bound on memory: open indexes stay within soft_open across partitions (each partition's pass relieves an
  overage); per-index memory +1.3 KiB (3%); the wide-search hold bounds a batch's concurrent pins.

Earlier:

Suite 261118-063506-98ae6b at 0889ae829 failed to compile, introduced here: test_pool.cc:1757,1780-1781 read
`indexes.one_catalog(pool)`; fixed to `one_catalog(indexes.pool)` (Xapiand:84ce2e1bc). Its timing build and
A2 row were skipped behind the failed post-D2 profile (062628-76934e: `sample` found no pid because the gate's output
was block-buffered; profile.sh now runs `python3 -u`). Merged master (Xapiand:4f03a9fbd; master changed tests and
harness only, no src, so bins/base still is main's server). Branch pushed for CI.

A2 row 261118-131125-16ca52 (exclusive, both arms built in the job by `.scratch/build-arms.sh`, base at
f722ff939): pool 200 base p50 948/977 ms, lanes 884/888 ms (about 8% faster); pool 5000 base 125/153, lanes
131/143 ms. Lanes alone is 6.5x off pool 5000, so A2 is not met by D1 alone.

Profile 261118-145656-a26428 (strict, beside 6 light holds, so its absolute times are inflated; 7b9c3b wrote no
sample because profile.sh looked for an `xapiand` process and BIN is bins/lanes; it now samples the gate's server
child). Over 8.26 s of pool-200 searches with lanes:
- The wall is a serial chain: `run_open` holds 6,278 thread-ms in 8,262 ms, 0.76 opens in flight on 8 lanes.
  search_batch still awaits each pin before the next, so the lanes sit idle; D3 is what fills them.
- Each open's time: strata `basic_index::open` 57% (3,548), the Xapiand payload `open_blocking` 27% (1,717: WAL
  864, DataStorage 1,006 inclusive), the directory's PosixIO open 13% (813).
- By leaf, `open(2)`/`close(2)` are 3,934 thread-ms, over half the busy time. Their callers: strata `recover`'s
  per-file `open_existing(n)->size()` 1,707 (27% of run_open, the largest single item), the directory open 673,
  the WAL 574, DataStorage 411, load_files 263, scan_directory 211.

H3 (261118-150608-5ad670, exclusive): lanes + D3 pool 200 p50 291/284 ms against lanes 869/843; pool 5000 118/126
against 128/129. D3 is the fix: built at Xapiand:3b8d41c39 (`search_batch` pins its batch with `when_all`; node test
`test_a_search_batch_pins_its_indexes_together`, batches of 8 over 24 indexes with a missing one).

At 3b8d41c39: suite 261118-184756-bf6036 passed (45 of 45; A3, A4). CI green at 937a5e94e (CI 44447325291, FreeBSD
44447325350, arm64 44447325397). A2 before/after 261118-184757-7f0046 (exclusive, base f722ff939 vs head, both built
in the job): pool 200 base 959/881 ms, head 285/283 ms (3.2x faster); pool 5000 base 122/123, head 128/129 ms.
Head's pool 200 is 2.2x its pool 5000: A2 (within 2x) is not met.

H4 (261118-213949-011031, exclusive, ABAB x2, 10 searches): p16 is head with up to 16 partitions (12 at pool 200,
`.scratch/lanes16.patch`). p50 pool 200: head 283/281 ms, p16 264/262 ms (7% faster); pool 5000: head 125/126, p16
125/124. p16 is 2.1x its pool 5000, head 2.25x: H4 loses narrowly, so the cap is not committed.

Profile 261118-223530-9e3c57 failed (exit 4, empty graph): the 12 s sample outlasted the gate's 20 searches (about
8 s), the server exited under the first take and was gone for the second. A sampling problem, not the code; profile.sh
now takes RUNS, and the rerun samples 8 s inside 60 searches.

H5 (261119-033144-b85265, exclusive, ABAB x2, 10 searches; `.scratch/strata-h5.patch` replaces recover's
`open_existing(n)->size()` with `entry_footprint(n)`, one fstatat; built by `.scratch/build-h5.sh`). p50 pool 200 /
pool 5000, ms: head 281,282 / 124,123 (2.28x); h5 262,262 / 123,124 (2.12x); p16 266,268 / 128,125 (2.11x); p16+h5
251,249 / 122,124 (2.03x). No arm is within 2x; the cap and the fstatat change stay probes.

Profile 261119-033151-f32bdd (strict, 8 s inside 60 searches; beside 6 light holds, p50 692 ms): run_open 27,737
thread-ms, strata open 25,449, recover 24,110; recover's size lambda 21,634, of which `close(2)` 20,008 (72% of
run_open); open_blocking 2,330 (8%); the directory's PosixIO 791. H5 removes exactly those closes yet gains 7% when
exclusive, so the profile's close time is the light holds' file-system load, not the quiet cost.

Per-index memory at 2,500 open (pool 5000): head 41.1-41.9 KiB against base 40.0-40.3 in 184757-7f0046 (+1.3 KiB,
3%); H4/H5 arms 41.2-41.5. In docs/design/cheap-reopen.md.

## Next

1. Fill Evidence from the three jobs at 0dc08322a; A2 passes at <= 2.2x (D135); then State: ready for review.
2. Correctness review (questions under Doing).

## Blocked on

Nothing. A2 answered by D135 (owner 11-19: accept 2.2x; fstatat and the partition cap follow as small quests; 2x
moves to a strata quest). The request, for the record:

A2 decision (owner): five experiments done; the best arm is 2.0x, the shippable head 2.2x (283 vs 128 ms).
- Accept 2.2x: land D1-D3 (pool 200 3.4x faster, 959 to 283 ms; +1.3 KiB per open index) and amend A2. The
  `fstatat` (H5) and the 16-partition cap (H4) add 11% together (2.0x) and could follow as their own small quests.
- Build the one mechanism left: the catalog keeps a closed index's recovered log state (and file sizes) in its
  ghost entry, trusted only when the manifest log has the same inode and size, so a reopen skips `scan_directory`,
  the per-file sizes and `read_log` (about a third of a reopen's CPU at main). A strata quest, unmeasured; it would
  subsume H5. Projection, not measured: about 1.8x.
- Recommendation: accept 2.2x and land D1-D3 now (small and landed beats big and pending), and file the
  remembered log state as a strata quest that carries the 2x target, measured against this head.

## Lead's ruling (answered)

- 11-19: D3 is kept, but it went past the lead's deferral: handler.cc is wide-search-loop's file set. cheap-reopen
  lands after wide-search-loop; then merge master and resolve search_batch as the design note says (the budget
  checkpoint in the loop, the pins after it); handler.cc joins this quest's set at that point. Until then, do not
  touch handler.cc; ask the lead, with evidence, before going past any ruling.
- 11-19: H4 runs when the exclusive queue frees (about 05:17); if it loses, H5 (recover's sizes by `fstatat`) as a
  strata probe. A fresh contributor continues from this journal.

- test_pool.cc and test_node.cc granted: done, `one_catalog()` in test_pool.cc (its pools are one partition), the
  key's catalog or a sum over partitions in test_node.cc.
- handler.cc refused for now: D3 deferred to after wide-search-loop; A2 measured with lanes only.
- strata sizes/log state filed as a follow-up; WAL/DataStorage scan folding refused: design note updated.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/cheap-reopen.md` (breakdown from profile 261118-060438-5fb87b; H1 060440-baf657, H2 060951-f40471) |
| A2 | H5 261119-033144-b85265: best p16+h5 251/249 vs 122/124 ms, 2.0x, head 2.28x (not met); H4 261118-213949-011031: p16 pool 200 264/262 vs 125/124 ms, 2.1x (not met); before at main: 261118-060120-5b74f3 (pool 200 p50 1,079/979 ms, pool 5000 129/134 ms); after (lanes only): 261118-131125-16ca52, pool 200 884/888 vs pool 5000 131/143 ms; after (lanes + D3, 3b8d41c39): 261118-184757-7f0046, pool 200 285/283 vs 128/129 ms, 2.2x (not met) |
| A3 | suite 261118-184756-bf6036 at 3b8d41c39; earlier 261118-112223-56c3f7 (tests/test_cheap_reopen.cc: evicted, crashed, partitions) |
| A4 | suite 261118-184756-bf6036; CI green at 937a5e94e (44447325291, 44447325350, 44447325397) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- D2 maps ENOENT and ENOTDIR on the directory's open to index_not_created; any other open error (EACCES) now fails
  the pin as itself instead of reading as a missing index (fs::exists swallowed it).
- D1's partition count: the work pool's threads, at most 8 (H2 used 8), at most one per 16 of soft_open so small
  pools (every test pool) stay one partition; a descriptor overage is relieved by each partition's pass.
- The crash test forks before any thread starts (it runs first) and `_exit`s after a commit plus one uncommitted
  write; WAL off, so "last commit" is exact.
