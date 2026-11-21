# xapiand-final-release: journal

State: waiting on jobs 261120-174741-3e4b95
Quest: .campaign/xapiand/quests/xapiand-final-release.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Backports on the quest branch (based on the `xapiand` tip e692a64b1, so the tool's worktree base fits). Done: parser cherry-picks (4b9ee717e, 8ab6b2b26) with the adapter on by default and a nested NEAR test; id 0 (971f90559, d0c9a5733) with the harness check added to the smoke layer; A3 (fdb32df96); changelog (72ebbfa83). Job 261120-155714-a3ba72 failed (getStopWords include resolved to the wrong stopper.h); fixed; build-test 261120-172811-fc88f8 failed 3 of 61 checks of test_generate_terms (lines 334-336, the positive/floating terms-only loop); base-commit run (e692a64b1) is job 261120-174741-3e4b95 to tell pre-existing from mine.

## Next

1. On a green build-test: push the quest branch, then fast-forward `xapiand` to it and push (CI there: build-test, asan, e2e, Release/FreeBSD/Bottles dispatch).
2. A5: read CI and the three workflows at the final commit; report to the lead. No tag.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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

- Tool flow: the quest branch already sits on `xapiand` (worktree base fits); `campaign land` lands on main, so landing is `--record` after the tag, as the quest says. `campaign brief` flags "master changed 1485 files" and out-of-file-set changes: expected, this branch is not master's.
- A1: the adapter was opt-in (XAPIAND_QUERYPARSER=1); made default, =0 selects Xapian. test_text_parser pins =0 as its reference.
- A2: the id-0 check was a harness script only; run_all.sh smoke now runs it (build-test runs smoke).
- A3 float bounds: lower bound ceil, upper bound floor, saturating, in range.cc integer_bound.
