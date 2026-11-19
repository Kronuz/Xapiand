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

The cause was in the htm library (`htm.cc`, `HTM::getId`): a point on an edge of a root trixel fell to the middle
child at every level. htm `0c09a02` places an edge point in the nearest child that holds it, and
`intersectConstraints` clamps its result; the lens covers its center at all four root vertices
(`test_lens_of_wide_circles_covers_its_center`), and every point above hits its 1 km circle.

Ids move only for points on an edge, so the corpus answers move only for its 15 root-edge points (indexes 6000 to
6014). Rerun at the new pin, 24 of the 99 answers differ from the pin before it (same code otherwise), in 12
queries and 53 points, every one an edge point: 24 gained, 29 lost. Against the exact distance to each circle:

| Class | Count | Exact distance |
|---|---|---|
| Gained, inside the shape | 23 | inside by 821 to 3,940 km (`partials` false or true) |
| Gained, outside the shape | 1 | `convex cap and negative` with partials: 220 km outside; that setting also hits 76 other points up to 350 km outside, partial cells by design |
| Lost, inside the shape | 0 | |
| Lost, outside the shape | 29 | outside by 504 to 6,198 km: the old id put the edge point in the middle of its octant, which these shapes held |

With whole cells only (setting 2) every gained point is inside and every lost point outside. The queries that changed:
circle 5,000 / 9,000 / 15,011 km and equator 3,000 km, convex lens 9,000 / 5,000 km, convex negative, both ring orders,
and convex cap and negative; `tests/test_geo_corpus.cc` keeps each `before` and `now`.
