# geo-correctness review: correctness

Commit: Xapiand:d2ba9e9634bc (its code is identical to Xapiand:7630da9f1 and Xapiand:fdc169e74, where the A1, corpus
and suite jobs ran; the commits after them change only the journal)
Verdict: APPROVE
Date: 2026-11-19

The fixes are right across every input class I probed. An exact-membership oracle over 1,101 geometries finds no
point inside a shape that a `partials=true` cover misses (267 million (point, setting) pairs). The geometries
include negative circles, rings in both orders, the antimeridian, the poles, root vertices, holes, every corpus
query the repin moved, and 300 random convexes of 1 to 3 circles from 1 km to 20,000 km, each tested as built,
merged and parsed. An independent recount of the corpus diff reproduces the contributor's classification, and
every finding's test fails over the parent's `src/geospatial` with the head's htm pin. Nothing blocks. The
remaining whole-cell over-coverage is pre-existing (R2). The changelog and reindex note are for the lead to add (R1).

## Evidence I relied on

- A1: 261119-041250-4fe1c5 (geo_correctness and coverage_geospatial pass at 7630da9f1). Fails-without
  261119-004729-f41c38 and 261119-004732-82117a (exit 8, at the scratch commit with the parent's `src/geospatial`
  and the old htm pin). I read their logs. Every F18, F19, F21, F22 and F23 check fails. f41c38 segfaults in
  `test_circles_that_never_meet_cover_nothing` (an empty convex's `begin()`), so the D130 test never ran there;
  my R4 probe settles that.
- A2: 261119-040317-8d9156 (corpus table, exit 0, at fdc169e74). Per-point lists 261119-024749-524b1b (after) and
  261119-024740-da1dc6 (before). Xapiand:88191d384 differs from Xapiand:f686aa12a only in the htm pin in
  CMakeLists.txt.
- A3: suite 261119-041251-84206c (54 of 54, at 7630da9f1) and 261119-040319-57eeba. On GitHub, CI run 44520394807
  at 7630da9f1 passes its unit tests under ASan+UBSan and TSan. Two of its jobs failed in "Install build deps"
  on apt-mirror fetch errors, which is infrastructure: rerun them. The arm64 and FreeBSD runs were still in
  progress when I checked.

## What I checked myself

The scripts are in the worktree at `.scratch/xapiand/review-geo-correctness-correctness/`
(Xapiand-wt/xapiand/geo-correctness), in the review slot `review-geo-correctness`.

1. **Cover oracle, Q1 and Q2** (`test_review_geo_oracle.cc`, `run.sh`, job 261119-044559-b9ea18).
   - Membership comes from the library's own constraint vectors: angle(p, center) <= arcangle for every circle,
     on the WGS84 unit vectors that `Cartesian` builds.
   - The corpus has 60,000 uniform points plus points on the root edges, the poles, the antimeridian and the
     F22 test's own 5,500 km points. Points within 50 m of a boundary are skipped.
   - Each shape is built in its given order, merged with `add(Convex)`, parsed from EWKT (simplified), and, for
     one circle, also as a `Circle`. Settings: error 0.05 and 0.3, partials on and off.
   - Head: **0 false negatives** with partials on. With partials off, false positives remain only on the rings
     at error 0.3 (R2) and on one unsimplified random convex.
   - The same oracle over the parent's `src/geospatial` (head's htm) also shows the F21 hole defect. Whole cells
     covered points inside the 1,000 km and 200 km holes, 381 and 17 of them, up to 913 km in. Head removes it.
     The parent also shows order-dependent and stale-sign ring covers: 13,271, 12,484 and 5,470 false positives
     against head's 6,918 in every order. Head removes those differences too.
2. **Per-fix attribution** (`run_attrib.sh`, job 261119-045245-39dc85). The quest's tests run over the parent's
   `src/geospatial` with the head's htm pin.
   - F18, F19, F21, F22 (the 26 checks at line 257 and the order equalities at lines 270-271), F23, and the
     coverage flips at lines 681, 762, 889 and 902 all fail.
   - The F20 checks pass there, so F20 is the htm repin alone (D129), as the design note says.
   - The oracle at the parent (job 261119-045517-52ab8b) covers all 3,623 ring points with partials on, the
     F22 test's points included. So F22's misses were whole-cell misses (partials off), caused by the coarser
     `begin()` resolution and order dependence. They were not uncovered points (R4).
