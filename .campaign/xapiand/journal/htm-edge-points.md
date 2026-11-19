# htm-edge-points: journal

State: waiting on jobs 261118-233612-167c31 261118-233616-40d68a 261118-233619-f5ccea 261118-233622-7075f8
Quest: .campaign/xapiand/quests/htm-edge-points.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix committed (htm:aa46816) and pushed for CI. Jobs: A1 261118-233612-167c31, A1
fails-without (edge test on cadec57's htm.cc) 261118-233616-40d68a, A2 (100,000 random ids
at cadec57 vs head) 261118-233619-f5ccea, suite 261118-233622-7075f8.

## Next

1. Record job results; CI run on quest/xapiand/htm-edge-points (Kronuz/htm) green on every platform.
2. Ready for review.

## Blocked on

Nothing. Lead: the file set (include/**, src/**, ...) misses htm's root files the fix
must touch: htm.cc, CMakeLists.txt, ARCHITECTURE.md. See Rulings.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261118-233612-167c31 (htm_edge), 261118-233616-40d68a (--fails-without at cadec57's htm.cc) |
| A2 | job 261118-233619-f5ccea (test/compare_ids.sh, 100,000 random ids, cadec57 vs head); htm_diff holds 225,000 random points to the open rule's ids (suite) |
| A3 | [artifact] htm test/golden.h re-id'd (92 of 396 change: 75 with a zero coordinate, 17 exact deeper-trixel vertices/midpoints); htm CHANGELOG.md 2026-11-18 names the changed ids and the reindex |
| A4 | suite 261118-233622-7075f8; CI on quest/xapiand/htm-edge-points (pending) |

## Result

htm:aa46816: `inside<Round>` and `HTM::insideVertex_Trixel` test closed triangles (`>= 0`).
Local (x86-64, Release): htm_edge 140 edge points pass; htm_diff/fma/rounded fail only on
the 92 old goldens before the re-id. Docs: htm ARCHITECTURE.md "Naming a point" (tie rule),
CHANGELOG.md 2026-11-18. Perf not measured: the change swaps a `>` for `>=` in the same
compare, no extra work.

Finding (pre-existing, not fixed; lead decides): a point exactly on a *deeper*, non-axis
trixel edge (an exact vertex or midpoint as the descent computes it) can still get a trixel
that does not hold it, before and after. The children's midpoints are rounded off the
parent's great circle, leaving slivers in no child 0..2, so the point falls to child 3.
Measured on the 17 such golden points (scratch tool): old ids 27-1,110 km from the point,
new 21-530 km. Real coordinates hit it with probability ~1e-15 per point at shallow levels
(within one ulp of an edge); lat/lon inputs on axis planes, the real case, are fixed. A fix
(test child 3 in the else branch, pick the least-outside child) costs extra triples per
level; it would need its own quest with a bench.

## Rulings I made

- htm keeps sources at the root; the quest's file set has no root globs. The fix needs
  htm.cc (the one-line rule), CMakeLists.txt (the edge test target) and ARCHITECTURE.md
  (the tie rule). Edited them; lead to widen the file set.
- `HTM::insideVertex_Trixel` (public) gets the same closed rule: one tie rule for the
  descent and the predicate. Its Xapiand callers (circle.cc, convex.cc, polygon.cc) use
  it as "center/corner touches the trixel", where closed is the correct, more inclusive
  answer.
- test/ieee.h and test/frozen.h (the references) follow the closed rule; ieee.h keeps
  the open rule as `ieee::getId<false>` so htm_diff holds every random point to its
  pre-fix id.
