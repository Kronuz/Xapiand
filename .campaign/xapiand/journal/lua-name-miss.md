# lua-name-miss: journal

State: ready for review
Quest: .campaign/xapiand/quests/lua-name-miss.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix in compile_local (processor.cc): empty body on a miss throws ClientError naming the script. Test and docs done.

## Next

1. Review.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | pass 261116-191106-ac85b9; fails without the fix (master processor.cc) 261116-185852-7c38a4 |
| A2 | docs/mechanisms/lua-memory.md (unbounded wait; C1 error) |
| A3 | suite 261116-191113-863775 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
