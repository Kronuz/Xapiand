# hunt-single-flight-race: journal

State: ready for review
Quest: .campaign/xapiand/quests/hunt-single-flight-race.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261123-164230-775c11 (300 of 300 runs passed at 82389d31ba04); check build-test 261123-164232-54d3be |
| A2 | CI at the landing commit |

Failed jobs, lead's slips, not the test: 261123-161346-8a2d4b and 261123-163836-bf6a80 named a target that does not
exist (test_single_flight; the executable is xapiand_test_single_flight) and configured without -DBUILD_TESTS=ON;
261123-163833-755105 was cancelled for the same reason.

## Result

B counts its cancellation inside its own task, which ends a moment later; the test now waits for the task count
to reach 2 instead of reading it once (Xapiand:82389d31b). 300 of 300 runs pass; master CI failed it in run 45242486008.

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
