# track-c-c3-gate: C3 gate report: T5 rows and the repair mutant at consensus main, three platforms

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-c-c3-gate`.

## Outcome

C3's gate report: T5's rows and T2's repair-below-`base_index` mutant at `consensus` main through `Driver::repair`, on macOS, Linux and FreeBSD, with the architect's review (Track C A4, from strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007)).

## Acceptance criteria

- A1 T5's rows and the repair mutant pass at consensus main on macOS, Linux (VM) and FreeBSD (VM): jobs
- A2 [artifact] `docs/gates/track-c-c3.md` reports each row with its job id
- A3 [artifact] The architect's review approves: `docs/reviews/track-c-c3.md`

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- C3 passes at the Core level: `Driver::repair` only forwards and touches no log state (review F3, 11-18); Driver-level repair rows are owed to consensus-sim-loops A3, repair liveness to its A5. The gate also waits on consensus-c3-rows (review F1, F2).

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
