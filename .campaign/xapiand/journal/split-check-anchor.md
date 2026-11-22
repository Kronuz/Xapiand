# split-check-anchor: journal

State: ready to land; the lead reads the diff and lands with --push
Quest: .campaign/xapiand/quests/split-check-anchor.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

The lead fills this journal (the contributor's work is cpp-libraries:449d913; the lead's pyrefly fix is fe8491f). Only evidence remains.

## Next

1. When the jobs and CI pass, the lead reads the diff and lands with --push.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | the `test` check at fe8491f: 261121-220731-d77242 (passed): test_split_check.py's repeated-line fixtures; the new tests fail on the checker before the change: 261121-204458-816e97 (queued) |
| A2 | the `lint` check at fe8491f: 261121-220731-795185; tool tests: 261121-220731-d77242 (passed) |
| A3 | 261122-000012-53c86b (0 errors, 43 warnings): fe8491f's split_check.py on schema-split's head Xapiand:1d471a9 (`--base master src/database`) |

## Result

split_check matches a split's repeated lines (`}`, a shared signature line) by their unique anchors (449d913). The lead narrowed one Optional for pyrefly (fe8491f).

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
