# consensus-flow-bench: T7 on the host transport: throughput at 0/1/10 ms RTT, 100 groups with a slow follower, control-frame gap (R19)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-flow-bench`.

## Outcome

T7 on the host transport: one group's replication throughput at 0, 1 and 10 ms of added round trip, pipelined against `pipeline = 1`; 100 groups on one connection with one follower's barrier slowed 10x; the longest control-frame gap behind Bulk, which stands as R19's evidence (D123). Track C A5, from strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007). T7's snapshot row waits for `group-install`.

## Acceptance criteria

- A1 Throughput at 0, 1 and 10 ms added RTT, pipelined against `pipeline = 1`: exclusive jobs on the Linux VM with netem
- A2 100 groups on one connection, one follower's barrier slowed 10x: the other groups' throughput and the slow group's unsent bytes stay within the transport's stated bound: exclusive jobs
- A3 The longest control-frame gap behind Bulk traffic, against the bound the transport states (R19): a job
- A4 [artifact] `docs/perf/flow-control.md` with each row's job id

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
