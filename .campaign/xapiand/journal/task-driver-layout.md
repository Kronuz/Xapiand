# task-driver-layout: journal

State: ready for review
Quest: .campaign/xapiand/quests/task-driver-layout.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix and test committed on the quest branch (task worktree): a4a9e42 test, cd6a54f fix, b7b76b5 revert (mutant), 63b43bd reapply (head).

## Next

1. Lead: confirm CI green, review, land.

## Blocked on

The quest file set (task.hh, test/**) omits CMakeLists.txt, which the new test registration (3 lines) needs. Resolved: the lead added it.
<"

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 361954 mutant at b7b76b5 exits 1 on the assertion ("layout: release 216/8, debug 224/8: FAILED"); b00b7b at 63b43bd "release 224/8, debug 224/8: ok" (CMake build) |
| A2 | 48d7b2 deadline_test two-drivers: the assert aborts, "two-drivers ok" |
| A3 | 261119-125708-077ad8 (suite); CI on the branch |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Test is sizeof/alignof across two TUs (private members block offsetof): test/layout_test.cc, layout_release.cc (NDEBUG), layout_debug.cc; CMake target task_layout_test.

- The first direct-compiler A1/A2 jobs lacked the wheel include path; superseded by the CMake-build jobs 361954, b00b7b, 48d7b2.
