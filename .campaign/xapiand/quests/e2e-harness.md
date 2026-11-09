# e2e-harness: The E2E harness fails when it proves nothing

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status e2e-harness`.

## Outcome

A docs E2E run is GREEN only when its own capture ran and every assertion passed. `run_all.sh e2e`
grades a fresh report it just wrote and fails when the capture fails; `engine_e2e.sh` exits with the
capture's and the assertions' status; and with `RUN_ALL_STRICT=1` (what gate checks set) a skipped
layer is a failure, not a pass.

## Acceptance criteria

- A1 A failed capture fails `run_all.sh e2e`, even with an old report left from an earlier run: the `probe` check (a binary that exits 1)
- A2 `engine_e2e.sh` exits non-zero when the capture or the assertions fail: the `probe` check
- A3 Under `RUN_ALL_STRICT=1` a skipped layer fails the run (`tls` without newman 6): the `probe` check
- A4 [manual] The `probe` check fails at master (revert check): the lead names the job of the same command at master's commit and its exit

## Contracts

None: harness scripts. `x-pool`, `x7-gate` and every quest whose `e2e` check calls `run_all.sh` gain
real E2E evidence once this lands and their checks set `RUN_ALL_STRICT=1` with newman 6 first on PATH.

## Decisions

- Found by the x-pool contributor (journal at Xapiand:90519facc): the jobs had newman 4.5.0, the
  capture wrote nothing, and `run_all.sh` graded `/tmp/xa_e2e_ours.json` of 2026-11-04 as GREEN; the
  `asan` check's docs E2E (`engine_e2e.sh`) skipped without failing. The lead confirmed both.
- newman 6.2.3 lives at `~/.scratch/newman6` (as the earlier green runs used); the checks put it first
  on PATH. A pinned install beside the harness is a follow-up.

## May land early

None: it is one small change.
