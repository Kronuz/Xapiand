# coverage-areas: journal

State: waiting on jobs 261118-195812-46640b (geospatial) 261118-174725-3659a2 (serialize); A4 waits on logger-exit-lifetime
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

## Findings from the field parser fork (src/field_parser.cc; tests/test_coverage_fieldparser.cc at 48bb471c5, job 38046f queued, no numbers yet)

- F7 `['abc` hangs the parser forever (`case '\0': break;` in SQUARE_BRACKET_QUOTE never advances); its test is written but not run. F8 a space in a field name is not counted (`a b:c` names `a `). F9 the 1,024-byte field-name cap never applies. F10 an escaped quote closes the string. F11 a double-quoted name leaves the value flagged double-quoted. F12 `lens_*_quote` is not reset at the colon, so a later getter over-reads. F13 `parse(100)` clamps to 10 but arrays of size 10 are indexed at level 10 (out-of-bounds write). F14 `skip_quote` is unused. Unreachable: `case SQUARE_BRACKET_END`, the `>= 1024` check in INIT.

## Next

11-18 late. Field parser job a12f7b: field_parser.cc 450/474 lines (94.9 percent), 243/268 branches (90.7 percent); the 24 uncovered lines are the unreachable ones (the 1,024-byte cap 114-115, SQUARE_BRACKET_END 501-519), the hang case (432-433, F7) and line 202; the decoders area needs 98 percent lines and 95 percent branches over six files, so serialize.cc's number decides it. Geospatial job f210d3 (draft 1): lines 2658/2786 = 95.4 percent over the 11 files (above T13's 90), branches about 90 percent (T13 80), but 12 checks failed: the tests had guessed wrong about large circles, lenses and rings and one error type. Fixed at the next commit by pinning today's behavior as findings F18 (MULTIPOINT(1 2, 3) raises a non-EWKTError), F19 (a collection holding an empty intersection does not write EMPTY), F20 (a lens of two 9,000 km circles misses the center at 8 of 9 longitudes), F21 (a 15,011 km circle does not cover its own center), F22 (a point inside a ring's circles is not covered); rerun 261118-195812-46640b. Serialize 3659a2 is still queued. Remaining: serialize numbers and its gap, coverage and suite reruns at the final head, A4 after the logger quest (pin bump, 20 runs), Evidence rows, docs/dev/coverage.md.

Serialize fork (4b690309d, ~890 lines, 20 tests, never compiled yet; job 3659a2): findings F15 `object`, `guess_type` and `guess_serialize` refuse the `_date`, `_multiconvex` and `_lua` casts; F16 `guess_serialize` of `{"_time":"10:30:00"}` and `{"_timedelta":"+01:30:00"}` throws; F17 `get_field_type("boolean")` returns datetime.

Pre-check 50b1a8: with the logger's OnceFilter never destroyed (patched copy, job checkout only) the aggregations test fails 0 of 20 (baseline 20 of 20): the cause is confirmed; not A4 evidence. harness/coverage_one.sh reported 0/0 (relative source paths): fixed at 09806551e; jobs 38046f and 13cfad ran the old script. Field parser commit 48bb471c5 passed its 192 checks (job 38046f exit 0). Geospatial commit b35430350 (draft 1), serialize commit 4b690309d.

