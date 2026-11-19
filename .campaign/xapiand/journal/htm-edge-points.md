# htm-edge-points: journal

State: blocked on lead: closed rule alone misses 3.4% of dyadic edge points (A1); the full fix changes 0.42% of random ids (A2 as written)
Quest: .campaign/xapiand/quests/htm-edge-points.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on the lead's choice between two branches (both in Kronuz/htm, local worktree):

- (a) quest/xapiand/htm-edge-points = htm:aa46816 + htm:b572ff0 (closed `>= 0`, the
  quest's decided one-liner, plus test.cc's axis ids). Pushed; jobs at b572ff0: A1
  261118-235236-7257de, A1 fails-without 261118-235238-6f0465, A2 261118-235240-ba7661,
  suite 261118-235244-a2968f, Debug ctest 261118-235315-6f984c. CI run pending.
- (b) explore/htm-edge-nearest = (a) + htm:7c33b0a (child 3 only when it holds the point,
  else the child the point is least outside of). Local only. Jobs at 7c33b0a: A1 dyadic
  261118-235302-13c067, fails-without on (a)'s htm.cc 261118-235304-011c3c, Debug ctest
  (htm_diff A2 check, restated) 261118-235307-3e9c50.

## Next

1. On (a): finish CI, record jobs, add a CHANGELOG known-issue line for the dyadic misses.
2. On (b): fast-forward quest branch to 7c33b0a, push, CI, record jobs.
3. Ready for review.

## Blocked on

Lead (changes the quest's Decision and A2). The decided one-liner does not meet A1's
outcome for every root-edge point. Probe (scratch, 655,340 points on the equator and the
four meridians at 90/2^m spacing, m <= 14, lat/lon in degrees):

| | edge points not held | worst | random ids changed (1,000,000) |
|---|---|---|---|
| cadec57 (open) | 262,181 | 6,090 km | - |
| (a) closed only | 22,374 (3.4%) | 1,850 km (lat 0 lon 22.5) | 0 |
| (b) closed + nearest child | 0 | 0 | 4,215 (0.42%), levels 20-25 |

Cause: midpoints are rounded, so children's edges miss the parent's by rounding and leave
slivers in no child; the descent sent such a point to child 3, which does not hold it
either. Near a deeper vertex on a root edge that happens at shallow levels (km); for random
points from level 20 down (0.3-10 m cells). Every one of the 4,215 changed random ids was
outside its point's trixel (worst 7.74 m); the new ones are within rounding (<= 6.6 mm,
the measurement's own noise). Cost: none measured (htm_bench getId best of 3 interleaved
runs, strict beside 6 light holds: open 1223/1243/1272 ns, (a) 1222/1263/1274, (b)
1214/1253/1240). (b) restructures the test so child 3's check is free (its edges are exact
negations of the corner children's inner edges).

Options: (a) land the one-liner, state the dyadic miss as a known issue and a follow-up;
(b) land the full fix, restate A2 as "random ids change only where the old trixel did not
hold the point" (htm_diff checks exactly that: 972 of 225,000 differ, all slivers).
Recommendation: (b). It fixes the cause, it is free, and every id it changes was wrong;
stored ids that change outside the root edges move by <= 7.74 m, so only points with a
zero coordinate need the reindex.

## Evidence

Branch (a) head b572ff0 (rerun after the test.cc fix; aa46816 jobs passed too:
261118-233612-167c31, -233616-40d68a, -233619-f5ccea, -233622-7075f8).

| Criterion | Evidence |
|---|---|
| A1 | 261118-235236-7257de (htm_edge, 140 points); fails without: 261118-235238-6f0465 (edge test on cadec57's htm.cc: 110 of 140 fail) |
| A2 | 261118-235240-ba7661 (test/compare_ids.sh: 100,000 random ids, cadec57 vs head); htm_diff in the suite holds 225,000 random points to the open rule's ids |
| A3 | [artifact] htm test/golden.h re-id'd (92 of 396 change: 75 with a zero coordinate, 17 exact deeper-trixel vertices/midpoints); htm CHANGELOG.md 2026-11-18 names the changed ids and the reindex. On (b): 238 of 396, plus 19 of 48 fusing probes in htm.cc |
| A4 | suite 261118-235244-a2968f; Debug ctest 261118-235315-6f984c; CI on quest/xapiand/htm-edge-points (first push failed on test.cc asserts, Debug only; fixed in b572ff0) |

## Result

(a) htm:aa46816 + htm:b572ff0; (b) adds htm:7c33b0a. Numbers in "Blocked on". Docs:
htm ARCHITECTURE.md "Naming a point" (tie rule; on (b) the nearest-child rule),
CHANGELOG.md 2026-11-18. Scratch probes in ~/.scratch/xapiand/htm-edge-points (probe.cc
dyadic sweep, rand.cc random sliver count, holds.cc golden distances).

Also for the lead: the campaign suite for htm builds Release, so test.cc's asserts
(NDEBUG) never ran; CI's Debug builds caught them. The Debug ctest job above covers it.

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
- golden.h re-id'd in place (same points, ids from the new ieee reference) instead of
  rerunning gen_golden.cc: its last class needs a fusing arm64 build to find the same
  points; this host is an x86 i9, so a rerun would swap points and blur the diff.
- test.cc's axis-point ids updated (the asserts pinned the N3333... ids).
