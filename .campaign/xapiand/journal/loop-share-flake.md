# loop-share-flake: journal

State: waiting on jobs 261112-114433-db65fa 261112-114436-2b0a05 and CI 43293281405
Quest: .campaign/xapiand/quests/loop-share-flake.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Final head f25ea81fd. A2's first mutant (job 261112-110934-a8f97e) was not evidence: no --fails-without, and test_loop_yield passed. The mutant did put the prepares inline (`fetch` false), but the old baseline (schema root `_script`) put its Lua off the loop (loop 1046 of 6220 ms process CPU), so the mutant's loop CPU was 626 ms, a 0.599 share against the 0.6 bound: too weak (diagnostic job 261112-112529-ed80c4).
Fix: the baseline is every object as an `_op_type":"update"` (reads the stored document, so it prepares on the loop, no Lua), 2000 documents of 1000 words. Measured (ctest -V): real code 0.40 (3000x300 words), 0.54 (2000x1000, under a concurrent build); mutant 0.57 (update baseline, 3000x300) and 0.88 (2000x1000, job 261112-113406-8ad22b: test Failed on the CHECK). Bound 0.7 sits between 0.54 and 0.88.
Jobs at f25ea81fd: mutant 261112-114433-db65fa (--for A2 --fails-without), build-test 261112-114436-2b0a05 (local). CI run 43293281405 (A1; the prior runs failed on test_node.cc:535, a hunt.md item, not this quest).

## Next

1. When the jobs finish: confirm the mutant failed on the loop-CPU CHECK (grep its log for "loop CPU"), then fill Evidence from `campaign brief` and the CI conclusion.

## Blocked on

Nothing. Note: `DocIndexer::seams.serial` is not on master (only on quest/xapiand/x7-loop-fix-c), and the file set is tests only, so the on-loop baseline uses a schema root `_script` (every object prepares inline) instead.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | CI run 43293281405 at f25ea81fd (Build + unit + smoke); local build-test 261112-114436-2b0a05 |
| A2 | job 261112-114433-db65fa (--fails-without: the loop-CPU check fails with every prepare inline) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- On-loop baseline is every object an `update` (above); the earlier `_script` baseline is dropped. (Was: schema root `_script` (`_doc.n = _doc.n`), since seams.serial does not exist on master. The script adds a little Lua + stored-doc read per object to the baseline, so it overstates the loop's serial share slightly; the margin absorbs it. Superseded.)
- The bound 0.7 and the 2000 x 1000-word body are measured, not argued (Doing).
