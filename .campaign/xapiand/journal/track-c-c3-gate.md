# track-c-c3-gate: journal

State: ready for review
Quest: .campaign/xapiand/quests/track-c-c3-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Report written: docs/gates/track-c-c3.md in the strata worktree (strata:5cab3e7b). macOS rows green. FreeBSD
and Linux VM rows are resubmitted jobs (the lead said not to wait): FreeBSD rows dba12c, mutant 9d13e2;
Linux build 71911d, rows 3bd680, mutant 031f8b. Fold their results into the doc when they finish.

## Next

1. When the VM jobs finish, fill their cells in the gate doc; the architect's review is the lead's (A3).

## Blocked on

Nothing. Finding for the lead: Driver::repair forwards `Repair` to Core; T5's rows and the mutant are caught at Core::step(Repair). Driver-level coverage is one lone-voter row, and the Driver simulation never repairs. A Driver-level repair-below-base_index row belongs to a consensus quest.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | macOS: ea8fde, 961e8c, 8f229e (fails-without). FreeBSD: db899b (T5, T2 ok; Driver row failed on a lost binary), rerun dba12c and mutant 9d13e2 queued. Linux: 71911d, 3bd680, 031f8b queued. |
| A2 | docs/gates/track-c-c3.md (strata:5cab3e7b), each row with its job id |
| A3 | the lead's architect review: docs/reviews/track-c-c3.md |

## Result

macOS at consensus b7f48b2414d8: T5's rows (8 boundaries restored, 39,611 checks, 0 failures), the Driver rows (124 checks), T2 (200 seeds x 10,000 steps, 1,081 repairs) and the Driver simulation pass; the mutant "repair below base_index" fails the unit suite and the simulation (first at seed 1). VM rows pending as above.

## Rulings I made

- Ran jobs in the consensus repo (--repo, --commit pinned to main's sha) with an inline-filtered tools/mutants.sh; the mutant is judged by the script's own unit/membership/simulation report.
- vm.sh rebuilds missing targets and syncs before stopping the VM (FreeBSD lost its last-linked binary to poweroff).
