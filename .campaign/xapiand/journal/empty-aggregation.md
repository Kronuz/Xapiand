# empty-aggregation: journal

State: waiting on jobs 261121-090800-a9013e (A1 at the fix; ran at Xapiand:ee5bbdaae, same code), 261121-090803-0ef0af (A1 at Xapiand:6720a2f5c, must fail), the suite job and CI; normal risk: one correctness review, or a waiver if the evidence holds
Quest: .campaign/xapiand/quests/empty-aggregation.md (the lead's; never edit it)

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
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261121-090800-a9013e (the fix); 261121-090803-0ef0af (master's code with the test, must fail) |
| A2 | the suite job |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
