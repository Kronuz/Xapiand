# placement-locality-sim: Placement strategies compared in simulation: locality (SPAR-style) against hash, count, affinity and offline partitioning

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status placement-locality-sim`.

## Outcome

The owner wants to see how strata can keep related indexes on the same nodes (SPAR, "The Little
Engine(s) That Could") and how that compares with other replication and placement algorithms, before
any of it is built. A simulator over the design in strata `docs/design/placement-strategies.md`
answers that with numbers, without hardware and without touching the first release's placement.

## Acceptance criteria

- A1 A simulator places units (path subtrees, or index-shard pairs) on N nodes under each strategy:
  hash or random, count (rf voters plus read copies), affinity and follow (declared), offline graph
  partitioning (METIS-style, as the ideal), SPAR's online algorithm as published, and the design's
  corrected adaptive locality (read copies only, budgets, hysteresis, hub promotion). Each strategy
  has a unit test on a small graph with a hand-checked placement.
- A2 It replays co-access graphs: at least one public social graph (SNAP or similar, with its license
  noted) and one synthetic many-indexes workload shaped like ours (millions of small indexes, a few
  hubs, nested paths). It reports per strategy: replication overhead (copies per unit), the fraction
  of requests served on one node, fan-out per request, bytes moved per 1% membership churn, peers per
  node (the copyset bound), and voter spread over failure domains.
- A3 [artifact] `docs/perf/placement-strategies-sim.md`: the results table per graph and node count
  (10, 100, 1,000), what SPAR gets right and where the corrected design differs, and a recommendation
  for which strategies the placement after the first release should build first.

## May land early

Everything: it is a simulator and a note, not product code.
