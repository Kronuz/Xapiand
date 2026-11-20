# coverage-server: server (search_views) reaches T13 coverage through E2E drivers

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status coverage-server`.

## Outcome

The server area reaches T13's target, search_views.cc first (792 uncovered branches; server was 76.8% lines, 49.4% branches at 277715). Unit tests can't reach it, so the coverage comes from E2E drivers run in the instrumented build.

## Acceptance criteria

- A1 Line and branch coverage of server at or above T13's target: harness/coverage.sh before and after
- A2 Every new test checks values, not only that code ran; a test of a fixed bug fails without the fix
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: split from coverage-areas (its journal, "Proposed split"); floors stay at T13, never reset as a ratchet.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
