# cheap-reopen: journal

State: waiting on jobs 261118-150608-5ad670
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

Experiment H3 (job 261118-150608-5ad670, exclusive): lanes vs lanes + D3 as a probe (`.scratch/concurrent-pin.patch`
over head, built by `.scratch/build-d3.sh`, never landed while the lead defers D3), pool 200 and 5000, ABAB x2.
It targets the serial chain. If it wins and the lanes fill, the CPU half follows (H4): recover's sizes from
`io_->inspect_entry(n)` (one `fstatat` on the directory descriptor, already in completion's PosixIO) instead of
an open, fstat and close per file, about a quarter of each open.

## Next

1. Read H3 261118-150608-5ad670: lanes + D3 at pool 200 against lanes at pool 5000 (target within 2x). Experiment
   budget: H1, H2, H3 used of about five; stop at the third loser in a row.
2. H4 (recover sizes by fstatat, a strata probe patch over .deps-perf/strata-src) if H3 leaves CPU the limit.
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
| A2 | before at main: 261118-060120-5b74f3 (pool 200 p50 1,079/979 ms, pool 5000 129/134 ms); after (lanes only): 261118-131125-16ca52, pool 200 884/888 ms vs pool 5000 131/143 ms (not met) |
| A3 | pending: suite 261118-112223-56c3f7 (tests/test_cheap_reopen.cc: evicted, crashed, partitions) |
| A4 | pending: suite 261118-112223-56c3f7; CI on the pushed branch |

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
