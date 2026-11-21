# placement-locality-sim: journal

State: ready for review
Quest: .campaign/xapiand/quests/placement-locality-sim.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: the note is written at strata:4a641459.

## Next

1. Review. Lead: main changed docs/design/placement-strategies.md (scripted placement), so merge main at landing (`campaign land --merge-main`); the note already describes the scoring seam.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | check sim 261120-113014-f2178d (smoke + 26 hand-checked unit tests) at strata:7d31634b |
| A2 | Facebook 261120-113016-34489c (N=10,100), -113018-d8cecc, -113022-e0eb42, -113025-27329d (N=1000); Slashdot -113028-7a1b5f, -113031-f76ac7, -113034-79ca70, -113036-5cc371; synthetic -113039-a24e16, -113041-eb2b90, -113043-241386, -113045-1109aa; budget and drift -113047-03a766, -113049-c6cae1; root model -113051-f5d8a1 |
| A3 | [artifact] docs/perf/placement-strategies-sim.md at strata:4a641459: per-graph tables at 10/100/1,000 nodes, budget and hysteresis sweeps, SPAR comparison, root-index section, scoring seam, recommendation |

Coordinator messages: (1) root index served by `copies: all` against nearest member plus cache: modeled in sim/placement/root_index.py (job 261120-113051-f5d8a1), own section in the note. (2) Scripted placement seam: sim/placement/scored.py, a strategy is one scorer over (unit, node) and the engine enforces spread, peer bound and capacity after it; tested; named in the note.

## Result

Simulator: strata sim/placement/ (pure Python; `run.py --smoke` runs 26 hand-checked tests and small runs in under a second). Note: strata docs/perf/placement-strategies-sim.md. Headlines (jobs at strata:7d31634b):
- Two-level (partition voters plus budgeted read copies) on the synthetic workload at 1,000 nodes: 100% local, 3.07 copies per unit (4.12x bytes), at most 28 peers, churn 1.00x the minimum. SPAR: 4.54 copies (4.99x), 999 peers, 1.29x.
- Facebook at 1,000 nodes: full locality costs 14.3 to 14.6 copies and 855 to 909 peers; under the 64-peer bound two-level reaches 47.9% local at 3.74 copies (fan-out 22.4 to 8.9); Slashdot 71.2%.
- SPAR's balance rule: literal reading COV 1.92 (41x load); strict reading balanced but 14.56 copies, no better than partition+local (14.30).
- Hysteresis h=0.25: 33% (synthetic) and 44% (Facebook) fewer read-copy bytes moved after a 10% rate redraw.
- Defects found in the design's rules: hub promotion by node fraction promotes 25,452 of 63,731 users at 10 nodes; one shared relay tree puts its root over the 64-peer bound (99).
- Root index: cache sends fewer messages from 14 to 19 nodes; at 10,000 nodes and 100/s it costs 0.03 cores against 10.
- Recommendation: copyset voters with declarative strategies first, then two-level locality, root through a cache; not SPAR's online moves.

## Rulings I made

- Pure-Python, stdlib-only simulator; the offline ideal is our own METIS-style multilevel partitioner (heavy-edge matching, BFS seed, size-constrained greedy refinement), not networkx (not installed, and this avoids a dependency).
- Graphs: Facebook New Orleans WOSN links (SPAR's own dataset, so results can be checked against the paper) and SNAP Slashdot0902; neither states a license, both ask for citation. Kept in .scratch/, not committed.
- All strategies keep 3 durable copies (SPAR K=2) so the comparison is like for like; SPAR's slaves are asynchronous and its reads go to masters only, as published.
- Hash is simulated by its distribution (uniform random sets); its churn moves the repair plus each joiner's rendezvous share. Copyset strategies replace a node in its slots, so churn moves exactly the bytes it held.
- SPAR's balance rule (§4.2) read literally lets masters pile up (master COV 0.47/1.14/1.92 at N=10/100/1000 on Facebook, job 261120-112346-77e38b and -112348-bff2cf at 3c2abca0) against the paper's 0.0019; default `spar` moves a master only to a partition with fewer masters, `spar-literal` keeps the other reading.
- SPAR moves are evaluated only for users of degree at most 2000 (all of Facebook's; it skips the synthetic hubs, whose move can never pay).
- Copysets are nodes j, j+N/3 and j+2N/3, each moved forward to a new rack, so every node is in about three copysets.
- Adaptive locality: request-completion greedy (rate per byte of missing copies) at the anchor leader, locality budget beta x mean voter bytes per node, peer bound 64 with explicit feeders, hubs promoted when read copies reach half the other nodes; hysteresis discounts incumbents by 1+h.
