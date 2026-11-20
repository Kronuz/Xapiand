# datetime-range-zone: datetime: map ranges and zone offsets compute the right instant

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status datetime-range-zone`.

## Outcome

A datetime given as a map selects the range it names, and a zone offset is applied in the right order, so a datetime with an offset indexes and queries as the same instant as its UTC form. coverage-core-small pinned both as wrong (job 261119-011017-86a323; its tests pin today's output).

## Acceptance criteria

- A1 A datetime map range (e.g. `{"_year": 2020, "_month": 1}` forms) selects exactly its span: test with boundary instants; fails without the fix
- A2 `2020-01-02T10:00:00+02:00` equals `2020-01-02T08:00:00Z` when indexed, stored and queried, for positive, negative and half-hour offsets: test; fails without the fix
- A3 coverage-core-small's pins for these two cases flip to the correct values (tests/test_coverage_core*: name them in the journal; if outside the set, say so for the lead); suite passes and CI green

## Contracts

Index path handling (user-visible: fixes only; `:port` URIs are refused).

## Decisions

- From hunt.md rows (coverage-core-small, cheap-reopen F2); filed 2026-11-19.

## May land early

None.
