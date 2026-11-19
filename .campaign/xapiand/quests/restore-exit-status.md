# restore-exit-status: xapiand --restore exits nonzero when the restore fails

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status restore-exit-status`.

## Outcome

`xapiand --restore FILE` exits nonzero when the restore fails (a file that is not a dump now logs "Indexing object has an unsupported type: POSITIVE_INTEGER" and exits 0), so scripts can tell success from failure. Pinned by coverage-core-main job 261119-044115-72c517.

## Acceptance criteria

- A1 A restore of a non-dump file exits with a nonzero status (EX_DATAERR or the code main.cc uses for bad input) and names the problem; a test fails without the fix
- A2 A good restore still exits 0 (test)
- A3 The suite passes and CI is green

## Contracts

None beyond the fixed behavior.

## Decisions

- From hunt.md rows; filed by the lead 2026-11-19.

## May land early

None.
