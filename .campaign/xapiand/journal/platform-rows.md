# platform-rows: journal

State: waiting on CI runs 43817110685 (FreeBSD) 43817110777 (arm64) at Xapiand:2c972efe5
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pushed 2c972efe5: freebsd.yml gains an ASan+UBSan build and gate7c_rows.sh as nobody; new arm64.yml (ubuntu-24.04-arm) builds, ctest, ASan, rows. Waiting on the two runs.

## Next

1. Read run results; fix harness/workflow failures in file set; source failures go to the lead (sortable-encoding owns src/**) or become a named quest (A3).
2. Add green run links to docs/dev/freebsd.md and new docs/dev/platforms.md (A1, A2); fill Evidence.

## Blocked on

Nothing.

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
