# open-mix: Measure the share of index opens that never write while open

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status open-mix`.

## Outcome

We know, with numbers, what share of Xapiand's index opens see no write while open (D60b), which decides read-only-open's shape. The index registry (src/database/pool.cc) counts, per open, whether any write (index, update, delete, commit, RESTORE object) reached that index before it closed, and exposes the totals (a log line at close or the metrics the pool already reports). The counts are taken under the loads we have: x7-gate's T4/T5 mix (harness/gate7c_rows.sh), a RESTORE, the docs E2E suite, and a many-indexes pass that opens far more indexes than the registry keeps open (so idle closes and reopens happen), each reported separately.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The counters are exact: a test opens indexes with and without writes, closes them, and checks both totals: the build-test check, with a `--fails-without` job (the write mark removed)
- A2 [artifact] Per load: opens, opens with no write, the share, and how many of those opened under a concurrent fsync writer (where the ~23 ms flush applies), with job ids and what each load stands for; and a one-line recommendation against D60b's rule: `docs/perf/open-mix.md`

## Contracts

None. The counters are diagnostics; no API or format change.

## Decisions

- Owner, 11-12 (D60b): measure before choosing read-only-open's shape.
- Lead: loads that exist today only; the many-indexes harness (hunt row, phase3-ratchets A3) is not built here, a pass over many small indexes with the existing tools is enough.

## May land early

The counters.
