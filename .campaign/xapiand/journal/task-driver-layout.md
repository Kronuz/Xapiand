# task-driver-layout: journal

State: waiting on jobs 261119-125658-e4aa1e 261119-125701-a28181 261119-125704-1e0770 261119-125708-077ad8
Quest: .campaign/xapiand/quests/task-driver-layout.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix and test committed on the quest branch (task worktree): a4a9e42 test, cd6a54f fix, b7b76b5 revert (mutant), 63b43bd reapply (head).

## Next

1. When jobs pass: confirm A1-A3 rows, set State ready for review (A3 also needs CI green after a push, not done: no push).

## Blocked on

The quest file set (task.hh, test/**) omits CMakeLists.txt, which the new test registration (3 lines) needs. Lead: add it to the file set. Otherwise nothing.
<"

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | e4aa1e (fails-without at b7b76b5 revert), a28181 (passes at 63b43bd) |
| A2 | 1e0770 deadline_test two-drivers (Debug abort) |
| A3 | 077ad8 suite; CI after push |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Test is sizeof/alignof across two TUs (private members block offsetof): test/layout_test.cc, layout_release.cc (NDEBUG), layout_debug.cc; CMake target task_layout_test.
