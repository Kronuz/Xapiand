# coverage-database-t13: Database coverage reaches T13's target

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status coverage-database-t13`.

## Outcome

src/database reaches T13's coverage target (90% lines, 80% branches) suite-wide. coverage-database left it at 64.2% / 53.8% (job 261119-230342-b6192c); the largest gaps were handler.cc 28.3%, wal.cc 32.9%, name_index.cc 17.8% and schema.cc 49.5% from that test alone. Split into tasks per file when started; each new test checks values and pins defects with a hunt row.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 Line and branch coverage of database at or above T13's target: harness/coverage.sh before and after
- A2 Every new test checks values, not only that code ran; a test of a fixed bug fails without the fix
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
