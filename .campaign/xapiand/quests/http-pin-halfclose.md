# http-pin-halfclose: Xapiand takes http's half-close fix

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status http-pin-halfclose`.

## Outcome

Xapiand pins http at ec273dd (http-halfclose, D126), so a half-closed client gets its :search and :count response (probe 261118-170529-87c0b4 showed 503 on master).

## Acceptance criteria

- A1 harness/wide_search_halfclose.py answers GET, :search and :count with 200 for a half-closed client, and fails at the old pin: a job and a `--fails-without` job
- A2 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: after coverage-areas, which holds CMakeLists.txt for the logger repin. TLS half-close (http review F4) is a hunt row.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