Lead 11-18: option (a). Quest logger-exit-lifetime (in ../logger) fixes the OnceFilter lifetime with a test; coverage-areas lands after it; CMakeLists.txt is in my file set for the pin bump. Then: bump the logger pin (CMakeLists.txt:563) to its commit, rerun the aggregations test 20 times under one libc++ (0 of 20 is A4's evidence; 50b1a8 is only a pre-check of the patch), rerun coverage and the suite. Forks: fieldparser done (48bb471c5, job 38046f), serialize and geospatial running.

00000000000. Lead split (b929d23d2): this quest keeps A1 (aggregations, lua, geospatial, decoders), A4 (the exit-time mutex abort, fixed in the product with a check that fails without), A2 (done), A3. Forks (own files, commit only them): tests/test_coverage_serialize.cc (src/serialize.cc), tests/test_coverage_fieldparser.cc (src/field_parser.cc), tests/test_coverage_geospatial.cc (the ~200 geospatial lines), each iterating with harness/coverage_one.sh jobs (e2e1fa66b). Me: A4 from diag8 b09a49's backtrace; then the coverage job and suite at the new head, Evidence rows, docs/dev/coverage.md.

0000000000. 11-18 evening. Proof 7684ae: one libc++ now; geospatial 0/20 failures; aggregations 20/20 failures (deterministic: OK then system_error mutex lock failed at exit, a mutex locked after its static died; LLVM's libc++ returns EINVAL where Apple's tolerates it). Diag8 b09a49 (at bf83947ba): lldb break at __throw_system_error / abort for the backtrace; fix in the frame's file (src/node.cc, node.h are in the file set; tell the lead if it is elsewhere) with a test that fails without it. Suite 69ce14 failed only test_coverage_lua (a function value in a table is stored as nil, not absent): fixed in bf83947ba. Coverage 277715 (at 4e69dda8b): met: collectors, engine strata, engine seam, aggregations 95.9/92.3, lua 92.9/82.3; geospatial 83.0/81.7 (short on lines: ewkt.cc 145, geospatial.cc 143, polygon.cc 86, convex.cc 60 lines uncovered; ~200 lines to 90%); below: decoders 65.6/62.3 (serialize.cc 618 lines, field_parser.cc 232; the strata decoders are at 96-100%), server 76.8/49.4 (search_views.cc 525 lines, 792 branches), database 61.1/51.4 (schema.cc 3969 lines, handler 702, wal 315, schemas_lru 286, shard 371), multivalue 70.4/69.8 (generate_terms 228, keymaker 144, range 103), core 63.5/49.1 (query_dsl 605, main 366, datetime_msgpack 298, manager 225, cast 184); all of src 68.8/58.8.

### Proposed split (the lead decides)

This quest closes: aggregations, lua (met), then geospatial lines (~200 lines in ewkt, geospatial, polygon, convex, one more pass), and decoders (serialize.cc and field_parser.cc, ~850 lines, pure functions: one test file each, mutants easy).
Follow-up quests, gaps named: (1) multivalue (generate_terms, keymaker, range: ~480 lines to 90%); (2) core-small (cast.cc, datetime_msgpack.cc, endpoint.cc, query_dsl.cc: ~1,200 lines, all unit-testable); (3) server (search_views.cc: 525 lines and 792 branches, needs a node or E2E driver, branches are the gap); (4) database (schema.cc alone is 3,969 uncovered lines; wal, handler, shard, schemas_lru: a program of its own, E2E corpora more than unit tests); (5) core main.cc/manager.cc (process entry, CLI and startup: E2E/harness, ~590 lines). Areas 3, 4 and 5 will not reach their floors from unit tests; the quest wording should say E2E-driven, or the floors for them should be re-set as a ratchet.

000000000. Cause of both aborts (diag7 271f1d): harness/coverage.sh's instrumented binaries linked two C++ runtimes (CMake's find_library(c++) took the SDK's libc++, -L added LLVM 22's), so an exception thrown in one was missed by a catch in the other (geospatial CIRCLE(1 2)) and a mutex destroyed by one was locked by the other (aggregations at exit). Fixed in 4e69dda8b: coverage.sh names LLVM's libc++/libc++abi (-DLIBCPP_LIBRARIES, -DLIBCPPABI_LIBRARIES) and one_runtime() fails the build when a binary links two; the geospatial test is back to TEST_MAIN. No product fix: the normal build was never affected. Jobs at 4e69dda8b: proof 7684ae (geospatial and aggregations 20 runs each, 0 failures expected), coverage 277715 (A1 after), suite 69ce14 (A3). A2 8aa284 done. Then: lua/other areas still below their floors (see the coverage table), docs/dev/coverage.md, Evidence rows.

00000000. diag6 a62f41: geospatial last line RUN test_ewkt_errors then terminate (InvalidArgument); aggregations aborts 1/20 baseline, 14/20 with _nodes_mtx/_nodes before _local_node (do not land that reorder). Hypothesis to test: the instrumented binaries load two libc++ copies (LLVM 22's via rpath and the system's via a dependency or libSystem), so exception types and std::mutex objects cross ABIs; both aborts only occur under harness/coverage.sh's LLVM toolchain. diag7 271f1d (at ca1cfe7c3): prints the failing EWKT input, otool -L / rpath, and DYLD_PRINT_LIBRARIES. If confirmed, the fix is link flags in harness/coverage.sh (not product); otherwise bisect the noexcept frame.

