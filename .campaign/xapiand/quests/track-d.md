# track-d: Track D, distribution

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-d`.

## Outcome

strata's distribution design (strata `docs/DISTRIBUTION.md`) is built in a deterministic cluster
simulator first and on real nodes second: the scale contract and a simulator of at least 100,000 nodes
and 10^7 shards, gossip membership over Detent's peer transport, a partitioned placement authority,
index namespaces, the query path at scale, storage tiers, the blob store decision, and rolling upgrades.
Each item passes its gate at the scale contract and on a real cluster of at least 30 nodes. This quest
holds the design and D1; the lead splits D2 to D8 into quests of their own when distribution resumes.

## Acceptance criteria

From strata `docs/PLAN.md`, "Track D: distribution (parallel, from Phase 3)". Gate per item: the
simulator's runs at the scale contract (no acknowledged loss, no split placement, bounded convergence
and fan-out latency), measurements on a real cluster of at least 30 nodes, and the architect's review.

- A1 D1: the scale contract and a deterministic simulator of at least 100,000 nodes and 10^7 shards (time, network, disks and failures simulated), shared with Track C's C5; fidelity tiers stated with the hardware each runs on: simulator jobs
- A2 [artifact] D1's design passes the architect's re-check of revision 1 (SIM-R1 to SIM-R19 folded; SIM-R20 to SIM-R27 parked), and the owner answers its Q1 to Q6: `docs/design/simulator.md`
- A3 D2: membership and failure detection by SWIM-style gossip over Detent's peer transport (Xapiand's UDP multicast discovery and UDP Raft are not carried forward): simulator and cluster jobs
- A4 D3: a partitioned placement authority (a strict-consensus root and placement groups), rebalancing, draining, rack- and zone-aware placement, per-index placement strategies and adaptive locality copies (strata `docs/design/placement-strategies.md`); after C1 and C2: simulator and cluster jobs
- A5 D4: index namespaces, Xapiand's index per path mapped to shards and groups, with aliases and resharding by split: jobs
- A6 D5: the query path at scale: fan-out trees with aggregator tiers, mergeable partial results, global statistics, P10's threshold sharing, partial results under timeouts, hedged requests: jobs
- A7 D6: storage tiers: local segments, and immutable segments on object storage with stateless searchers for cold indexes (the cold tier's reserved remote tier, strata `docs/design/cold-tier.md` D8): jobs
- A8 [decision] D7: whether Xapiand's Haystack-style blob volumes stay in the node or become a separate service, decided before Phase 7 (strata `docs/DISTRIBUTION.md`, "Open decisions"; Phase 7c's Q4 left `stored` blobs in `DataStorage`): a Decisions row
- A9 D8: rolling upgrades with mixed versions, and the wire and format compatibility rules (N and N-1, SYS-3): simulator and cluster jobs
- A10 [manual] A test cluster of at least 30 nodes exists before Track D's first gate (no rented machines: the owner provides it): the owner names it

## Contracts

Consumes Track C's peer transport and consensus (`track-c`); provides the placement authority and the
query path that Phase 3R and Phase 7b build on.

## Decisions

- Parked by the owner (2026-10-20, D4): distributed work parks behind engine speed; the single-node
  release ships first and 7b follows it (2026-10-29, D9). Blocked until the owner resumes distribution.
- Adopts `design/simulator` (strata:27cfb3033, 2026-10-17: revision 1 in review; S1 the engine, S2 the
  network, S3 the checker, green in CI and under ASan and UBSan; 961 commits behind strata main).
- Campaign never rents machines: A10's cluster and the bare-metal rows need hardware the owner provides
  (PLAN open question 9).

## May land early

The design documents and the simulator behind its own build option.
