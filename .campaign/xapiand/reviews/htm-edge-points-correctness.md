# htm-edge-points review: correctness

Commit: htm:d529b499d7796aeec71907b4fe432522105adfb0
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The code is right. The closed test plus nearest-child descent is deterministic, agrees with the
IEEE reference everywhere I probed, and every circle from 1 m to 100 km covers the new id of its
center and of points inside it, once a pre-existing NaN in `HTM::intersectConstraints` is taken
out (F3). The one blocker is the CHANGELOG's reindex guidance: its "at most 7.74 m" bound is a
sample figure. Points without a zero coordinate changed by up to 40.8 m on random input and up to
2,120 km near deeper vertices.

## Evidence I relied on

- 261118-235818-798d95 (htm_diff 1000000 at d529b49, exit 0): 0 failures, 4,705 of 1,125,000
  random points in slivers. 261118-235820-219166 (Debug ctest at d529b49) and
  261118-235821-6b7b9b (Release suite). CI 44483693645 at d529b49 is green on freebsd-x86_64,
  linux asan/ubsan, tsan, arm64-gcc, x86_64 clang and gcc, and macos arm64 and x86_64. That run
  includes `htm_fma` and `htm_rounded`, so the 48 re-id'd probes still detect a fusing build.
- A1's fails-without jobs (261118-235815-bffda3, 261118-235817-c86c33) show `htm_edge` fails on
  the closed-only rule and on the open rule. I did not rerun any of these.

## What I checked myself

Scripts are in `.scratch/xapiand/review-htm-edge-points-correctness/`: `probe.cc` with
`run_probe.sh` (Debug), `cover_cmp.cc` with `run_cover_cmp.sh` (head vs cadec57), and
`cover_bench.cc` with `run_bench.sh`.

1. **Determinism (Q1).** Probe 261119-002701-9e5dae, Debug with asserts on:
   1. `getId`, `ieee::getId` and a separate replica of the closed descent agree on 1,000,000
      random points.
   2. Nearest-child ties: 0, across the 4,151 points that took a nearest level.
   3. The zero vector, ±0 in every slot, NaN, inf and both poles give stable ids equal to
      ieee's, and no assert fires.
   4. `nearest()` uses the same products as `ieee::child<true>`, and child 3's products are exact
      negations of t0..t2 (htm.cc:672-686), so the shortcut and the reference cannot diverge.
2. **A2, made explicit (Q3).** Same probe, 1,000,000 random points:
   1. 4,143 ids change, first at levels 17 to 25.
   2. At the first differing level, the old child holds the point in 0 cases, so the old trixel
      always excludes it.
   3. The new child is a nearest choice in all 4,143 cases. Its final trixel lies at most 2.07 mm
      from the point (long-double geometry). That is inside double's own noise at level 25:
      unchanged ids reach 6.56 mm.
   4. htm_diff's test ("a changed id implies a nearest level") is a real check. It implies the
      old trixel excludes the point; it does not count.
3. **Cover (Q2).** Circles of radius 1, 10, 100 m and 1, 10, 100 km, using edge_test's cover,
   which mirrors Xapiand `Circle::verifyTrixel`/`lookupTrixel`. Each circle checks its center
   and 2 to 4 points inside it, at up to 0.95 r. Center sets:
   1. 50,000 random points.
   2. 4,151 sliver points.
   3. 32,912 edge points: dyadic latitudes and longitudes on all root edges in degrees, exact
      x=0, y=0 and z=0 vectors, and both poles at 72 longitudes.
   4. 48,000 exact and perturbed deeper vertices.

   With `intersectConstraints` clamped, new ids miss 0 of 3,080,946 checks. Old ids miss 292,378
   of them (edge points 122,910). Unclamped, the misses are all at 1 to 10 m and all come from F3.
