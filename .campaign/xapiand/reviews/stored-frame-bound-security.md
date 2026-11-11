# stored-frame-bound review: security

Commit: strata:94480d52
Verdict: APPROVE
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- build-test 261110-222851-fda023 at 94480d52: 74 of 74 tests pass. They include reader_check
  (its empty-records case, 6,000 documents with no stored bytes), format14's fixtures and the fuzz
  replays.
- The S1 probe from the postings-fuzz-memory security review, run on prospect with strata at
  94480d52 (job 261110-222853-29eb1c). In both cases `check_segment` throws "stored frame
  count", and 0 of 40 ordinals read differently through the cache and through `stored(o)`.
- Revert job 261110-222857-88df36 at strata:7e021e45. That commit is 94480d52 with only the
  reader.hh lines reverted. The log shows reader_check_test.cc:180 `CHECK(threw)` failing twice:
  "check_segment ACCEPTS the overlap" for both cases.

## What I checked myself

`!=` against every writer:

- `encode_stored` (writer.hh:331-353) covers `stored_ends_` from 0 to the end in runs:
  `first = last + 1`.
- `gather`/`close_block` advance `first_ordinal_ += pending_count_` (writer.hh:356-371).
- merge calls `b.add(rec)` once for every entry of `m.order` (merge.hh:213-218).
- `add` and `add_stored_view` call the stored builder for every document whenever the stored bit
  is set (writer.hh:528-541, 623-631). A document with no stored fields adds an empty record,
  which still counts toward its frame.

So frames are contiguous in every writer. Also, a frame holding fewer records than its span was
already corrupt: the first ordinal in the gap fails `holds()` and throws "stored frame count".
`!=` only rejects such a frame earlier, at decode, and cannot reject a segment whose reads all
succeeded before.

Test fidelity: `check_overlap` ports the probe's segment (40 raw records, 256-byte blocks, frame 0
extended over frame 1, frame 1 left valid or given a bad codec) and asserts that the check throws.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B1 | no | nit | The test asserts only that `check_segment` refuses the segment. It does not compare the cached read with `stored(o)`, which the probe also checks. The bound sits in `decode_stored_frame`, so both read paths share it. | reader_check_test.cc:180 | Optional: also compare the in-order cached read with `stored(o)` over every ordinal. |

## Not run

- No new fuzzing; fuzz_stored's replay ran in build-test.
- Other platforms are left to CI.
