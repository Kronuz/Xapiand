# completion-task-pin: journal

State: waiting on jobs 261120-231816-526106
Quest: .campaign/xapiand/quests/completion-task-pin.md (the lead's; never edit it)

## Doing

Lead-done: completion:714186e moves completion's task pin from 18eb516 to 95798a0, the commit
journal and consensus pin. The top-level build turns COMPLETION_TASK on, so the build-test job
fetches and builds the new pin.

## Next

1. Land on the build-test job and CI; then tell Detent's lead (msg-193) and file the journal and
   consensus repins to this commit after journal-turn-buffers and node-admission land.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261120-231816-526106 at completion:714186e |
| A2 | CI at completion:714186e |

## Result

completion pins task 95798a0 (from 18eb516): blocking_scheduler, inline post_call, the
settle-behind-cancel fix and notes come in; completion's API is unchanged.

## Rulings I made

None.
