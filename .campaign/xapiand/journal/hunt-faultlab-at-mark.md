# hunt-faultlab-at-mark: journal

State: waiting on jobs 261121-202427-8ee313 261121-202430-846c9f
Quest: .campaign/xapiand/quests/hunt-faultlab-at-mark.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Two cases added to faultlab test/faultfs_test.cc (faultlab:50b5394): at_mark and two ordering points. A1 = build-test job 261121-202430-846c9f. A2 = mutant job 261121-202427-8ee313 (script .scratch/xapiand/hunt-faultlab-at-mark/mutant.sh in the faultlab worktree, uncommitted): deletes `node_->at_mark.clear()` in the after-effect sync fault, builds, expects the test to exit 1.

## Next

1. Read both jobs; if the mutant is caught by the at_mark case, mark ready.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
