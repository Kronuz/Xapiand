# x-pool-followups: journal

State: ready for review
Quest: .campaign/xapiand/quests/x-pool-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All four fixes committed and verified at the branch head (confirmed on main by reading, not by a job): C3 `tests/test_pool.cc`
(sweep test: TTL 3 s, entries still all there at 1.5 s, gone within 7 s, under strata's 10 s default); C5
`tests/test_node.cc` (`names_the_abandoned_index`: one line with `abandon' (` and `, 1 pin, pinned)`); C6 (`pool.cc:690`
passed `use = true` on every pin; `DatabasePool::pin` takes `use`, `Shard::reopen` passes `!held`; the review's
`pool.cc:690` is the line, the fix needs no change to shard.h, which is outside the file set); A4
`harness/restore_stream_check.sh` waits until the count holds for 10 consecutive seconds (cap 300 s) before the kill -9.

## Next

1. Review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261109-154800-ae210c (build-test), 261109-162043-d6456d (mutant) |
| A2 | job 261109-154800-ae210c |
| A3 | jobs 261109-154800-ae210c, 261109-162043-d6456d (revert mutant) |
| A4 | job 261109-163822-60fbd2 |

## Result

C3, C5, C6 and the restore check are fixed; the head's code equals 72203782 (the later commit is the journal). build-test
261109-154800-ae210c passes at 72203782. Mutants (261109-162043-d6456d): x5 (negative TTL ignored) killed at
test_pool.cc:2298 and the C6 mutant (re-pin counted as a use) killed at test_pool.cc:1702, 2 of 2. The restore check
passed 20 of 20 (261109-163822-60fbd2, 12,679 documents kept across kill -9 in the last run). Open: the restore-end wait is a
10 s still-count heuristic; an exact signal needs a log line in search_views.cc (outside the file set).

## Rulings I made

- C6: the re-pin is detected in `Shard::reopen` (`held` = a pin was in hand before the hand-back), and `ready()` is left alone so shard.h stays unchanged (outside the quest's file set). The re-pin inlines `ready()`'s two lines.
- A4: no restore-end signal exists without changing src (the failure path logs nothing and counts no metric), so the harness waits for 10 s of an unchanged positive count. That is a heuristic; a "restore over" log line in `search_views.cc` would be exact and is outside the file set.
- C3 test also asserts no erase happened at TTL/2 by comparing the failed-entry count before and after, not an absolute count (the pool may evict some of the 100 names).
- A4 job uses the plain build (`build/bin/xapiand`, asserts on) instead of the ASan build of the probe, to avoid a second full build; the race is the harness's timing, not a sanitizer matter.

Lead, 11-09: the C6 re-pin goes through `Shard::ready(bool use)` (Xapiand:ad641a000) instead of an inlined copy; `shard.h` joined the file set. Evidence rows cite the jobs at that head; the earlier jobs at 72203782 (f12ec7, 23ccac, a09d65) passed too.

Lead, 11-09: job 261109-154759-439c43 skipped the C6 mutant (its string was no longer unique after the ready() refactor); the retargeted 261109-162043-d6456d killed 2 of 2 and replaces it.
