# coverage-areas: journal

State: working
Quest: .campaign/xapiand/quests/coverage-areas.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Harness committed (0ec136187: coverage-uncovered.txt lists uncovered lines/branches per file). Baseline job d4c5ad (A1 "before", ctest + corpora + e2e at 0ec136187) queued. New tests drafted, not yet compiled (hold covareas-t1 queued behind a strict hold): tests/test_coverage_{geospatial,y2j,aggregations,reader_registry}.cc. Local build dir .scratch/build (uncommitted CMakeLists.txt edit = test-glob's, drop it when test-glob lands and rebase).

## Next

1. Compile and run the four new tests (cmake --build .scratch/build --target xapiand_test_coverage_*), fix, commit.
2. With the baseline's coverage-uncovered.txt, target lua, decoders (field_parser, serialize, strata document/pattern/codec), then server and database error paths.
3. Mutant per area as a --fails-without job; after-coverage job; docs/dev/coverage.md; A2 job = test_coverage_reader_registry output.

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
