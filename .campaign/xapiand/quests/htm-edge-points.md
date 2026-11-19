# htm-edge-points

Outcome: `HTM::getId` gives a point lying on an edge of the root mesh (latitude 0, longitude 0 or 90/180/270, the poles) the id of a trixel that contains it, so a circle around such a point finds it. Today it returns `N3333...` (geo-correctness F20, D129): `inside<Round>` tests `> 0`, which is false on the edge.

## Criteria

- A1 Points on every root-mesh edge (lat 0 at several longitudes, lon 0/90/180/270 at several latitudes, both poles) get an id whose trixel contains them, and a 1 km circle around each covers that id: a test failing at the parent commit
- A2 Ids of points off every edge are unchanged: a golden comparison over at least 10,000 random points, every id identical before and after
- A3 [artifact] htm's goldens regenerated; `CHANGELOG.md` says which ids change (points on root-mesh edges) and that indexes holding such points must reindex them
- A4 The suite passes, and CI is green on every platform

## Decisions

The fix is the one-line change from `> 0` to `>= 0` in `inside<Round>` unless A1/A2 show it assigns a point to two trixels inconsistently; ties go to one trixel deterministically.
