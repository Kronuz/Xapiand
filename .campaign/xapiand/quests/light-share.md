# light-share: Light requests keep their bound under heavy load

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status light-share`.

## Outcome

Light requests keep Phase 7c's bounds while heavy work saturates the node: a share of the worker pool and shard turns for light work (D86's P1). Fixes x7-gate's T4 (worst loop p99 6,144 us vs 1,024), T5 (loaded light p99 33.0 ms vs 8.91) and T10 (flood p99 48.0 ms vs ~20) (D90).

## Acceptance criteria

- A1 [artifact] Where the loaded light wait goes (worker pool, shard turns, loop) and the share chosen: `docs/design/light-share.md`
- A2 T4 and T5 at their 7c bounds: x7-gate's T4/T5 job (`--for`), strict, before and after
- A3 T10's flood p99 at its bound: x7-gate's T10 job, strict, before and after
- A4 Heavy throughput (T12 RESTORE and QPS) within the A/A band of before
- A5 The suite passes and CI is green at the landed code

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
