# Geospatial answers: F18-F23

Six wrong answers in `src/geospatial`, found by the first coverage tests (F18-F22) and by the corpus probe beside
them (F23). `tests/test_geo_correctness.cc` guards each; `tests/test_geo_corpus.cc` shows that no other answer moved.

| Finding | Wrong answer | Cause | Fix |
|---|---|---|---|
| F18 | `MULTIPOINT(1 2, 3)` raised a non-`EWKTError` (a 500); the valid `MULTIPOINT(1 2, 3 4)` raised too | the form without inner parentheses passed the empty range `(first, first)` to the point parser, and the parser's message held an unfilled `{}`, so `std::format` threw | parse each `lon lat` up to its comma; drop the `{}` |
| F19 | a collection of an empty intersection wrote `GEOMETRYCOLLECTION(GEOMETRYINTERSECTION EMPTY)`, which does not parse | `to_string` wrote every intersection, `empty()` counted them | skip empty intersections; `empty()` holds when all are |
| F20 | the lens of two 9,000 km circles 45 degrees either side of a point did not cover it | not the convex: see below | HTM, outside this repository |
| F21 | a convex of a circle past a hemisphere (the complement of a cap) left out the trixel holding its center and took whole the trixel holding the hole | the hole test looked for the center of the negative circle; the excluded cap is around its antipode | `HTM::thereisHole`, as `Circle` uses |
| F22 | a ring (a negative circle with a positive one) added largest first missed points inside both circles | resolution and the "smallest circle" came from `circles.begin()`, which is the smallest only after `simplify()` sorts; a convex added through another kept a stale sign | `smallest()` by arcangle; `add(Convex)` updates the sign |
| F23 | circles that never meet (an empty convex after `simplify()`) covered the whole sphere | `getRanges` read the first circle of an empty vector | an empty convex covers nothing |

The engine reaches a geometry through `EWKT::getGeometry()` and `GeoSpatial::getGeometry()`, which call `simplify()`,
so a convex from a query is sorted; F22 shows for a convex built with `add()` and used as is. F23 and F21 show on the
query path.

## F20: the point is on an edge of the mesh

The lens test puts its center at `(0, lon)` for `lon` in 0, 90, 180, -90: a vertex of the eight root trixels. The
lens covers the neighborhood of that point, in all four octants around it; the cover misses `HTM::getId` of the
point itself. `HTM::getId` descends by strict tests (`triple > 0` for all three edges), so a point on an edge of a
trixel is in none of its children 0 to 2 and falls to child 3, the middle one. Every level repeats it, and the id
is `N3333...`, the middle of the octant, whatever the point.

```
(0, 10)  -> N33333333333333333333333333   (the circle of 1 km around it: N32001..., S00002...)
(0, 0)   -> N33333333333333333333333333
(90, 0)  -> N33333333333333333333333333
```

So a point on the equator, on the prime meridian, or at a pole is indexed under the wrong trixel and a circle
around it does not find it: at `(0, 10)`, `(0, 0)`, `(10, 0)` and `(90, 0)` a 1 km circle misses its own point.
The circle test passes today by luck: a 9,000 km circle holds the middle of the octant too.

The cause is in the htm library (`htm.cc`, `inside<Round>`). Taking the edge as inside, `>= 0`, puts a point on
an edge in the first child that holds it, and with that one change (a scratch copy of the pinned htm
`bb5e01d`) every point above hits its 1 km circle and the lens covers its center at all four longitudes, both
partial settings (8 misses of 8 before, none after). It moves ids only for points on an edge, which are the ones
misplaced now; but htm's `test/golden.h` pins those ids (92 of its 396 golden points mismatch) and ids are stored
in indexes. Whether to move them, and when, is the owner's call; it is not made here.

When the htm id is fixed, the lens in `tests/test_coverage_geospatial.cc` flips to `covers(ranges, center)`, and
`test_lens_of_wide_circles_covers_its_center` can take the root vertices as centers.
