# cheap-reopen: journal

State: working
Quest: .campaign/xapiand/quests/cheap-reopen.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on the suite (261118-062615-515090, A3/A4) and the post-D2 profile (build 261118-062623-696378, profile
261118-062628-76934e at bd6e5d50c: `fs::__status` under resolve should be gone).

## Next

1. On the lead's ruling below: D1 (P catalogs in DatabasePool) + D3 (concurrent batch pins), then an exclusive A2
   row (`.scratch/gate2.sh`, base vs head at pool 200 and 5000, ABAB) and per-index memory at 2,500 open.
2. Add the reopen-after-crash run under CRASH_POINTS if the reviewer wants strata's crash points, not only a fork.

## Blocked on

The lead: the mechanism (docs/design/cheap-reopen.md) needs files outside this quest's set.
1. D3: `src/database/handler.cc` (`search_batch` pins its batch with `task::when_all`; probe patch
   `.scratch/concurrent-pin.patch`). Alone it gains nothing (H1); with lanes it is the 3.0x (H2).
2. D1: `tests/test_pool.cc` and `tests/test_node.cc` call `pool.catalog()`; P catalogs change that accessor.
   pool-from-memory also holds `src/database/pool.*`: order the two (recommend pool-from-memory first, it is smaller).
3. D4: a strata quest (recover's sizes by one `fstatat`; the ghost keeps the closed log state), and a ruling on
   the Xapiand half (`src/database/shard.cc`, `wal.cc`, `data.cc`: one directory scan for the WAL and DataStorage,
   both replaced code; ~20% of a reopen).
Recommendation: grant 1 and 2 to this quest (D1+D3 are one change), file D4's strata half as its own quest after
this design, and take D4's Xapiand half only if A2 misses without it.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/cheap-reopen.md` (breakdown from profile 261118-060438-5fb87b; H1 060440-baf657, H2 060951-f40471) |
| A2 | before at main: 261118-060120-5b74f3 (pool 200 p50 1,079/979 ms, pool 5000 129/134 ms); after: pending D1+D3 |
| A3 | pending: suite 261118-062615-515090 (includes tests/test_cheap_reopen.cc) |
| A4 | pending: suite 261118-062615-515090 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- D2 maps ENOENT and ENOTDIR on the directory's open to index_not_created; any other open error (EACCES) now fails
  the pin as itself instead of reading as a missing index (fs::exists swallowed it).
- D1's partition count: the work pool's threads, capped at 8 until measured (H2 used 8).
- The crash test forks before any thread starts (it runs first) and `_exit`s after a commit plus one uncommitted
  write; WAL off, so "last commit" is exact.
