# loop-share-flake: journal

State: waiting on jobs 261112-110926-51a68c 261112-110934-a8f97e and CI
Quest: .campaign/xapiand/quests/loop-share-flake.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

The check now compares loop CPU ms (chores < on-loop * 0.6; measured about 0.33-0.36; the root script also puts Lua work off the loop, so shares invert, per the lead's msg). Jobs: build-test 261112-110926-51a68c (A1 local), mutant 261112-110934-a8f97e (A2: `fetch` returns false, so every prepare is inline; it must fail; check its log that it fails on the CHECK, not the build). Branch pushed for CI (A1).

## Next

1. When the jobs finish: confirm the mutant failed on the loop-CPU CHECK; record the CI run id; fill Evidence from `campaign brief`.

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
