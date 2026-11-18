# cheap-reopen: journal

State: waiting on jobs 261118-131125-16ca52 261118-131131-7b9c3b
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

Suite 261118-112223-56c3f7 (A3, A4) passed at 4f03a9fbd. A2 row 261118-112224-c75c29 failed on the rig: it ran in a
fresh checkout with no bins/base. `.scratch/build-arms.sh` now builds both arms in the job (base from
`--ref base=f722ff939`, master's merge-base; lanes at head), writes bins/arms.sha, creates wide2500 if missing, and
gate2.sh checks the sums first. Requeued in slot xapiand-cheap-reopen: A2 row 261118-131125-16ca52 (exclusive, pool
200 and 5000, ABAB x2); profile 261118-131131-7b9c3b (strict, after the row).

## Next

1. Read the A2 row: lanes alone (no D3). If lanes pool 200 is over 2x of lanes pool 5000, record the number; the
   quest then waits for wide-search-loop and D3 (hunt.md).
2. Read profile 261118-131131-7b9c3b (sample of lanes at pool 200).
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
| A2 | before at main: 261118-060120-5b74f3 (pool 200 p50 1,079/979 ms, pool 5000 129/134 ms); after (lanes only): 261118-131125-16ca52 queued |
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
