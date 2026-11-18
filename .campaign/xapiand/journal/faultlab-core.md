# faultlab-core: journal

State: working
Quest: .campaign/xapiand/quests/faultlab-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 2: include/faultlab/faultfs.h from strata:491a1bd core/faultfs.hh with F2-F7, tests for A1-A4.

## Next

1. faultfs.h + tests (A1 forced coins, A2 strata seed table, A3 sector probe vs journal MemoryIO and storage CrashIO, A4 link/lease/map_shared).
2. recorder.h (F10) + tests (A5 verify).
3. scenario.h, events.h (F11, F12).
4. Python half: history.py (monotonic, seal, set-aside), proxy.py, scenario.py; selftest, proxy_test (A5).
5. README, docs/ (A6).

Worktree: /Users/kronuz/code/Kronuz/faultlab-wt/xapiand/faultlab-core (faultlab repo, branch quest/xapiand/faultlab-core).
Step 1 done: faultlab:bfce6a8 (CMake, CI, test/check.h).

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
