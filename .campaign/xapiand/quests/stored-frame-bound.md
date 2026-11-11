# stored-frame-bound

## Outcome

A stored frame whose record count runs into the next frame's ordinals is corrupt. `decode_stored_frame`
rejects it, so the stored check, `records()` and merge agree with point reads again
(postings-fuzz-memory's security review, S1).

## Acceptance criteria

- A1 `decode_stored_frame` (include/strata/core/reader.hh) rejects a frame with `n > next_first - first`
  when a frame follows; a test in test/reader_check_test.cc crafts the overlapping segment of the review's
  `overlap.cc` probe (prospect `.scratch/review-pfm/`), and `check_segment` throws on it: the test passes at
  the head and a revert job shows it failing without the line: jobs
- A2 The suite: the `build-test` check

## Decisions

The bound is strict `>`: a frame may hold fewer records than its ordinal span only if the format allows
gaps; if the writer never leaves one, say so in the journal and use `!=`.

## May land early

None.
