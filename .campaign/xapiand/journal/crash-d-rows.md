# crash-d-rows: journal

State: ready for review
Quest: .campaign/xapiand/quests/crash-d-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Cherry-picked e4884d358 as e580b0626. A1 job runs the whole script (A-D) as gate7c_rows.sh does (no workflow invokes it). A2 mutant 293735bfe on mutant/crash-d-rows.

## Next

1. Lead: check CI, then land.

## Blocked on

Nothing

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261119-122426-23b4be |
| A2 | 261119-122426-23b4be, 261119-131239-b80edb; fails without: 261119-122444-f74d98 (mutant 293735bfe: B wrong row '1 999999', D wrong row 'w1-1 999999', each with listing line and raw hit) |
| A3 | suite 261119-122452-83d60f passed; CI on the branch (FreeBSD, arm64) was running at 9f510b4e8 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
