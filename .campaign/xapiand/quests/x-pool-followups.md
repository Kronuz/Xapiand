# x-pool-followups: x-pool review follow-ups and the restore check's race

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x-pool-followups`.

## Outcome

The x-pool review's non-blocking gaps C3, C5 and C6 are closed and `harness/restore_stream_check.sh` waits for the restore it checks instead of racing it (hunt list rows; `reviews/x-pool-correctness.md`).

## Acceptance criteria

- A1 C3: the sweep test also checks the negative entries are still present before their TTL ends, so an erase that ignores the TTL fails it (a mutant that ignores the TTL is killed): the `build-test` check and a mutant job
- A2 C5: both node-stop tests match the abandoned index's line (its key and `1 pin`), not "pinned" anywhere: the `build-test` check
- A3 C6: a re-pin after a reopen inside one request is not counted as a use (CT-R31), with a test that fails without the fix: the `build-test` check and a revert job
- A4 `harness/restore_stream_check.sh` waits for the restore to finish before it checks; repeated 20 times it passes every time (`.scratch/x-pool-probes/repeat_restore.sh` is the starting probe): a job

## Contracts

None.

## Decisions

- Non-blocking findings from x-pool (landed Xapiand:1c2b3d32b); C4 and C7 need no change (the review says so).

## May land early

None.
