# hunt-faultlab-at-mark: journal

State: ready for review; waiting on jobs 261121-233632-7ed5f1 261121-233633-0df7d0
Quest: .campaign/xapiand/quests/hunt-faultlab-at-mark.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Three cases in faultlab test/faultfs_test.cc: faultlab:50b5394 (at_mark on an unstaged file; two
ordering points) and faultlab:7f3910c (a stale at_mark an after-effect sync fault must drop).
Mutant script: .scratch/xapiand/hunt-faultlab-at-mark/mutant.sh in the faultlab worktree
(ignored): deletes `node_->at_mark.clear();` at include/faultlab/faultfs/file.h:157, builds,
passes only when the test exits 1 and the log shows `FAIL: a sync failing after its effect drops
the at_mark ...`.

## Next

1. Read 261121-233632-7ed5f1 (A1) and 261121-233633-0df7d0 (A2); both green: review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-233632-7ed5f1 (build-test at faultlab:7f3910c); earlier 261121-202430-846c9f at faultlab:50b5394 |
| A2 | 261121-233633-0df7d0 (mutant of file.h:157 at faultlab:7f3910c; the new case must FAIL) |

Examined: 261121-202427-8ee313 failed: the same mutant at faultlab:50b5394 survived (GREEN, 0
failures). Neither first case reaches a state where line 157 matters; see Rulings.

## Result

faultlab's suite covers faultfs's at_mark sync path: an after-effect sync fault on a file its
point did not stage, across two points, and the one state where the fault's own `at_mark.clear()`
is observable. Tests only; no faultfs change. Leaves open the stale-at_mark finding under Rulings.

## Rulings I made

- Why the first mutant survived: in the after-effect branch, `settle_staged()` runs right after
  line 157 and, whenever `marks_` is non-empty, `clear_marks()` empties every inode's at_mark. So
  line 157 matters only when `marks_` is empty and the file still holds an at_mark. `note_write`
  records at_mark only while a mark exists, so that needs a mark removed without `clear_marks`:
  `order_files_and_directory` pops its mark when its directory step fails before its effect and
  no faultfs file staged, and it releases the lock between the push and the pop. A write in that
  window (a `hook` file's sync) leaves at_mark (epoch 0, "old") with no mark. The new case then
  syncs under an `after` fault ("mid" durable), opens a new point (epoch 0 again), writes "NEW"
  (note_write sees epoch 0 already recorded) and crashes with epochs none, size current, sectors
  fresh: correct faultfs keeps "mid"; the mutant restores "old". Kept the same mutant, not an
  easier one.
- Finding for the lead (faultfs, outside this file set): that popped mark leaves a stale at_mark
  until the next sync or settled point. Unsynced it is benign (the stale bytes are the durable
  ones, an allowed crash outcome), but it is the only path where at_mark outlives its mark.
