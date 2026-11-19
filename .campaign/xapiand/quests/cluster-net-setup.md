# cluster-net-setup: VM net setup is all-or-nothing; IPv6 obeys partitions and shapes

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cluster-net-setup`.

## Outcome

cluster-vm-tier's review findings (.campaign/xapiand/reviews/cluster-vm-tier-correctness.md) are fixed: a guest's first tc/nft setup either completes or fails the command (N1: a later `shape` exited 0 and did nothing, 50 ms measured 1.28 ms), and partitions and shapes cover IPv6 on l2 (N2). N3-N7 fixed where cheap, otherwise noted in the harness docs.

## Acceptance criteria

- A1 A setup stopped partway makes the next `shape` or `partition` redo it or fail, never exit 0 doing nothing: a net_test.py case that fails without the fix
- A2 IPv6 traffic between a partitioned pair is dropped and a shaped pair is delayed: a VM job with values
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: follow-up of cluster-vm-tier review N1-N7; cluster measurements depend on shaping really applying.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
