# geo-correctness: journal

State: waiting on jobs 261118-231033-23ce28 261118-231034-bee9e4 261118-231042-09af0d 261118-231043-0a7400 261118-231044-1e0858
Quest: .campaign/xapiand/quests/geo-correctness.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All five findings handled in code at xapiand:56afff06b (tests at 673c25282 first); table commit follows. Jobs
submitted: fails-without 261118-231033-23ce28 (at 673c25282), passing 261118-231034-bee9e4 (56afff06b), corpus before
261118-231042-09af0d (673c25282, print), corpus after 261118-231043-0a7400 (bb8403367, table check + print), suite
261118-231044-1e0858. F20 is not the convex: see Blocked on.

## Next

0. msg D129 (lead): F20 is fixed in htm quest htm-edge-points; after it lands I repin htm (CMakeLists.txt joins my file set) and flip the F20 pins. Branch not pushed by me: contributors never push; the lead pushes for CI.

1. Read the jobs when they finish (`campaign brief geo-correctness`); fold in a CI failure on any platform.
2. When coverage-areas lands and the lead merges master and widens the file set: flip the pinned assertions in
   tests/test_coverage_geospatial.cc (F18 `MULTIPOINT(1 2, 3)` is an EWKTError; F19 `with_empty.toWKT()` ends
   EMPTY; F21 and F22 add `covers(...)` of the center and the 5,500 km point; F20 stays pinned until htm moves).

## Blocked on

F20, the lead's decision (owner for the id change): the lens is right; `HTM::getId` is wrong for a point on an
edge of the root mesh (lat 0, lon 0, poles): it returns the id of the middle of the octant, `N3333...`, so a
point at (0, 10) is indexed in a trixel the 1 km circle around it does not cover. Real search bug, not only the
lens. The fix is one line in the htm library (`inside<Round>`: `> 0` to `>= 0`, closed edges). Measured on a
scratch copy of the pinned htm: all 8 pinned lens misses gone; every edge point hits its own 1 km circle. It
changes ids of edge points only, but htm's test/golden.h pins them (92 of 396 golden points mismatch) and ids
live in indexes. Options: (A, recommended) fix in htm, regenerate its goldens, repin Xapiand, flip the F20
assertions; edge-point ids were wrong anyway; (B) leave it and document that lat 0 / lon 0 points are not
searchable by area (not acceptable by the lead's own decision on wrong answers); (C) a workaround inside
Xapiand's Point/MultiPoint (a second id rule next to htm's): not recommended. docs/design/geo-correctness.md.

## Evidence

Jobs prove criteria by their tags (`--for A1`).

| Criterion | Evidence |
|---|---|
| A1 | F18, F19, F21, F22 (and F23): fails-without and passing jobs below. F20: [decision] the convex is right (lens covers its center at 60 generic centers, tests/test_geo_correctness.cc `test_lens_of_wide_circles_covers_its_center`, and at all four root vertices once HTM::getId places them: scratch htm patch, 8 of 8 fixed); the wrong answer is HTM::getId, in the htm library; docs/design/geo-correctness.md |
| A2 | tests/test_geo_corpus.cc: 6,015 corpus points, 33 queries, 3 settings = 99 answers; 12 change, all listed in its table (`before`): convex apart x3 (6015 hits to 0, F23), convex negative p=false (4396 to 5340, F21), convex negative hole 1000km x2 (6015 to 6007, 5071 to 5978, F21), multipoint plain x3 and multipoint plain one x3 (error to parsed, F18). The other 87 are equal: jobs below |
| A3 | suite job 261118-231044-1e0858 at bb8403367; CI: the lead pushes, I do not |

## Result

<When ready for review.>

## Rulings I made

- F23 (new): circles that never meet simplify to an empty convex, whose getRanges read begin() of an empty
  vector and returned the whole sphere (query `CONVEX((a,r),(b,r))` with disjoint circles matched everything).
  Fixed with the rest: it is the same convex code and the corpus probe found it.
- F19 fix is in Collection (skip empty intersections when writing, `empty()` true for only-empty
  intersections), not in the parser: the parser already reads `GEOMETRYCOLLECTION EMPTY`.
- F22 is the order of circles in an unsimplified convex; the engine simplifies (getGeometry), so only a
  directly built convex showed it. Fixed in the convex (`smallest()` by arcangle, sign kept by `add(Convex)`),
  not by sorting on add: that would change the order circles are written in.
- F20 not fixed here: the cause is outside my file set and changes stored ids (Blocked on).
