# failed-restore-exit: journal

State: waiting on jobs 261121-084644-7a6921 (A1 at the fix), 261121-084646-2a8d62 (A1 at Xapiand:586374bf7, must fail), 261121-084647-b4addc (suite, A2) and CI; normal risk: one correctness review, or a waiver if the evidence holds
Quest: .campaign/xapiand/quests/failed-restore-exit.md (the lead's; never edit it)

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
| A1 | 261121-084644-7a6921 (the fix); 261121-084646-2a8d62 (the script alone, must fail) |
| A2 | 261121-084647-b4addc |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
