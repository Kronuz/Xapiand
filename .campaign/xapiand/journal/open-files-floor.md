# open-files-floor: journal

State: waiting on jobs 261121-163011-1ee7dc 261121-163013-2b7cd3 261121-163015-5d9da9 261121-163007-0100b9
Quest: .campaign/xapiand/quests/open-files-floor.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged explore/xapiand/open-files-floor (clean, no conflicts with failed-restore-exit) at d9f7622afc5e. Jobs queued.

## Next

1. When jobs pass: paste `campaign brief` table into Evidence; State ready for review.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | unit 261121-163011-1ee7dc, e2e 261121-163013-2b7cd3 at fix; fails-without at master main.cc 261121-163015-5d9da9 (nonzero must be the test failing, not a build error: check its log) |
| A2 | suite 261121-163007-0100b9 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
