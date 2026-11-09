# cjk-segmenter: journal

State: working
Quest: .campaign/xapiand/quests/cjk-segmenter.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

At analysis:edf34a2 (segmenter, NFKC fast path, docs/cjk-segmenter.md, README, LICENSE-ICU), jobs
submitted: build-test 261109-153713-c159bf, A2 261109-153714-0ad2b4, A4 261109-153716-29ddc3,
A5 261109-153718-704c19 (exclusive). Local mutants of the DP (tie-break, unknown cost) re-run
against the corpora in a light hold (.scratch/mutants.sh in the worktree).

## Next

1. Read the jobs; fill the Evidence table and Result with their numbers.
2. If the tie-break or unknown-cost mutant survives ctest but the corpora kill it, add vectors that
   kill it in ctest (cases from the corpora).

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <job id at the current code> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
