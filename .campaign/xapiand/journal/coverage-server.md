# coverage-server: journal

State: waiting on jobs 261119-003910-444b1a
Quest: .campaign/xapiand/quests/coverage-server.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Driver harness/coverage_e2e_server.py (groups per node flag set) + tests/test_coverage_server.cc written blind against an old binary; coverage.sh has a `server` workload and NOBUILD=1. Job 444b1a is the first coverage build (slot xapiand-coverage-server keeps build-cov: run the driver against its bin/xapiand to fix expectations, then NOBUILD=1 jobs). Baseline (coverage-areas job 8642ea): server 76.8/49.4, search_views.cc 76.3/48.9, mime_types.cc 88.2/66.0.

## Next

1. Read job 444b1a's log, fix the driver's expectations against the real binary, then read coverage-uncovered.txt and add scenarios until server >= 90 lines / 80 branches.

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
