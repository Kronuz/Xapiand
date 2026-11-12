# loop-share-flake: journal

State: waiting on jobs 261112-105953-49f5c7
Quest: .campaign/xapiand/quests/loop-share-flake.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Test rewritten (xapiand:26f30b20f): restores the same 3000 docs into /onloop/ (schema with a root `_script`, so every object prepares on the loop, D1) and /offloop/, checks chores share < on-loop share * MARGIN (placeholder 0.8). Job 261112-105953-49f5c7 builds and prints the shares 3 times; pick the margin from them.

## Next

1. Set the margin from the job's numbers, comment it in one line.
2. `campaign verify loop-share-flake`; --fails-without job (A2); push the branch, record CI run id (A1).

## Blocked on

Nothing. Note: `DocIndexer::seams.serial` is not on master (only on quest/xapiand/x7-loop-fix-c), and the file set is tests only, so the on-loop baseline uses a schema root `_script` (every object prepares inline) instead.

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

- On-loop baseline via schema root `_script` (`_doc.n = _doc.n`), since seams.serial does not exist on master. The script adds a little Lua + stored-doc read per object to the baseline, so it overstates the loop's serial share slightly; the margin absorbs it.
