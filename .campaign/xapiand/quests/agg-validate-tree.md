# agg-validate-tree: aggregations: validate the whole tree before the search

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status agg-validate-tree`.

## Outcome

A search whose aggregation tree names an unknown type anywhere, nested under a bucket included, is refused with a 400 before the search runs. Today bucket.h builds sub-aggregations lazily, so a nested unknown type is refused only when the first bucket forms at match time, and a search with no matching documents never refuses it (serialize-validation journal).

## Acceptance criteria

- A1 A nested unknown aggregation type under each bucket kind is refused with a 400 naming the type, before any document is matched, including on an empty index; tests fail without the fix
- A2 Valid nested trees still return the same aggregation results (existing aggregation tests pass unchanged)
- A3 The suite passes and CI is green

## Contracts

No API change.

## Decisions

- From hunt.md; filed by the lead 2026-11-19.

## May land early

None.