4. **Pre-existing cause (F3).** cover_cmp 261119-001929-9b271d, Release, head vs cadec57, 50,000
   random centers per radius. Centers missed: 47 at 1 m and 8 at 3 m (cadec57: 58 and 9), and 0
   from 10 m to 100 km. Each diagnosed miss has the center inside the trixel and 0 m from its
   bounding circle's center, yet `intersectConstraints` returns 0.
5. **Reindex classes (Q4).** probe 261119-002701-9e5dae:
   1. Edge set: 20,485 of 32,912 change, every one with a zero coordinate, up to 5,202 km away.
      Points in degrees at lon 90, 180 or 270, and poles at lon ≠ 0, have no exact zero, and none
      of them changed.
   2. 200,000 decimal lat/lon (0 to 3 decimals): 1,186 change. Of those, 727 have no zero
      coordinate, and their old trixel lies up to 20.8 m away.
   3. Exact or 1e-15-perturbed deeper vertices: 42,965 of 48,000 change with no zero coordinate,
      up to 2,120 km away.
6. **Cost (Q5).** Job 261119-002604-50bf6d, strict, Release, three interleaved runs beside two
   light holds:
   1. getId median: head 815, 820, 888 ns against cadec57 797, 793, 814 ns, about 3% slower.
   2. Cover: 1 m 16.9k vs 17.0k ns, 100 m 18.4-18.9k vs 18.7-18.8k ns, 10 km 13.6-14.8k vs
      13.3-14.6k ns, within noise.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The CHANGELOG 2026-11-18 "Changed" bullet says other changed ids "moved by at most 7.74 m" and change only "at levels 20 to 25". It offers reindexing "every stored point with a zero coordinate" as the partial option. Both figures are sample-specific. Another 1M random sample changes from level 17 with old trixels up to 40.8 m away. Decimal user coordinates change by up to 20.8 m with no zero coordinate. Points at or within 1e-15 of a deeper trixel vertex (no zero coordinate) had old ids up to 2,120 km away. A partial reindex of zero-coordinate points therefore leaves points that a 1 to 40 m circle misses, and some that any circle misses. ARCHITECTURE.md ("0.42% ... from level 20 down") and the quest's Decision repeat the sample figures. | probe 261119-002701-9e5dae (reindex lines; A2 distances), CHANGELOG.md:20-23, 37-46; ARCHITECTURE.md:51-54 | Restate the CHANGELOG without the bound: zero-coordinate points move up to ~6,000 km; points within rounding of a deeper vertex up to thousands of km; 0.4% of other points by meters (40.8 m seen). Recommend reindexing all stored geo points, and keep "zero coordinate" only as the class that certainly moved far. Give ARCHITECTURE's level as "from level ~17 down" or drop it. Doc only; the lead confirms the text. |
| F2 | no | gap | A2 as written ("its new one holds it") is false by construction: every changed random id is a nearest choice whose trixel lies outside the point, by up to 2.07 mm, within double's level-25 noise (6.56 mm on unchanged ids). htm_diff proves the first half and says nothing about distance. | probe 261119-002701-9e5dae (A2 lines) | Lead restates A2 as "the old trixel excludes the point; the new is the child it is least outside of (≤ a few mm)". Optionally htm_diff could assert a distance bound. |
| F3 | no | defect (pre-existing, outside the quest) | `HTM::intersectConstraints` (htm.cc:431) takes `acos` of a dot product that rounds above 1 for nearly coincident centers, gets NaN, and says the circles are disjoint. Xapiand's `Circle::verifyTrixel` (src/geospatial/circle.cc:70) tests it before the edge and center checks. Result: 1 m circles miss their own center's id for 0.09% of random points (cadec57: 0.12%), 3 m circles for 0.02%, and deep-vertex centers at 10 m. Clamping removes every miss. | cover_cmp 261119-001929-9b271d; probe 261119-002701-9e5dae (original vs clamped) | Clamp the dot to [-1, 1]. Test: 1 m and 3 m circles around 50,000 random centers cover the center's id. It fits this quest (same file, same repin) or a hunt row for geo-correctness; lead's call. |
| F4 | no | nit | The CHANGELOG says getId "runs at the same speed". Strict interleaved runs show about 3% slower (median 820 vs 797 ns), about 23 ns per indexed point; cover is unchanged. | job 261119-002604-50bf6d | Say "within ~3%", or rerun exclusive if the claim stays. |

