# cold-tier-step2-item4: Wake protocol and bounded closes

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:4795b7e (2026-11-03); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item4`.

## Outcome

Owed slots wake through a chore that promotes mappings, then delivers waiters after releasing its lease scope. Failed wakes back off; bound closes commit uncommitted work and close slots after repeated failures.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S5 No request pin before promotion; unleased maps stay zero and apply does not return map_owed: `docs/status/cold-tier-step2.md`
- A2 [artifact] S5 Chore pins on owed commit without waking; chore-pin stream cannot starve wake: `docs/status/cold-tier-step2.md`
- A3 [artifact] S5 Nested chore pin, timer repost backoff, final scope release and too-many-open backoff work: `docs/status/cold-tier-step2.md`
- A4 [artifact] S6 Persistent EMFILE backs off; request pins fail fast and chore pins still enter: `docs/status/cold-tier-step2.md`
- A5 [artifact] S6 Eighth failure closes committed slot; uncommitted slot commits before close; failed commit leaves owed with 10 s probe: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Host timer is required; repost delays double from 1 ms to 64 ms and timer callback only posts work.
- `too_many_open` takes backoff but is excluded from failure count.
- Bound-close chore commits then demotes mapped records; a failed commit returns to owed with 10 s probe.
- Risk high: Changes concurrent wake, timer, retry and close protocols (the lead's mapping).

## May land early

None.
