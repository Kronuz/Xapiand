# htm-edge-points

Outcome: `HTM::getId` gives a point lying on an edge of the root mesh (latitude 0, longitude 0 or 90/180/270, the poles) the id of a trixel that contains it, so a circle around such a point finds it. Today it returns `N3333...` (geo-correctness F20, D129): `inside<Round>` tests `> 0`, which is false on the edge.

## Acceptance criteria

- A1 Points on every root-mesh edge (lat 0 at several longitudes, lon 0/90/180/270 at several latitudes, both poles) get an id whose trixel contains them, and a 1 km circle around each covers that id: a test failing at the parent commit
- A2 Random ids change only where the old trixel did not hold the point: `htm_diff` over at least 1,000,000 random points shows every changed id's old trixel excludes the point, and its new trixel is the nearest one, within double rounding of the point
- A5 `HTM::intersectConstraints` clamps the dot product to [-1, 1] before `acos` (htm.cc:431), so a small circle never reads as disjoint from its own center's trixel: a test failing at the parent commit (review F3; 1 m circles miss their center for 0.09% of random points)
- A3 [artifact] htm's goldens regenerated; `CHANGELOG.md` says which ids change (points on root-mesh edges) and that indexes holding such points must reindex them
- A4 The suite passes in Release and in Debug (asserts on), and CI is green on every platform

## Decisions

Closed edges plus a descent that takes child 3 only if it holds the point, else the nearest child (lead, 2026-11-19): rounded midpoints leave slivers no child holds; the closed test alone leaves 22,374 of 655,340 edge points unheld (worst 1,850 km); this leaves 0, and every random id it changes had an old trixel that excluded the point (from level 17; up to 2,120 km near deeper vertices), so all stored geo points are reindexed.
