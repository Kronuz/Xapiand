# coverage-areas: The six areas below 7c's coverage target reach it

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status coverage-areas`.

## Outcome

Aggregations, lua, geospatial and decoders reach x7-gate's T13 coverage target, the instrumented build loads one C++ runtime, the exit-time mutex abort is fixed, and the reader registry's bytes are measured (D90). The other areas moved to follow-up quests.

## Acceptance criteria

- A1 Line coverage at or above T13's target in aggregations, lua, geospatial and decoders: a coverage job before and after
- A4 The aggregations test's exit-time `mutex lock failed` abort (20 of 20 under one libc++) is fixed in the product, with a check that fails without the fix: a job
- A2 The reader registry's bytes per open index measured: a job
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: split. This quest keeps aggregations, lua, geospatial and decoders. coverage-multivalue, coverage-core-small, coverage-server, coverage-database and coverage-core-main take the rest. Floors stay at T13 (no ratchet reset); server and core-main use E2E drivers.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
