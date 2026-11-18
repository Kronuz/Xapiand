# cheap-reopen: journal

State: waiting on jobs 261118-063506-98ae6b 261118-063508-79daaf 261118-063511-bec7e0
Quest: .campaign/xapiand/quests/cheap-reopen.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

D1 built (Xapiand:0889ae829): one catalog per pool partition. Jobs: suite 261118-063506-98ae6b (A3, A4; supersedes
062615-515090 at bd6e5d50c), timing build 261118-063508-79daaf (bins/lanes in the slot), A2 row 261118-063511-bec7e0
(exclusive, `.scratch/gate2.sh`, main bins/base vs lanes at pool 200 and 5000, ABAB x2). The post-D2 profile
261118-062628-76934e still runs at bd6e5d50c.

## Next

1. Read the A2 row: lanes alone (no D3). If pool 200 is over 2x of pool 5000, report the number; the quest then
   waits for wide-search-loop and D3 (hunt.md).
2. Per-index memory at 2,500 open from the row's pool-5000 arms (the gate's "KiB per open index"), lanes vs main.

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
| A2 | before at main: 261118-060120-5b74f3 (pool 200 p50 1,079/979 ms, pool 5000 129/134 ms); after (lanes only): 261118-063511-bec7e0 pending |
| A3 | pending: suite 261118-063506-98ae6b (tests/test_cheap_reopen.cc: evicted, crashed, partitions) |
| A4 | pending: suite 261118-063506-98ae6b |

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
