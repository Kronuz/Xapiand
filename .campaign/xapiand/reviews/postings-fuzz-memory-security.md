# postings-fuzz-memory review: security

Commit: prospect:35a9886 (strata pin strata:71750940)
Verdict: CHANGES
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- A3, job 261110-192311-518a9b at 35a9886: I read its log. 146,038 runs, peak RSS 420 MB,
  374-408 MiB from 600 s to 3,600 s, 0 artifacts. The RSS fix holds.
- The strata revert test (reader_check_test, job 261110-182037-74c93c): it shows the cache
  removes the per-document allocations. It does not cover frame boundaries.

## What I checked myself

Probe scripts are in `prospect/.scratch/review-pfm/` (`overlap.sh` + `overlap.cc`,
`inject.cmake`, `targets.cmake`; `coverage.sh`).

- S1, job 261110-221634-8d503f (exit 1 = finding). It writes a raw-codec segment of 40 records in
  8 frames, then rewrites frame 0 so its record count also covers frame 1's ordinals [5, 10) as
  empty records, and reseals the checksums. Case A leaves frame 1 valid; case B gives frame 1 a
  bad codec byte. In both cases `check_segment` passes. The cached read returns "" for ordinals
  5-9. The point read `stored(o)` returns frame 1's records (A) or throws corrupt_segment (B).
- S3, job 261110-212207-102e23: source coverage of `fuzz_postings_replay` over the 841-input
  corpus.
  - 626 inputs reach the flips.
  - 175 inputs reach `check_segment`, and 164 run the stored check.
  - Each stored check decodes 1 frame (164 decodes for 583k records; 162 zstd, 2 raw).
  - The check still validates every frame it reaches, so the cache removes only repeated decodes.
  - The flips touch only term a's postings. fuzz_postings never feeds corrupt stored bytes, before
    or after this change. strata's `fuzz_stored` is the corrupt-input fuzzer for the stored
    decoder, and it has no oracle comparing cached reads with point reads.
- Read for risk 1:
  - `decode_stored_frame` (strata reader.hh:457-521) grows its buffer only after these checks:
    `raw_size <= max_stored_frame` (about 64 MiB), the codec's ratio (raw exact, lz4 255x,
    zstd 32,768x the payload) and zstd's recorded content size. The offset table is
    4 x (n + 1), where n is at most the remaining payload bytes and at most doc_count.
  - The check keeps one cache, so memory stays at one frame.
  - prospect's `positions_reader` caps `count` at `max_positions` and at
    129 x the term's range (reader.hh:1501-1533). The postings groups and regions are checked
    against the range (reader.hh:720-728).
  - I found no allocation sized from an unbounded input length.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | yes | defect | Risk 2: `decode_stored_frame` does not check that a frame's record count stays below the next frame's first ordinal (reader.hh:470 checks only that firsts increase). With the check now reading through the cache (reader.hh:570-571), a frame that overlaps the next one serves its own records for the next frame's ordinals, so the next frame is never decoded. `check_segment` passes a segment whose point reads return other bytes or throw. Before this change, the check located each ordinal's frame on its own and would decode frame 1 and throw (the point-read column in case B). `records()` (digest) and merge (merge.hh:216) already read through such a cache, so they had this divergence before; the check was the one path that caught it. | job 261110-221634-8d503f: "check_segment PASSED", 5 of 40 ordinals differ in A and in B | In `decode_stored_frame`, when `f + 1 < frames`, reject `n > next_first - first` (and possibly require `first + n == next_first`). One line fixes the check, `records()` and merge together. Rerun `overlap.sh`: it exits 0 only when check and point reads agree. A check-only fix leaves the read divergence and still fails the probe. Add the probe's case to strata's reader_check_test or fuzz_stored. |
| S2 | no | gap | Risk 3: fuzz_postings never feeds corrupt stored bytes (its flips stay inside term a's postings), so it fuzzes the stored check only on writer-made frames of empty records. The cache hides no coverage it had. The stored decoder's corrupt-input coverage depends entirely on strata's fuzz_stored. | job 261110-212207-102e23 | Optional: give fuzz_stored an oracle comparing the in-order cached read with `stored(o)`. It would have found S1. |
| S3 | no | nit | Risk 1: decode work in the check is bounded per frame. A crafted zstd frame can claim 32,768x its payload, up to about 64 MiB, and each frame decodes once. Total check CPU is at most about 32,768x the section's bytes, and memory is one frame. This is the format's existing bound (D1d); the change reduces the work from per document to per frame. | strata reader.hh:466, 483 | None for this quest. |

## Not run

- No probe at the old pin (strata 9d4630c). Before the change, the check read each ordinal with
  the cacheless `stored(o)`, which is S1's point-read column.
- No new fuzzing beyond the A3 log. No ASan run of the probes; they are deterministic and stay in
  bounds.
- Linux and other platforms are left to CI.
- I did not audit the manifest-log commits the pin also brings (9d4630c..15d39c30); their own
  reviews cover them.
