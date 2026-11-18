# coverage-areas: journal

State: waiting on jobs 261118-112139-f34646 261118-112141-c954ca 261118-112149-f9ba50
Quest: .campaign/xapiand/quests/coverage-areas.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Committed: harness/coverage.sh lists uncovered lines per file (coverage-uncovered.txt); harness/coverage_mutants.sh; tests/test_coverage_{geospatial,y2j,aggregations,reader_registry}.cc (all pass locally, .scratch/build). Master merged (test-glob landed); the branch's CMakeLists.txt now equals master's (git diff master -- CMakeLists.txt is empty).

Jobs: d4c5ad = A1 "before" (ctest + corpora + E2E coverage at 0ec136187, no new tests); 731f0f geo-km, f2de7d y2j-hex, 6b1e21 agg-depth = --fails-without mutants at ef6548565 (each must exit nonzero = killed).

A2 evidence: run `.scratch/build/bin/xapiand_test_coverage_reader_registry` output (idle 120 B, first reader 440 B, 194 readers 15,800 B per open index; the old fixed registry was 12,416 B for 194 records) as a job once the glob is in.

## Findings (src bugs seen while testing; outside this quest's file set, for the lead)

- F1 src/geospatial/ewkt.cc:314: `THROW(InvalidArgument, "Expected ' ' after of longitude [{}]")` has a `{}` and no argument, so `POINT()`, `POINT(1)`, `MULTIPOINT(1 2 3 4)` throw std::format_error (a 500) instead of an EWKTError (a 400). Pinned as std::exception in test_inputs_that_surface_a_format_error.
- F2 src/geospatial/convex.cc:373 `Convex::getTrixels` dereferences `circles.begin()` of an empty convex (segfault); a convex whose circles do not intersect simplifies to empty.
- F3 src/aggregations/aggregations.cc: `_.fhh(kind)` is not a membership test: `{"_aggs":{"x":{"_bogus":{"_field":"age"}}}}` is accepted (it hashes onto a served kind), so the "Aggregation type ... is not valid" error is reachable only by hash luck.
- F4 src/geospatial/geospatial.cc make_multipolygon: the ARRAY case calls get_data(o) (which reads o as a map), so the nested-array `_latitude`/`_longitude` form is unreachable.
- F6 `{"_polygon":{"_latitude":[1,2,3],"_longitude":[1,2,3],"_altitude":[1]}}` throws std::out_of_range (vector) from make_polygon, not a GeoSpatialError.
- F5 EWKT `POINT(200 2)` is accepted (the map and array forms range-check, the EWKT form does not).

## Next

0. Jobs at 674fbdc5a: f34646 (ctest -N registration + ctest -R test_coverage + A2 registry bytes), c954ca (suite, A3), f9ba50 (rerun of d4c5ad: coverage at head, A1 after). Mutants 731f0f, f2de7d, 6b1e21 passed (killed) at ef6548565, same tests. Then step 1 below, docs/dev/coverage.md, Evidence table.

1. When d4c5ad is done: read `coverage-uncovered.txt` in its out; target by file: lua (needs ~3 lines, ~5 branches), decoders (field_parser.cc, serialize.cc, strata document/pattern/codec), then server and database error paths; one mutant per area in harness/coverage_mutants.sh and a --fails-without job.
2. Job: ctest -R test_coverage (plain pass) and the A2 reader-registry output.
3. After-coverage job (same command as d4c5ad) at the final commit; docs/dev/coverage.md (before/after table, what is open, reader registry bytes); Evidence table.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
