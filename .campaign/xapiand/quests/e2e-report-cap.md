# e2e-report-cap: The docs E2E's newman report stays parseable

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status e2e-report-cap`.

## Outcome

The docs E2E's newman report no longer carries whole response bodies, so it stays small and
`harness/e2e_assert.py` parses it every run (job 261110-152519-7a2db9 failed on a 62.9 MB report
truncated mid-JSON).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The E2E passes with a report under 5 MB: an `e2e` job at the head, its log naming the size.
- A2 The assertions still read what they check (status codes, the fields they compare): a
  deliberately broken expectation still fails the assert (a job or a test).

## Contracts

None.

## Decisions

Lead: cap or drop bodies in the report, whichever keeps the asserts' inputs.

## May land early

Everything.
