# keyword-open-range: Lower-bound-only keyword and text ranges match

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status keyword-open-range`.

## Outcome

A keyword or text range with only a lower bound (`{"_from": "b"}`) matches every value at or above
it. Before, the missing end serialized as "" and sorted below the start, so `getStringQuery`
(src/multivalue/range.cc) returned the empty query. Numeric, date, time and timedelta ranges
already compared only the ends given. Hunt-list row M2, from the coverage-multivalue job
261119-035511-e1d.

## Acceptance criteria

- A1 A lower-bound-only keyword range and text range return the values at or above the bound:
  test_coverage_multivalue_range, failing at the test commit alone (job 261121-042222-ac72f6 at
  Xapiand:0703f2b27, exit 1) and passing at the fix (`--fails-without`)
- A2 The suite passes at the landed code: a job

## Contracts

None: query results change only for a range that returned nothing.

## Decisions

- Query-only fix; no indexed term changes, so no reindex. The owner decides whether it is
  backported into alpha.11 (posted 2026-11-21, default: yes).

## May land early

None.

## Tasks

None: the fix and its test are Xapiand:0703f2b27 and Xapiand:fd33f492a on
explore/xapiand/keyword-open-range.
