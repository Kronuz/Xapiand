# group-heal: journal

State: waiting on jobs 261121-054230-6f2ce0 261121-054233-ce72e0 261121-055544-08fada
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Task `install` (journal group-heal--install): the transfer is done; its binding to consensus's
Driver install surface (driver-install branch, 887a88b) is written at strata:87bc0ebf on
quest/xapiand/group-heal--install, first build queued. Task `sim-port` merged.

## Next

1. install: build and prove the binding (A1, A2), then merge into the quest branch.
2. repair, witness (order 2), then sim-rows (order 3): not started.

## Blocked on

Lead: CMakeLists.txt's consensus pin and group_install_test registration (group-heal--install,
Blocked on 1).

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
