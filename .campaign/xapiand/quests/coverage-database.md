# coverage-database: database (schema.cc) reaches T13 coverage

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status coverage-database`.

## Outcome

The database area reaches T13's target, schema.cc first (3,969 lines; database was 61.1% lines, 51.4% branches at 277715).

## Acceptance criteria

- A1 Line and branch coverage of database rises from the 55.3% / 45.4% baseline: harness/coverage.sh before and after (lead, 2026-11-19: T13's 90/80 moved to quest coverage-database-t13; the gap, ~4,000 lines at 64.2% / 53.8%, is several quests of work)
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
