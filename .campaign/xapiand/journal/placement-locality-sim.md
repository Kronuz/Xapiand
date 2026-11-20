# placement-locality-sim: journal

State: working
Quest: .campaign/xapiand/quests/placement-locality-sim.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Sweeps queued as jobs at strata:3c2abca0 (simulator in sim/placement/, smoke green).

## Next

1. Collect the sweep jobs, write docs/perf/placement-strategies-sim.md (tables, SPAR vs corrected design, recommendation, root-index section for msg from the coordinator).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | check sim 261120-112255-ce93b5 (smoke + 20 hand-checked unit tests) at strata:3c2abca0 |
| A2 | sweeps 261120-112346-77e38b, -112348-bff2cf, -112350-abb449 (Facebook WOSN), -112352-a828f9, -112354-c1b7d3, -112359-c0357c (Slashdot), -112402-7af7ea, -112404-a3bc19, -112409-5c72ab (synthetic), -112412-a61980, -112413-06cbe8 (budget and drift); root model 261120-112332-ce3e88 |
| A3 | pending |

Coordinator's root-index question (comparing `copies: all` with nearest member plus cache): modeled in sim/placement/root_index.py, job 261120-112332-ce3e88; it will get its own section in the note.

## Result

Pending.

## Rulings I made

- Pure-Python, stdlib-only simulator; the offline ideal is our own METIS-style multilevel partitioner (heavy-edge matching, BFS seed, size-constrained greedy refinement), not networkx (not installed, and this avoids a dependency).
- Graphs: Facebook New Orleans WOSN links (SPAR's own dataset, so results can be checked against the paper) and SNAP Slashdot0902; neither states a license, both ask for citation. Kept in .scratch/, not committed.
- All strategies keep 3 durable copies (SPAR K=2) so the comparison is like for like; SPAR's slaves are asynchronous and its reads go to masters only, as published.
- Hash is simulated by its distribution (uniform random sets); its churn moves the repair plus each joiner's rendezvous share. Copyset strategies replace a node in its slots, so churn moves exactly the bytes it held.
- SPAR moves are evaluated only for users of degree at most 2000 (all of Facebook's; it skips the synthetic hubs, whose move can never pay).
- Copysets are nodes j, j+N/3 and j+2N/3, each moved forward to a new rack, so every node is in about three copysets.
- Adaptive locality: request-completion greedy (rate per byte of missing copies) at the anchor leader, locality budget beta x mean voter bytes per node, peer bound 64 with explicit feeders, hubs promoted when read copies reach half the other nodes; hysteresis discounts incumbents by 1+h.
