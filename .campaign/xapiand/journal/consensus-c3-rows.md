# consensus-c3-rows: journal

State: waiting on jobs 261118-044817-ce4093 261118-044818-d6773d 261118-044819-082e50
Quest: .campaign/xapiand/quests/consensus-c3-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rows and mutants committed at consensus:d2a2cc0 (consensus-wt branch). Jobs queued: build-test ce4093 (A3), P1 fails-without d6773d (A1), P2 fails-without 082e50 (A2). Jobs run .scratch/c3rows-mut.sh in the Xapiand home (applies the mutant to a copy of core.h, builds and runs consensus_test; exit 127 on a build problem).

## Next

1. Read the job results; if a mutant job shows a pass for the wrong reason, fix.

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

- The accept half of A2 ((applied, commit] accepted) is unreachable at Core: with applied < commit a delivery is outstanding or a batch pending, both answer Busy. The row covers the refusal at every boundary in (commit, last_index] on a follower; the existing loop covers acceptance up to applied. Lead to confirm.
- Renamed "a boundary above applied is refused" to "above commit".
