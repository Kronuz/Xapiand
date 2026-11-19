# geo-correctness: journal

State: ready for review
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

1. Read the corpus check and the suite (ids below); if green, fill Evidence and mark ready for review.
2. CI at ec6c7d982 (Linux fix) is the lead's.

Corpus under the htm fix (xapiand:48032de8b table, per-point diff of the hit sets before 88191d384 / after f686aa12a):
24 of 99 answers differ, 10 queries, 53 (point, answer) changes, all on the 15 root-edge points (indexes 6000-6014):
gained 23 inside the shape + 1 outside (a partial cell, 220 km out; setting 1 hits 76 points up to 350 km outside by
design); lost 0 inside + 29 outside (504 to 6,198 km). Setting 2 (whole cells): gained all inside, lost all outside.
No random or cluster point changes. Detail and classes: docs/design/geo-correctness.md.

## Blocked on

Nothing. (F20, formerly here, is fixed by the htm repin, D129.)

## Evidence

Jobs prove criteria by their tags (`--for A1`).

| Criterion | Evidence |
|---|---|
| A1 | Fixes F18, F19, F21, F22, F23 and D130 (`_multiconvex` map form), and the F20 htm repin: tests/test_geo_correctness.cc (incl. `test_multiconvex_serves_in_the_map_form`, root-vertex lens) and the flipped assertions in tests/test_coverage_geospatial.cc. Pass at head: 261119-041250-4fe1c5 (earlier at 532e055a5, same test and src files: 261119-004657-a00f7c). Fail without the fixes, htm pin and `_multiconvex` (xapiand:4866a9a0f, the head's tests over master's src/geospatial and CMakeLists): 261119-004729-f41c38 (test_geo_correctness, exit 8) and 261119-004732-82117a (test_coverage_geospatial, exit 8), both `--fails-without`. Coverage of src/geospatial from test_coverage_geospatial: 261119-004744-fa34d7 (geospatial.cc 85.7% lines, convex.cc 96.5%, collection.cc and multiconvex.cc 100%, ewkt.cc 95.1%) |
| A2 | tests/test_geo_corpus.cc: 6,015 corpus points, 33 queries, 3 settings = 99 answers. Before the htm fix 12 answers changed by the F-fixes (listed in the table's `before`; convex apart x3 6015 to 0 (F23), convex negative p=false 4396 to 5340 and hole rows (F21), multipoint plain x6 error to parsed (F18)). The htm edge-point fix moves 24 more, 10 queries, 53 (point, answer) changes, all on the 15 root-edge points: gained 23 inside the shape by exact distance and 1 outside (a partial cell, 220 km out), lost 0 inside and 29 outside (504 to 6,198 km); per-point jobs 261119-024749-524b1b (after) and 261119-024740-da1dc6 (before); detail in docs/design/geo-correctness.md. Table check at the htm fix: 261119-040317-8d9156 (exit 0). The corpus calls `getRanges` on geometries directly: it does not exercise geo global prefixes or an accuracy list, so M3 (coverage-multivalue, src/multivalue/generate_terms.cc: prefixes indexed in accuracy order, looked up reversed) is out of scope here |
| A3 | Suite at head: 261119-041251-84206c; earlier green at fdc169e74: 261119-040319-57eeba; CI: the lead pushes, I do not |

## Result

F18-F23 fixed and D130 served (docs/design/geo-correctness.md); htm repinned to 0c09a02; every pinned wrong answer in
tests/test_coverage_geospatial.cc flipped; the corpus table regenerated with its before/after per row. Review: one
correctness review (normal risk). Head recorded below.

## Rulings I made

- F23 (new): circles that never meet simplify to an empty convex, whose getRanges read begin() of an empty
  vector and returned the whole sphere (query `CONVEX((a,r),(b,r))` with disjoint circles matched everything).
  Fixed with the rest: it is the same convex code and the corpus probe found it.
- F19 fix is in Collection (skip empty intersections when writing, `empty()` true for only-empty
  intersections), not in the parser: the parser already reads `GEOMETRYCOLLECTION EMPTY`.
- F22 is the order of circles in an unsimplified convex; the engine simplifies (getGeometry), so only a
  directly built convex showed it. Fixed in the convex (`smallest()` by arcangle, sign kept by `add(Convex)`),
  not by sorting on add: that would change the order circles are written in.
- F20 fixed by the htm repin (D129); the Blocked on entry is resolved.
- D130 map form: nested lists, one convex per inner list (as polygon rings); a flat list is one convex.
