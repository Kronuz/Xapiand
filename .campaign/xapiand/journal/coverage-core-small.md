# coverage-core-small: journal

State: waiting on jobs 261119-055652-fae73e (suite), 261119-055658-c4ce88 (query_dsl at 344945a35), 261119-055700-02118e (query_dsl at the CI-failing 4339206eb, must fail)
Quest: .campaign/xapiand/quests/coverage-core-small.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

CI at 4339206eb failed test_coverage_core_query_dsl on all three platforms. Cause 1 (271): stop words are read from <data path>/stopwords, default the install prefix; this Mac has /usr/local/share/xapiand/stopwords, CI has none (only a one-time L_WARNING_ONCE "No stop words found"). Test now writes its own en.txt and preloads it (as test_control_plane.cc does). Cause 2 (428, FreeBSD only): `create_exp_op_dsl`'s `{{ x }}` gives `{"_not":[[x]]}` on macOS and Linux, `{"_not":[x]}` on FreeBSD; test unwraps lists of one. Product finding dsl-not-wrap, for a hunt row (src/query_dsl.cc is not in this quest's file set).

Four tests written (tests/test_coverage_core_{cast,datetime,endpoint,query_dsl}.cc, helper tests/test_coverage_core.h); none compiled yet. Cast probe job running; then one job for the other three with BUILD shared (build-cov-core).

## Next

1. Read c4ce88 (must pass, numbers as f37689), 02118e (must fail at 271), fae73e (suite); fill the Evidence rows with their ids.
2. Lead pushes and watches CI; mark ready when green.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | f37689 (REPEAT=3, T13 target 90 lines / 80 branches): cast.cc 336/336 lines (100.0%), 301/302 branches (99.7%); datetime_msgpack.cc 446/485 (92.0%), 253/282 (89.7%); endpoint.cc 285/292 (97.6%), 111/128 (86.7%); query_dsl.cc 1201/1311 (91.6%), 581/702 (82.8%). All met. Rerun at 344945a35: c4ce88 |
| A2 | Every check compares values (SEE, CHECK_THROWS_WITH); findings are pinned with their current values. The stop-word check fails without the fix: 02118e (4339206eb, no data path) must exit nonzero; c4ce88 passes |
| A3 | suite fae73e; CI after the lead's push |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- The stop-word test installs its own list rather than reading the source tree's: XAPIAND_SOURCE_DIR is a per-target definition in CMakeLists.txt (not in the file set), and a private list makes the expected query exact.
- "NOT a" is checked on the operand after unwrapping lists of one, not by shape: the shape is toolchain-dependent (dsl-not-wrap), the operand is not. Not an #ifdef.

## Findings so far (pinned in tests, from reading the source; confirm against the job output)

- cast-words: `Cast::get_field_type` has no row for `_date` or `_multiconvex`; `Cast::cast(map)` throws for `_multiconvex` and `_lua`.
- datetime-map-range: an object's hour/min/sec/fsec are not range-checked (`{"_hour": 99}` parses); a time string's are.
- datetime-zone-order: a zone offset in `_time` moves the date parsed so far, so `_time` before `_date` loses the day carry.
- acc-zero: `n._0` reaches `modulus(value, 0)` -> std::invalid_argument (500), not a QueryDslError.
- sort-boolean-key: `get_sorter` sorts a boolean reference value with `add_date` (DateKey); `BoolKey`/`add_bool` exist unused.
- dsl-scale-weight: `_scale_weight`, `_edit_distance` never build: the array form is refused by the engine, the object form is "Invalid operator". `process()`'s "Unexpected array" is unreachable.
- dsl-not-wrap: `make_dsl_query("NOT a")` is `{"_not":[[x]]}` (macOS, Linux CI) or `{"_not":[x]}` (FreeBSD CI): `object[op] = {{ stack_msgpack.back() }}` in `QueryDSL::create_exp_op_dsl` (src/query_dsl.cc:1321) is read differently per toolchain; the engine flattens both. Fix: build the list explicitly with push_back.
- endpoint-host-port: `xapiand://host:port/idx` drops the host (node_name empty).

## Job 86a323 (exit 1) readout

- cast: aborted, uncaught CastError: the test used `_geo_collection`; the words are `_geometrycollection`, `_geometryintersection` (fixed). No cast numbers yet.
- datetime 446/485 lines, 253/282 branches: only 4 wrong quote styles ('x' not "x"); datetime-map-range and datetime-zone-order checks PASSED, so confirmed.
- endpoint 278/292 lines, 105/128 branches: UUID text is the serialized form (a2d6e3b4-0000-1000 -> 0000000a-2d6e-63b4, 0000..-1000 -> -6000: version-1 time fields reordered, apparently by design, not a bug); partition expectations now use the normalized text, encoded case mirrors the early-return grouping. endpoint-host-port check PASSED: confirmed.
- query_dsl: compile error (Serialize::serialize takes a non-const tm_t&), fixed. acc-zero, sort-boolean-key, dsl-scale-weight still unknown.
- cast-words still unknown (cast never ran).

## Job f2d437 (exit 134) readout

- cast: 336/336 lines, 301/302 branches, 214 checks, 3 of 3 runs pass. cast-words stays pinned (serialize-validation fixes it).
- datetime: 446/485 lines, 253/282 branches (134 checks pass). endpoint: 285/292 lines, 111/128 branches (103 checks pass).
- query_dsl aborted: uncaught CastError "Type MAP cannot be cast to integer" from get_sorter({"n": {"_integer": "100"}}). NEW finding sort-cast-word: the sorter passes the whole map (`value = &o`, not `&val`) to Cast::cast, so every cast word throws CastError for integer, datetime and text fields (geo works: the cast returns the map). Not a process abort in product code: the test did not expect the throw. Pinned as CHECK_THROW.
- query_dsl wrong expectations fixed: phrase/near carry window 11; nest(20) keeps 20 pairs of parentheses; "k._10" with "a" throws integer type error; "NOT a" is `_not: [[...]]` (pinned, dsl-not-nested); "a b" is not refused (value printed by the next run, then pinned).
- Suspects: acc-zero, dsl-scale-weight, sort-boolean-key: unknown until query_dsl runs.

## Job 36eeba (exit 1) readout

- query_dsl ran: 1201/1311 lines (91.6%), branches 582/702 (82.9%); 275 checks, 11 failed, all wrong expectations in test_sorters: a datetime and a text cast word WORK in a sort (only integer throws: sort-cast-word narrowed); phonetic metrics store the encoded code, not "smith"; metric key names are Levenshtein, Jaro_Winkler, Sorensen_Dice, Jaccard, LCSubstr, LCSubsequence, Jaro (no "StringKey"); "a b" is `_or` of both terms (pinned).
- Confirmed (their pinned checks passed): acc-zero, dsl-scale-weight, sort-boolean-key. cast 336/336, 301/302; datetime 446/485, 253/282; endpoint 285/292, 111/128.
