# consensus-hello-limits: Node hello carries Limits and a wire version range; host transport reports PeerLimits

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-hello-limits`.

## Outcome

The node hello carries the node's ten `Limits` fields and a consensus wire version range; the host transport reports `PeerLimits` for each incarnation `member_of` places on that node; a host refuses a `maximum_frame` or group window below what its `rpc_bytes` needs. Track C A1 and SYS-3 (from strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007)). Order: after `detent:transport-feed-scan` (shared `host_transport.h`), before `format-freeze`.

## Acceptance criteria

- A1 A leader holds back an entry above a peer's hello-reported command limit, and a mutant that ignores the reported limits fails that test: a job and a `--fails-without` job
- A2 A hello whose limits fail `validate_limits` is refused: a test
- A3 A peer outside the version range is refused with a named error: a test
- A4 One CPU-hour of fuzzing on the hello's decoder, no crash: a job
- A5 The build-test check passes, and CI is green on every platform

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
