# track-c-gates: C2 report completed and C4 report written at consensus and journal main

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-c-gates`.

## Outcome

`docs/gates/track-c-c2.md` completed and `docs/gates/track-c-c4.md` written from jobs at `consensus` and `journal` main, with the architect's review of each (Track C A9, from strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007)).

## Acceptance criteria

- A1 [artifact] `docs/gates/track-c-c2.md`: C2's rows rerun at today's journal and consensus, each with its job id
- A2 [artifact] `docs/gates/track-c-c4.md`: C4's rows from consensus-hello-limits and consensus-flow-bench, each with its job id
- A3 [artifact] The architect's review of each approves: `docs/reviews/track-c-c2.md`, `docs/reviews/track-c-c4.md`

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
