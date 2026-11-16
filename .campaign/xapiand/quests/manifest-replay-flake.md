# manifest-replay-flake: manifest_log's replay-cost check holds on shared CI runners

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status manifest-replay-flake`.

## Outcome

`replay_cost` in `test/manifest_log_test.cc` keeps catching the per-edit scans it guards against
(each made the ratio about 100x) without failing on a loaded or shared host. It failed on macOS CI
runners at main strata:71750940 (run 42951960800, macos-arm64) and at strata-races 2a4d2aa0 (run
43043120858, macos-x86_64): `segments_ratio < 20`, wall-clock time of 2000 edits on 100k against
1k segments.

## Acceptance criteria

- A1 The check measures what cannot vary with the host (work counted: comparisons, visits or
  allocations), or a timing robust to it (median of interleaved trials) with a bound still far
  below 100x: the reasoning in a one-line comment at the check.
- A2 The check still fails with the per-edit identity scan reintroduced (a mutant), tagged
  `--fails-without`.
- A3 The check passes in 20 runs of `manifest_log` under the load of a parallel suite (a light job
  running both), and CI is green.

## Contracts

None.

## Decisions

Lead: low risk, test only; lands on CI green and the lead's read of the diff.

## May land early

None.
