# cheap-reopen: journal

State: waiting on jobs 261118-213949-011031 261118-223530-9e3c57
Quest: .campaign/xapiand/quests/cheap-reopen.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

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

H4 (261118-213949-011031, exclusive, queued behind light holds until about 05:17): head against head with up to 16
partitions (`.scratch/lanes16.patch`: 12 at pool 200, P = min(threads, 16, soft_open / 16)). Profile of head
261118-223530-9e3c57 (strict) reruns 184759-013c96, whose `sample` wrote an empty call graph; profile.sh now takes it
again once and fails on an empty one. If H4 loses, H5 is recover's sizes by `fstatat` (strata probe).

## Next

1. Read H4 261118-213949-011031: p16 pool 200 within 2x of its pool 5000? If it wins, land the partition cap and
   rerun the A2 before/after at that commit; then ready for review (high risk). Budget: H1-H4 used of about five.
2. Read profile 261118-223530-9e3c57; H5 (recover sizes by fstatat, a strata probe) if H4 loses.
3. Per-index memory at 2,500 open from the row's pool-5000 arms, lanes vs main.

## Blocked on

Nothing for D1. D3 waits for wide-search-loop (lead, msg answered below).

## Lead's ruling (answered)

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
| A2 | before at main: 261118-060120-5b74f3 (pool 200 p50 1,079/979 ms, pool 5000 129/134 ms); after (lanes only): 261118-131125-16ca52, pool 200 884/888 vs pool 5000 131/143 ms; after (lanes + D3, 3b8d41c39): 261118-184757-7f0046, pool 200 285/283 vs 128/129 ms, 2.2x (not met) |
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
