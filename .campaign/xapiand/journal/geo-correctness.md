# geo-correctness: journal

State: waiting on jobs 261119-024749-524b1b 261119-024740-da1dc6 (corpus hit indexes after and before the htm fix)
Quest: .campaign/xapiand/quests/geo-correctness.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

xapiand:532e055a5: master merged, htm repinned to 0c09a0283a507b2cf08975a7c8572d95ece60b68, pinned wrong answers flipped
(F18, F19, F20 x8 lens centers, F21, F22) in tests/test_coverage_geospatial.cc; D130 `_multiconvex` map form
(geospatial.cc: top level, collection, intersection; nested lists = one convex per inner list, flat = one) with
test_multiconvex_serves_in_the_map_form (tests/test_geo_correctness.cc) and the root-vertex lens centers (F20).
Fails-without commit 4866a9a0f (dangling scratch: head's tests over master's src/geospatial and CMakeLists).
Jobs: tests pass a00f7c; corpus table+print a4066a; fails-without f41c38 (geo_correctness) 82117a (coverage);
suite c04048; coverage fa34d7.

## Next

msg (lead, after the first job round): jobs at 97b397153: A1 pass a00f7c, coverage fa34d7, fails-without f41c38 and
82117a green. Suite c04048 failed with exactly one failing test, test_geo_corpus (53 of 54 pass); corpus a4066a: 24 of
100 rows differ under the htm fix, some by count DOWN (ring largest first, setting 0: 725 to 721), so "gains only" is
unproven: checked per point, not per count.

1. Corpus test now has `points` and `indexes` modes (xapiand:f686aa12a). Jobs: after (htm 0c09a02, head) 524b1b,
   before (old htm bb5e01d, fixes in; scratch commit 88191d384) da1dc6. Diff the sets per row with an exact-distance
   oracle for the circle and convex rows; classify each gained and lost point (root-edge point, sliver, other).
2. Regenerate the `expected` table at the htm-fixed code (`now`), keeping `before` for the F-fixes; record before/after
   per row under Evidence A2.
3. Rerun the corpus check and suite as light jobs --minutes 150; fill Evidence; mark ready if green.
4. CI at ec6c7d982 (Linux fix) is the lead's.

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
