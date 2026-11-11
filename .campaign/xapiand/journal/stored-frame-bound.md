# stored-frame-bound: journal

State: waiting on jobs 261110-222851-fda023 261110-222853-29eb1c 261110-222857-88df36
Quest: .campaign/xapiand/quests/stored-frame-bound.md (the lead's; never edit it)

## Doing

Fix committed (strata:e5979526); test compile fix on top (strata:94480d52, a segment viewing a temporary). Only jobs remain.

## Next

1. When the jobs pass, fill the Evidence table and set State to ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261110-222851-fda023 (runs reader_check_test); the review probe overlap.sh at the head 261110-222853-29eb1c, 0 of 40 reads differing |
| A2 | build-test 261110-222851-fda023 |

## Fails without the fix

- Revert job 261110-222857-88df36 on refs/scratch/sfb-revert: CHECK(threw) fails at reader_check_test.cc:180 ("ACCEPTS the overlap") in both cases.

- 261110-222857-88df36: scratch commit (refs/scratch/sfb-revert in the strata repo: head with reader.hh as before the fix) builds reader_check_test and requires it to fail; the job exits 0 when it fails.
- 261110-222619-9b3428: earlier revert job, failed to compile (test bug, fixed in strata:94480d52); not evidence. 261110-222541-cd9abe: build-test at the uncompilable test, cancelled.
- 261110-222609-76bc4e: first revert job, cancelled (`! (build && run)` would also pass on a build failure); replaced by the job above.

## Result

`decode_stored_frame` (include/strata/core/reader.hh) now rejects a frame whose record count n differs from `next_first - first` when a frame follows. test/reader_check_test.cc rebuilds the review probe's overlapping segment (frame 0 rewritten to claim frame 1's ordinals, frame 1 valid or with a bad codec byte, all checksums resealed) and requires `check_segment` to throw on both.

The probe job is a copy of the reviewer's overlap.sh/overlap.cc in ~/.scratch/sfb/probe with the strata source pointed at the job's `--ref strata` checkout (the head) instead of the pinned fetch; the job runs at prospect HEAD.

## Rulings I made

- Strict `!=`, not `>`: the writer never leaves a gap in a frame's ordinal span. Both paths (`encode_stored`, `gather`/`close_block` in writer.hh `put_frame`) number frames contiguously: each frame's first is the previous first plus its record count, and every document has a record (possibly empty). The last frame is already bounded by `first > doc_count_ - n`.