0000000. diag5 725348: the preloaded terminate handler never ran; aggregations passed 3/3 (intermittent), geospatial aborted 3/3 with no FAIL and no OK. diag6 a62f41 (at 49e53d251): geospatial prints MAIN and RUN <test> to stderr (no MAIN = static init; last RUN = the test whose noexcept frame throws); aggregations run 20 times, then 20 more with Node::_nodes_mtx/_nodes defined before _local_node in node.cc (experiment on the job's checkout only). Fix node.cc only if the rate goes to 0 of 20; geospatial: the test it names, bisect with the same prints.

000000. diag4 4ba278: geospatial still aborts with every test guarded (catch std::exception), so the InvalidArgument is thrown outside the tests (static init or exit), not in one; aggregations passed under lldb (no abort), so it is not deterministic there. Lead widened the file set to src/node.cc, src/node.h. diag5 725348 preloads a terminate handler (.scratch/tb.cc via DYLD_INSERT_LIBRARIES) that prints backtrace + what() for 3 runs of each; the stack at terminate is the throw site. Then fix (node.cc statics via function-local statics if the mutex is proven) + a test failing without, rerun coverage and suite.

00000. diag3 6b0c44: geospatial aborted with no FAIL text, so not the reprint (the abort is in an earlier test); aggregations: static std::mutex Node::_nodes_mtx (src/node.cc:112) is defined after Node::_local_node (:109), so it is destroyed first; suspect a lock at static destruction (unproven, the -O breakpoint filter did not filter). diag4 4ba278 (at 0b7192e42): geospatial tests now run guarded (names the test that throws); lldb breaks at __throw_system_error. Product fix only if the bt shows a src/ lock at exit; it would be outside the file set (src/node.cc), so ask the lead first.

0000. diag2 e47048 showed only the expected (caught) throws. Suspect for geospatial: test_ewkt_forms re-parses geometry->toEWKT() outside a try (an InvalidArgument from a reprint the LLVM 22 libc++ formats differently); the test now catches and prints it. diag3 6b6c44... = 261118-150347-6b0c44 prints that text and the backtrace of the aggregations std::system_error at exit (break -E c++ -O std::__1::system_error). Then fix, rerun coverage (campaign job rerun f9ba50-style) and the suite at the new head.

000. 11-18 later: A2 job 8aa284 done (registry: idle 120 B, first reader 440 B, 194 readers 15,800 B per open index; 128 readers 10,360 B); df194f ctest -N done (the 4 coverage tests register). Diag 477ffe reproduced both aborts in the instrumented build but its lldb printed no backtrace; diag2 e47048 breaks on every C++ throw and prints bts (aggregations: a mutex at exit, suspect static destruction; geospatial: uncaught InvalidArgument, only ewkt.cc and multivalue throw it, CHECK_THROW swallows all so it is an unprotected call). Fix cause (product bug => test that fails without), rerun coverage + suite, record A1/A2/A3 rows.

00. Lead 11-18: suite c954ca passed at 674fbdc5a; f34646 hit its deadline (invalid), split into df194f (ctest -N) and 8aa284 (A2 registry bytes, after df194f); f9ba50 (coverage at head) exited 1 because ctest failed in the instrumented build: test_coverage_aggregations aborts after "OK" (std::system_error: mutex lock failed, at exit) and test_coverage_geospatial aborts (uncaught InvalidArgument), both only under harness/coverage.sh's build (LLVM 22 libc++ and -fprofile-instr-generate); an aborted test writes no profile, so both areas show no gain (geospatial 28.6%, aggregations 85.9%). harness/coverage.sh's exit 1 is that ctest failure (STRICT is unset), not the floors. The quest's A1 says each area reaches T13's target, so the floors are targets. Diag job 477ffe builds both under the coverage flags and runs lldb bt; read its log, fix the tests (file set tests/test_coverage_*), rerun the coverage job. Then lua branches (79.6%, +2 needed: tests/test_coverage_lua.cc added), then the rest by distance to floor from coverage-uncovered.txt.

0. Jobs at 674fbdc5a: f34646 (ctest -N registration + ctest -R test_coverage + A2 registry bytes), c954ca (suite, A3), f9ba50 (rerun of d4c5ad: coverage at head, A1 after). Mutants 731f0f, f2de7d, 6b1e21 passed (killed) at ef6548565, same tests. Then step 1 below, docs/dev/coverage.md, Evidence table.

1. When d4c5ad is done: read `coverage-uncovered.txt` in its out; target by file: lua (needs ~3 lines, ~5 branches), decoders (field_parser.cc, serialize.cc, strata document/pattern/codec), then server and database error paths; one mutant per area in harness/coverage_mutants.sh and a --fails-without job.
2. Job: ctest -R test_coverage (plain pass) and the A2 reader-registry output.
3. After-coverage job (same command as d4c5ad) at the final commit; docs/dev/coverage.md (before/after table, what is open, reader registry bytes); Evidence table.

## Blocked on

A4's frame is not in Xapiand: diag8 b09a49 shows the abort is on the log thread (`Xapiand:LOG`), LogWorker::close() draining at exit into Logging::emit, which takes `static OnceFilter filter`'s mutex (Kronuz/logger logger.cc:1765, pin 6e7ab9e). The function-local static is constructed after the logger's own state, so it is destroyed first, and the drain locks its destroyed std::mutex (LLVM's libc++ returns EINVAL; Apple's tolerated it, so Xapiand's normal build never showed it). The fix is in the logger repository (make the filter a member of the logger or never-destroyed: `static OnceFilter& filter = *new OnceFilter;`) plus a pin bump in CMakeLists.txt (line 563), both outside this quest's file set and repository. Proof job 50b1a8 runs the aggregations test 20 times against a patched copy (the job's checkout only). Options for the lead: (a) a logger quest/commit, then pin bump here, with a logger test that fails without; (b) widen this quest to CMakeLists.txt and keep the logger change as a separate landing. Recommendation (a). Until then the aggregations test (20 of 20 abort under one libc++, instrumented build only) is A4's check, and its coverage lines still count only if the profile is written: to keep the coverage run honest meanwhile, the instrumented tests could close the logger before exit.

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
