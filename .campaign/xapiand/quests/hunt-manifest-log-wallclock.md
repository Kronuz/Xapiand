# hunt-manifest-log-wallclock: manifest_log_test's replay bound counts work, not wall-clock time

Risk: low (a test). The lead writes this quest; contributors read it and never edit it.

## Outcome

strata `test/manifest_log_test.cc:1198-1201` checks four replay timings against wall-clock seconds
(`0.05 * time_scale`, `0.5 * time_scale`; `time_scale` is 10 under sanitizers, 1 otherwise, :44-46).
On a shared or slow runner (macos-arm64 CI, the loaded host) a correct build can fail them. The test
guards a real regression (8.9 s replaying 128 delete edits on ordinal vectors), so the guard stays,
in a form a machine's speed cannot break: count the work (words or ordinals each edit touches, if
the code exposes a counter or one can be added to the test's own harness without a library change),
or bound each timing by a ratio to a calibration run of the same shape in the same process.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, a job tagged `--for A1`
- A2 The regression is still caught: a `--fails-without` job at a commit that restores the 8.9 s
  replay (the per-edit ordinal-vector rewrite, or a test-local mutant reproducing its cost) fails
  the new check
- A3 [artifact] The journal names which form it chose and why the other does not fit
- A4 [ci] CI is green on every platform

## Decisions

- Lead, 2026-11-24: test-only. A library change (a counter in the manifest log) needs its own quest.

## May land early

Nothing: one change.
