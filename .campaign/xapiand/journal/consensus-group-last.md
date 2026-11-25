# consensus-group-last: journal

State: waiting on jobs 261124-225604-857136 261124-225605-635aa9 261124-225614-d4ca21
Quest: .campaign/xapiand/quests/consensus-group-last.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and pushed (consensus branch quest/xapiand/consensus-group-last, consensus-group-last head b53d1db). Jobs queued; nothing else to do until they finish.

## Next

1. When the three jobs pass, set State to ready for review.
2. If the mutant job passes for the wrong reason (build error), rerun locally: `consensus_driver_test group_last` under `-DCONSENSUS_DRIVER_MUTANT=group-last-committed` must print `FAIL: last counts the staged entry`.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | plain: 261124-235520-47acc3 (`driver_test group_last`: plain log, checkpoint plus tail, held-turn staged entry; `driver_close_test group_last_closed`); mutant `group-last-committed` (reports `committed`): 261124-225605-635aa9 --fails-without |
| A2 | build-test 261124-235519-6bd5e7 |
| A3 | `CHANGELOG.md`: "Driver::GroupState::last" |

## Result

`Driver::GroupState` gains `Index last` (after `applied`), the Worker's `last_index()`, filled in `Driver::state` (the one place GroupState is built). Tests (consensus `test/driver_test.cc` `group_last`, `test/driver_close_test.cc` `group_last_closed`, `group_last` added to `test/driver_checks.sh`): last follows each append; recover from a plain log gives old end + 1 (a recovered lone voter's new-term no-op); from a checkpoint plus a tail the same; from a closed record exactly the record's log end; a proposal staged in a held turn moves `last` but not `committed` (the check that kills the mutant, since a recovered lone voter has committed == last). Mutant `group-last-committed` added to `test/driver_mutants.py` (`check` applies all 19). Local targeted run: 37 + 11 checks, 0 failures. Not done (outside the file set): `docs/DRIVER.md:142` lists the old GroupState fields.

## Rulings I made

- The expected index after a lone voter's recover is the log read plus the new term's no-op, since the Worker is already leading; the closed-record case has no such no-op (leader in its closed term).
- The mutant is killed by the held-turn staged-entry check, not by the recover cases (there committed == last).
- `docs/DRIVER.md` left for the lead (not in the file set).
