# coverage-core-small: journal

State: waiting on jobs 261119-011017-86a323 (long, all four tests; the 30-minute cold build timed out job bff9f2)
Quest: .campaign/xapiand/quests/coverage-core-small.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Four tests written (tests/test_coverage_core_{cast,datetime,endpoint,query_dsl}.cc, helper tests/test_coverage_core.h); none compiled yet. Cast probe job running; then one job for the other three with BUILD shared (build-cov-core).

## Next

1. Read the probe output, fix expectations (a mismatch is my misreading or a product bug: check against the source), then run all four with harness/coverage_one.sh, REPEAT=20.
2. Full coverage.sh job, per-file numbers, suite job, push, mark ready.

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

## Findings so far (pinned in tests, from reading the source; confirm against the job output)

- cast-words: `Cast::get_field_type` has no row for `_date` or `_multiconvex`; `Cast::cast(map)` throws for `_multiconvex` and `_lua`.
- datetime-map-range: an object's hour/min/sec/fsec are not range-checked (`{"_hour": 99}` parses); a time string's are.
- datetime-zone-order: a zone offset in `_time` moves the date parsed so far, so `_time` before `_date` loses the day carry.
- acc-zero: `n._0` reaches `modulus(value, 0)` -> std::invalid_argument (500), not a QueryDslError.
- sort-boolean-key: `get_sorter` sorts a boolean reference value with `add_date` (DateKey); `BoolKey`/`add_bool` exist unused.
- dsl-scale-weight: `_scale_weight`, `_edit_distance` never build: the array form is refused by the engine, the object form is "Invalid operator". `process()`'s "Unexpected array" is unreachable.
- endpoint-host-port: `xapiand://host:port/idx` drops the host (node_name empty).
