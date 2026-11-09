# x-pool-followups: journal

State: waiting on jobs 261109-143205-f12ec7 261109-143234-23ccac 261109-143234-a09d65
Quest: .campaign/xapiand/quests/x-pool-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All four fixes committed at the branch head (confirmed on main by reading, not by a job): C3 `tests/test_pool.cc`
(sweep test: TTL 3 s, entries still all there at 1.5 s, gone within 7 s, under strata's 10 s default); C5
`tests/test_node.cc` (`names_the_abandoned_index`: one line with `abandon' (` and `, 1 pin, pinned)`); C6 (`pool.cc:690`
passed `use = true` on every pin; `DatabasePool::pin` takes `use`, `Shard::reopen` passes `!held`; the review's
`pool.cc:690` is the line, the fix needs no change to shard.h, which is outside the file set); A4
`harness/restore_stream_check.sh` waits until the count holds for 10 consecutive seconds (cap 300 s) before the kill -9.

## Next

1. When jobs end: read build-test (f12ec7), mutants (23ccac: x5 TTL ignored must be KILLED; "m-c6" re-pin counted must be KILLED), restore x20 (a09d65).
2. Fill the Evidence table, run `campaign verify x-pool-followups`, set ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261109-143205-f12ec7 (build-test), 261109-143234-23ccac (mutant) |
| A2 | job 261109-143205-f12ec7 |
| A3 | jobs 261109-143205-f12ec7, 261109-143234-23ccac (revert mutant) |
| A4 | job 261109-143234-a09d65 |

## Result

Written when the jobs end.

## Rulings I made

- C6: the re-pin is detected in `Shard::reopen` (`held` = a pin was in hand before the hand-back), and `ready()` is left alone so shard.h stays unchanged (outside the quest's file set). The re-pin inlines `ready()`'s two lines.
- A4: no restore-end signal exists without changing src (the failure path logs nothing and counts no metric), so the harness waits for 10 s of an unchanged positive count. That is a heuristic; a "restore over" log line in `search_views.cc` would be exact and is outside the file set.
- C3 test also asserts no erase happened at TTL/2 by comparing the failed-entry count before and after, not an absolute count (the pool may evict some of the 100 names).
- A4 job uses the plain build (`build/bin/xapiand`, asserts on) instead of the ASan build of the probe, to avoid a second full build; the race is the harness's timing, not a sanitizer matter.
