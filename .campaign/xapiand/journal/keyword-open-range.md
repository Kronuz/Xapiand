# keyword-open-range: journal

State: waiting on job 261121-045338-baa3ca (A1 --fails-without at Xapiand:0703f2b27) and CI at the head; then lands (review waived: query-only fix with a failing-then-passing test)
Quest: .campaign/xapiand/quests/keyword-open-range.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261121-045337-0a227e (test_coverage_multivalue_range at the fix); 261121-045338-baa3ca (the same test at Xapiand:0703f2b27, the test commit alone, must fail) |
| A2 | 261121-045340-5b4053 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
