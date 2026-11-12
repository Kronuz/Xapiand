# drop-dir-reader: completion drops the unused shared directory lock

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status drop-dir-reader`.

## Outcome

completion's `IO::acquire_directory_reader()` (completion:504f409) is gone with its PosixIO path, tests, mutant m15 and docs: D60c removed its only planned user (strata) and Detent does not use it. The directory owner lock behaves as before 504f409; the one `lock(fd, ...)` helper stays only if it is still the simplest shape for the exclusive file and directory locks, otherwise it folds back.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 No `acquire_directory_reader` or shared directory lock remains in code, tests, tools/mutants.py (count back to 24), the workflow or docs, and the suite passes (completion_mutants_anchors included): the build-test check
- A2 CI green on every platform: the build-test job and CI

## Contracts

Removes completion:directory-lock's shared half. No user anywhere (Xapiand, strata, prospect, Detent checked).

## Decisions

- Owner, 11-12 (D60c); Detent lead 11-12: remove it.

## May land early

None.
