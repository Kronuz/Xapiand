# track-c-c3-gate: journal

State: ready to land
Quest: .campaign/xapiand/quests/track-c-c3-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Gate doc (strata:7cd834b3) reports the rows at consensus:6a76867, every VM cell filled from the finished jobs, F4 and N2 as residuals; docs/reviews/track-c-c3.md carries the accepted review. `campaign verify` reused build-test 261118-042103-6cde19 (docs-only change).

## Next

1. The lead lands the quest.

## Blocked on

Nothing. Finding for the lead: Driver::repair forwards `Repair` to Core; T5's rows and the mutant are caught at Core::step(Repair). Driver-level coverage is one lone-voter row, and the Driver simulation never repairs. A Driver-level repair-below-base_index row belongs to a consensus quest.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261118-042103-6cde19; fails without: 261118-025955-8f229e |
| A2 | docs/gates/track-c-c3.md (strata:7cd834b3), each row with its job id |
| A3 | docs/reviews/track-c-c3.md (strata:7cd834b3): the correctness review, accepted at strata:5cab3e7b, re-checked at consensus:6a76867 |

## Result

macOS at consensus b7f48b2414d8: T5's rows (8 boundaries restored, 39,611 checks, 0 failures), the Driver rows (124 checks), T2 (200 seeds x 10,000 steps, 1,081 repairs) and the Driver simulation pass; the mutant "repair below base_index" fails the unit suite and the simulation (first at seed 1). Linux and FreeBSD rows pass at the same commit (jobs in the gate doc). Rows at consensus:6a76867 (F1, F2 mutants P1, P2) are macOS-only.

## Rulings I made

- Ran jobs in the consensus repo (--repo, --commit pinned to main's sha) with an inline-filtered tools/mutants.sh; the mutant is judged by the script's own unit/membership/simulation report.
- vm.sh rebuilds missing targets and syncs before stopping the VM (FreeBSD lost its last-linked binary to poweroff).
