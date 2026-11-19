# nightly-retry: nightly dispatcher retries transient GitHub errors

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status nightly-retry`.

## Outcome

cpp-libraries' nightly dispatcher retries transient GitHub errors (HTTP 5xx, timeouts, connection resets) with bounded backoff, so one 502 at start no longer dispatches nothing (Nov 16, 17, 18 runs 43898843833, 44120109909, 44337538295).

## Acceptance criteria

- A1 A unit test with a fake transport: two 502s then success dispatches; a 4xx fails at once; persistent 5xx gives up after the bound with a clear error; fails without the retry
- A2 Tests pass and the cpp-libraries CI is green

## Contracts

No API change.

## Decisions

- From hunt.md; filed by the lead 2026-11-19.

## May land early

None.
