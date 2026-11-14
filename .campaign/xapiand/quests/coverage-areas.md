# coverage-areas: The six areas below 7c's coverage target reach it

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status coverage-areas`.

## Outcome

The six areas below x7-gate's T13 coverage target reach it, and the reader registry's bytes are measured (D90).

## Acceptance criteria

- A1 Line coverage per area at or above T13's target in each of the six areas: a coverage job before and after
- A2 The reader registry's bytes per open index measured: a job
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
