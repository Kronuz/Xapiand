# fuzz-cpu-hour: One real CPU-hour of fuzzing per prospect decoder the release reads

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status fuzz-cpu-hour`.

## Outcome

release-rows' fuzzing ran six independent 10-minute wall-clock starts per decoder at 3 to 32
inputs/s, discarding each corpus, with coverage still rising (review F2). Each decoder the release
reads (segment, postings, values, fielddict) gets one CPU-hour (measured CPU time) with a corpus
kept across runs, after its per-input cost is understood and cut where it is the harness.

## Acceptance criteria

- A1 [artifact] Each decoder's per-input cost before and after, with its cause (segment build per input?): `docs/perf/fuzz-targets.md`
- A2 One CPU-hour per decoder, CPU time measured, corpus kept across runs, zero findings: jobs
- A3 fuzz_segment's input length limit grows past its seeds (no fixed 26-byte cap): the A2 jobs

## Contracts

None.

## Decisions

- Lead, 11-11: split from release-rows A5 (review F2); hunt row on slow fuzz targets.

## May land early

Everything.
