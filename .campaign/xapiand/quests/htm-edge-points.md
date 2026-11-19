# htm-edge-points

Outcome: `HTM::getId` gives a point lying on an edge of the root mesh (latitude 0, longitude 0 or 90/180/270, the poles) the id of a trixel that contains it, so a circle around such a point finds it. Today it returns `N3333...` (geo-correctness F20, D129): `inside<Round>` tests `> 0`, which is false on the edge.

## Acceptance criteria

- A1 Points on every root-mesh edge (lat 0 at several longitudes, lon 0/90/180/270 at several latitudes, both poles) get an id whose trixel contains them, and a 1 km circle around each covers that id: a test failing at the parent commit
- A2 Random ids change only where the old trixel did not hold the point: `htm_diff` over at least 1,000,000 random points shows every changed id's old trixel excludes the point and its new one holds it
- A3 [artifact] htm's goldens regenerated; `CHANGELOG.md` says which ids change (points on root-mesh edges) and that indexes holding such points must reindex them
- A4 The suite passes in Release and in Debug (asserts on), and CI is green on every platform

## Decisions

Closed edges plus a descent that takes child 3 only if it holds the point, else the nearest child (lead, 2026-11-19): rounded midpoints leave slivers no child holds; the closed test alone leaves 22,374 of 655,340 edge points unheld (worst 1,850 km); this leaves 0, and the 4,215 of 1,000,000 random ids it changes (levels 20-25) were all outside their point's trixel by up to 7.74 m.
