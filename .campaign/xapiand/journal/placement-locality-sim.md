# placement-locality-sim: journal

State: waiting on jobs 261120-113016-34489c 261120-113018-d8cecc 261120-113022-e0eb42 261120-113025-27329d 261120-113028-7a1b5f 261120-113031-f76ac7 261120-113034-79ca70 261120-113036-5cc371 261120-113039-a24e16 261120-113041-eb2b90 261120-113043-241386 261120-113045-1109aa 261120-113047-03a766 261120-113049-c6cae1
Quest: .campaign/xapiand/quests/placement-locality-sim.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Sweeps queued as jobs at strata:7d31634b (SPAR balanced strictly, scoring seam added); the 3c2abca0 sweeps are superseded.

## Next

1. Collect the sweep jobs (`campaign job log <id> | grep -v RESULT` prints each table) and fill the Results, "What SPAR gets right" and Recommendation sections of docs/perf/placement-strategies-sim.md (draft at strata:238ecff9: method, workloads, root-index and scoring-seam sections done; synthetic unit counts pending from the job header).
2. Write "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | check sim 261120-113014-f2178d (smoke + 26 hand-checked unit tests) at strata:7d31634b |
| A2 | Facebook 261120-113016-34489c (N=10,100), -113018-d8cecc, -113022-e0eb42, -113025-27329d (N=1000); Slashdot -113028-7a1b5f, -113031-f76ac7, -113034-79ca70, -113036-5cc371; synthetic -113039-a24e16, -113041-eb2b90, -113043-241386, -113045-1109aa; budget and drift -113047-03a766, -113049-c6cae1; root model -113051-f5d8a1 |
| A3 | pending |

Coordinator messages: (1) root index served by `copies: all` against nearest member plus cache: modeled in sim/placement/root_index.py (job 261120-113051-f5d8a1), own section in the note. (2) Scripted placement seam: sim/placement/scored.py, a strategy is one scorer over (unit, node) and the engine enforces spread, peer bound and capacity after it; tested; named in the note.

## Result

Pending.

## Rulings I made

- Pure-Python, stdlib-only simulator; the offline ideal is our own METIS-style multilevel partitioner (heavy-edge matching, BFS seed, size-constrained greedy refinement), not networkx (not installed, and this avoids a dependency).
- Graphs: Facebook New Orleans WOSN links (SPAR's own dataset, so results can be checked against the paper) and SNAP Slashdot0902; neither states a license, both ask for citation. Kept in .scratch/, not committed.
- All strategies keep 3 durable copies (SPAR K=2) so the comparison is like for like; SPAR's slaves are asynchronous and its reads go to masters only, as published.
- Hash is simulated by its distribution (uniform random sets); its churn moves the repair plus each joiner's rendezvous share. Copyset strategies replace a node in its slots, so churn moves exactly the bytes it held.
- SPAR's balance rule (§4.2) read literally lets masters pile up (master COV 0.47/1.14/1.92 at N=10/100/1000 on Facebook, job 261120-112346-77e38b and -112348-bff2cf at 3c2abca0) against the paper's 0.0019; default `spar` moves a master only to a partition with fewer masters, `spar-literal` keeps the other reading.
- SPAR moves are evaluated only for users of degree at most 2000 (all of Facebook's; it skips the synthetic hubs, whose move can never pay).
- Copysets are nodes j, j+N/3 and j+2N/3, each moved forward to a new rack, so every node is in about three copysets.
- Adaptive locality: request-completion greedy (rate per byte of missing copies) at the anchor leader, locality budget beta x mean voter bytes per node, peer bound 64 with explicit feeders, hubs promoted when read copies reach half the other nodes; hysteresis discounts incumbents by 1+h.