## Not run

- Probes on a fusing build (arm64, or x86 with -mfma). I relied on CI's htm_fma/htm_rounded and
  the macOS/Linux arm64 jobs for id equality there. Cover-vs-getId agreement on a fusing build is
  argued (both sides within nm of each other, circles ≥ 0.1 m), not probed.
- An exclusive-level benchmark: the exclusive job queued behind two exclusive holds, so I
  cancelled it and ran strict.
- Xapiand's real `Circle::getRanges` end to end. I tested the copied cover (same verify/lookup
  shape); the `partials=false` path, which drops max-level partial trixels, is outside this
  quest.
- Polygon and convex covers, which also call `insideVertex_Trixel`, now closed.

## Re-check

Commit: htm:0c09a0283a507b2cf08975a7c8572d95ece60b68
Verdict: APPROVE
Date: 2026-11-19

One commit on d529b49. I checked each claimed fix against its diff and the job records.

1. **F1, fixed.** The 7.74 m, "levels 20 to 25", "from level 20" and 6,090 km figures are gone
   from CHANGELOG.md and ARCHITECTURE.md. The guidance is now "Reindex all stored geo points".
   It names points at or near a deeper vertex as moving up to thousands of km and other points as
   moving meters ("tens of meters seen"), which matches probe 261119-002701-9e5dae.
2. **F3 (now A5), fixed.** htm.cc:434 clamps the dot product. The clamp changes nothing but the
   NaN case:
   1. `std::clamp(x, -1, 1)` is x for every x in [-1, 1] and leaves a NaN dot (from a NaN
      input) NaN, so the result changes only where the dot product lies outside [-1, 1]. Before
      the fix, every such call returned false through NaN.
   2. Above 1, it now returns `0 < arcangle sum`, which is true. Below -1 (antipodal centers), it
      returns `pi < arcangle sum`, which is the true geometric answer.
   3. Every caller treats true as "maybe intersects" and false as OUTSIDE: edge_test's verify,
      Xapiand `Circle::verifyTrixel` (circle.cc:70), `Convex::intersectCircles`/`verifyTrixel`
      (convex.cc:80, 101) and `Polygon` (polygon.cc:380). So the change can only add trixels to a
      cover that the NaN wrongly dropped. It never removes one.
   4. Jobs: 261119-003445-380f20 passes at 0c09a02 (htm_edge, 100,000 1 m/3 m circles, 0
      misses), and the unclamped mutant 261119-003446-097d2d fails with 49 misses. That count is
      consistent with my 47+8 on a different sample.
3. **F2, addressed.** htm_diff now fails when a changed id's new trixel is more than 5 cm from the
   point. Job 261119-003448-692923: 4,700 changed ids, at most 6.68 mm, 0 failures. That agrees
   with my 2.07 mm on another sample and with double's level-25 noise. The criterion's wording is
   still the lead's to restate.
4. **F4, fixed.** The CHANGELOG says "about 3% slower (median 820 against 797 ns)".
5. **Suites.** Debug ctest 261119-003449-9282f5 and the check suite 261119-003450-7f1ba0 pass at
   0c09a02. CI run 44488771810 at 0c09a02 was still in progress when I looked; A4 needs it green
   before landing.

No blocking findings remain. One non-blocking nit: the CHANGELOG's "Every changed id's old
trixel did not hold its point" is proven for random points (htm_diff, and 0 of 4,143 in my
probe). For points exactly on a deeper inner edge or at a midpoint it is argued, not tested.