3. **D130** (`test_review_d130.cc`, job b9ea18).
   - The map-form test passes at head (17 checks).
   - Over the parent's `geospatial.cc` and `geospatial.h` it fails with `GeoSpatialError: Unknown geometry
     _multiconvex`.
   - Malformed map forms (`test_review_multiconvex_bad.cc`) raise `GeoSpatialError` in all 9 cases, never
     another type. The cases cover mixed nesting, scalars and strings where lists belong, size mismatches,
     altitude mismatches, triple nesting and a non-numeric radius.
   - `[]` and `[[]]` are accepted as `MULTICONVEX EMPTY`.
4. **Corpus classification, Q2** (`classify.py` over the 524b1b and da1dc6 logs, using WGS84 unit vectors).
   - The numbers match the contributor's: 24 answers in 10 queries; 23 gained inside (164 to 3,940 km); 1 gained
     outside (`convex cap and negative`, setting 1, 222 km, a partial cell); 0 lost inside; 29 lost outside (498
     to 6,185 km). All of them are on corpus indexes 6000 to 6005 and 6010.
   - Our distances differ by up to 13 km because of the sphere and ellipsoid models. The margins are hundreds of
     km, so that difference does not affect any classification.
5. **htm pin, Q3.**
   - CMakeLists.txt:932 is the only pin, and htm 0c09a02 is on htm `main`.
   - No test hardcodes an HTM id. test_geospatial.cc is structural. test_coverage_serialize.cc:444-447 hashes a
     one-trixel range to 0, which no pin changes. test_geo_corpus.cc's table was regenerated at this pin and
     passes (8d9156).
6. **Flipped pins, Q4.** Every flip at test_coverage_geospatial.cc:681, 762, 844, 889 and 902 asserts the
   geometrically right answer, and each fails at the parent (job 39dc85; 844 fails only with the old htm, job
   82117a). No flip pins today's wrong behavior. Two comments are stale (R3).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | gap | The repin changes stored geo ids, but no Xapiand changelog entry says to reindex. Per the htm CHANGELOG at 0c09a02, root-edge points move up to thousands of km, points near deeper vertices move too, and about 0.4% of other points move by meters. docs/design/geo-correctness.md ("Ids move only for points on an edge") contradicts that. The F18 parse, the F23 empty answer and D130's new `_multiconvex` map form are user-visible and also have no entry. The changelog is outside the quest's file set. | htm `CHANGELOG.md` at 0c09a02; `docs/src/content/docs/changelog.md:112` (the previous htm repin's reindex entry); `docs/design/geo-correctness.md`, the paragraph after the F20 code block | The lead adds an Unreleased entry: reindex geo points, plus the answer changes. Correct the design-note sentence. |
| R2 | no | defect (pre-existing) | With partials off at error 0.3, the ring (5,003 to 6,000 km around the antipode) covers 6,918 of 60,000 points outside it, up to 1,943 km into the hole. This holds in either order and through the parser. The parser path gives the identical 6,918 at the parent. An unsimplified random convex shows the same at error 0.3. The engine always simplifies. | job 261119-044559-b9ea18 (head and parent sections) | Hunt row: whole cells at coarse levels in `Convex::verifyTrixel` case 3 for a negative circle (hole or edge test). |
| R3 | no | nit | Two stale comments. test_coverage_geospatial.cc:455-462 still describes the `std::format_error` finding this quest fixed, and checks only `std::exception` where an `EWKTError` is now raised. test_geo_correctness.cc:157-159 says `HTM::getId` gives root-edge points the middle-of-root id, which is no longer true at this pin. | the files at d2ba9e963 | Tighten line 455's check to `EWKTError` and drop the finding text; update the lens comment. |
| R4 | no | gap (settled here) | The fails-without jobs reverted the htm pin together with the Xapiand fixes, and f41c38 segfaulted before the D130 test ran, so neither attributes per fix. My jobs settle both: each Xapiand fix fails alone, and D130 fails at the parent. F22's parent failures were whole-cell resolution and order dependence; the design note's "missed points inside both circles" holds for partials off only. | jobs 39dc85, b9ea18, 52ab8b | None needed. Optionally say "with whole cells" in the F22 row. |

## Not run

- No sanitizer or other-platform runs: CI covers those, and its arm64 and FreeBSD runs were pending.
- The oracle tests Convex and Circle only. Polygon, Chull, MultiPolygon and Intersection covers were not checked
  against exact membership.
- No diagnosis of R2's mechanism beyond showing it is pre-existing.
- No end-to-end HTTP search through the engine.
