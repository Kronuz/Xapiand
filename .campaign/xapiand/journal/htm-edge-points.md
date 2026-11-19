# htm-edge-points: journal

State: waiting on jobs 261119-003445-380f20 261119-003446-097d2d 261119-003448-692923 261119-003449-9282f5 261119-003450-7f1ba0
Quest: .campaign/xapiand/quests/htm-edge-points.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

The review (CHANGES at d529b49) is folded in at htm:0c09a02, which is pushed:
- F1: CHANGELOG and ARCHITECTURE drop the sample bounds ("7.74 m", "levels 20 to 25").
  The guidance is now to reindex all stored geo points.
- F3/A5: `intersectConstraints` clamps the dot to [-1, 1]. htm_edge checks 1 m and 3 m
  circles around 50,000 random centers. Locally 0 of 100,000 miss.
- F4: getId is about 3% slower (820 against 797 ns, job 261119-002604-50bf6d).
- F2/A2: htm_diff already fails any changed random id without a no-child level, which
  means the old trixel excludes the point. It now also bounds the new trixel's distance at
  5 cm. Locally, over 1,125,000 random points, 4,700 changed with a worst of 6.7 mm.

Jobs at 0c09a02:
- A1,A5 htm_edge: 261119-003445-380f20
- A5 fails without the clamp (sed mutant): 261119-003446-097d2d
- A2 htm_diff 1000000: 261119-003448-692923
- Debug ctest: 261119-003449-9282f5
- Release suite: 261119-003450-7f1ba0

## Next

1. If all jobs and CI on 0c09a02 are green: ready for review (F1-F4 answered).

## Blocked on

Nothing. msg (lead: take b): done. msg (review CHANGES): F1, F3, F4 fixed and F2 checked at htm:0c09a02.

## Evidence

At head d529b49 (option b):

| Criterion | Evidence |
|---|---|
| A1 | 261118-235302-13c067 (htm_edge, 10,360 edge points; same tree at 7c33b0a, reused). Fails without: 261118-235815-bffda3 (b572ff0's closed-only htm.cc: 394 of 10,360 fail), 261118-235817-c86c33 (cadec57's open htm.cc: 4,225 fail) |
| A2 | 261118-235818-798d95 (htm_diff 1000000: random ids differ from the open rule only where the descent finds a sliver), pending |
| A3 | [artifact] htm test/golden.h: 238 of 396 ids change from cadec57, and a re-id at head changes 0. The 48 fusing probes in htm.cc carry new ids (19 changed). CHANGELOG.md 2026-11-18 lists which ids change: root-mesh edge points, exact deeper vertices and midpoints, and 0.42% at levels 20-25 that were up to 7.74 m outside their trixel. It names what to reindex: every point with a zero coordinate, or all points for exact ids |
| A4 | Release suite 261118-235821-6b7b9b; Debug ctest 261118-235820-219166; CI 44483693645 green on every platform |

## Result

Landed shape (b): htm:aa46816, htm:b572ff0, htm:7c33b0a, htm:d529b49. Numbers from the decision: 0 of 655,340 dyadic edge points missed (one-liner: 22,374; open: 262,181); about 0.4% of random ids change, all previously outside their trixel (review: up to 40.8 m random, 2,120 km near deeper vertices); no getId speed change measured. Docs:
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
