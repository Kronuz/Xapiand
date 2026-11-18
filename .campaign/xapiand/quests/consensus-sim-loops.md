# consensus-sim-loops: Driver simulation on hosts of 2-4 loops, groups sharing and not sharing turn logs (T2)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-sim-loops`.

## Outcome

The `Driver` simulation runs hosts of two to four loops with at least eight multi-voter groups per host, so groups share and do not share turn logs (Track C A2, from strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007)).

## Acceptance criteria

- A1 T2's budget at several loops per host (seeds and steps stated), every oracle green: jobs
- A2 A mutant that acknowledges a record before its own loop's turn barrier is killed: a `--fails-without` job
- A3 The Driver simulation issues repairs (at and below the commit cursor, through `Driver::repair`), and T2's repair-below-`base_index` mutant is killed through the Driver, not only at `Core::step` (track-c-c3-gate finding, 11-18): a `--fails-without` job
- A5 Repair liveness under load (track-c-c3-gate review F4): the A1 summary reports repairs requested against completed and the longest wait, with none left pending at the end of a seed: the A1 jobs
- A4 The build-test check passes, and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
