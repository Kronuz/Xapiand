# coverage-multivalue: journal

State: waiting on jobs 2d327a 6819b2 0f1a51
Quest: .campaign/xapiand/quests/coverage-multivalue.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Three test files written (tests/test_coverage_multivalue.cc keymaker; _terms.cc generate_terms; _range.cc range + geospatialrange), never compiled until jobs 2d327a (keymaker), 6819b2 (terms), 0f1a51 (range) finish: read their out/ logs, fix failures (pin real behavior, list bugs below), then full coverage + suite jobs.

## Next

1. Read the three coverage_one jobs; fix tests; per-file numbers vs 90 lines / 80 branches (area total 1166/1657 lines at coverage-areas 8642ea).
2. Coverage job (harness/coverage.sh, WORKLOADS="ctest corpora e2e", newman on PATH) and suite job at the head; Evidence rows; push; ready.

Lead msg: if 2d327a, 6819b2 or 0f1a51 times out (exit 124), resubmit one light job, --minutes 150, running coverage_one.sh for all three tests in turn (e.g. bash -c with BUILD per test). None had timed out when read.

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

## Bugs found (pinned in tests, src/ untouched)

- M1 MIN_STR_CMPVALUE (keymaker.cc) is std::string("\x00"): built from a C string it is empty, not NUL; a descending SerializeKey on a missing slot keys as empty.
