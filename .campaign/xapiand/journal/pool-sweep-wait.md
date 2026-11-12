# pool-sweep-wait: journal

State: waiting on jobs 261112-012420-23a451 261112-012422-bf1ed3
Quest: .campaign/xapiand/quests/pool-sweep-wait.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix committed (610b4133c) and pushed; jobs: A1 x50 load run 261112-012420-23a451, build-test 261112-012422-bf1ed3.

## Next

1. When both pass, record the evidence rows and set ready for review.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | jobs 261112-012420-23a451 (x50 beside 2x-CPU load), 261112-012422-bf1ed3 (build-test) |
| A2 | `tests/test_pool.cc`: the wait is `index.wait_merges()`, the index's own merge completion (strata index.hh), under the entry's turn |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Linux CI failed REQUIRE(merge_output_unsynced) at 2201: the merge can finish before shard.commit(), whose publish_merges + commit_files then publishes and syncs it, so wait_merges() has nothing left to publish. Fix: the test's GatedExecutor holds the merge until after the commit (as test_the_autocommit_comes_back does), then gate.let_go() and wait_merges(): the order is deterministic, no time involved.

- No src/ change needed: strata's `wait_merges()` (public, at the pinned commit) is the completion signal; assertions kept (merges_pending()==0, merge_output_unsynced()).
- The later `until(... free, 10s)` poll on the sweep's close stays: the sweep has no completion signal and the quest names the merge loop only.
