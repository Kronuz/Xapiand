# cold-tier-item5: Demotion promotion and owed maps

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:081b7e7 (2026-11-02); its ledger `docs/status/cold-tier-item5.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-item5`.

## Outcome

`demote()` twins every record and publishes the change; `promote()` restores mappings when owed work requires them. The `map_owed` answer prevents mapping during deferred IO, while counters expose mapping losses and held views.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K5.1 Predicate refuses when buffered, merge-pending or unsynced; refusal changes nothing: `docs/status/cold-tier-item5.md`
- A2 [artifact] K5.2 Every record is twinned, including unbodied records: `docs/status/cold-tier-item5.md`
- A3 [artifact] K5.3 Twin records retain `deletes_dirty`, markers, bits and mapped-body facts: `docs/status/cold-tier-item5.md`
- A4 [artifact] K5.4 Demotion publishes and frees views; `views_held()` counts by segment id: `docs/status/cold-tier-item5.md`
- A5 [artifact] K5.5 Demotion and promotion are invisible to reads and contents: `docs/status/cold-tier-item5.md`
- A6 [artifact] K5.6 `map_owed` applies only under `defer_io`; promotion enables retry without mapping: `docs/status/cold-tier-item5.md`
- A7 [artifact] K5.7 Owed state ends correctly on answers, errors and close; markers remain corruption: `docs/status/cold-tier-item5.md`
- A8 [artifact] K5.8 Close with a posted delete is carried to step 2's sweep; not run here: `docs/status/cold-tier-item5.md`
- A9 [artifact] K5.9 T5 overlap and 120-round runs exercise readers, merges, commits, reclaim, demotion and owed answers: `docs/status/cold-tier-item5.md`
- A10 [artifact] K5.10 Turn bound tests allow at most one of each answer and at most 3 retries: `docs/status/cold-tier-item5.md`
- A11 [artifact] K5.11 Reads after promotion avoid unleased maps: `docs/status/cold-tier-item5.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- `demote()` relies on `publish_point()`'s registry scan rather than a separate `free_views()`.
- The owed bit is cleared by apply outcomes other than owed, throws and close; refresh and commit do not clear it.
- `views_held()` matches records by segment id.
- Risk high: Changes concurrent mapping, durability-adjacent state and lease behavior (the lead's mapping).

## May land early

None.
