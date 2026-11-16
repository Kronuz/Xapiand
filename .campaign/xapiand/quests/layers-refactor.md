# layers-refactor: Refactor format layers and plugins

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:3e3f8b9 (2026-10-18); its ledger `docs/status/layers-refactor.md` is the record. Its machine state is in `state.json`: `campaign status layers-refactor`.

## Outcome

Format layers separate core lifecycle responsibilities from plugin-specific sections and merging. The reviewed code changes pass the recorded macOS suite and preserve the measured 20k corpus states byte-for-byte.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Steps 0 to 4b and steps 5 to 6 completed: `docs/status/layers-refactor.md`
- A2 [artifact] Step 7a adds merge_options; step 7b adds per-segment holds mask: `docs/status/layers-refactor.md`
- A3 [artifact] Revision 4 folds in re-check findings R12-R19 and C5-C8: `docs/status/layers-refactor.md`
- A4 [artifact] Code review conditions LC1-LC10 folded in; LC4 optional one-spill item not done: `docs/status/layers-refactor.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead, 2026-10-18: LC2 must be fixed, not accepted; LC3 requires a move test; LC4 deletes oracle paths at landing; LC1 and LC5-LC10 fold in text and comment fixes.
- Format 13's F13-R2 ruling favors planner routing by prefix and exact key bytes.
- Format 13's F13-R3 ruling sets P::structure/P::structures and per-kind table.
- Format 13's F13-R4 ruling places inverted statistics in term_index's head.
- Architect check of LC2's changed merge context remains pending.
- Risk high: shared format and lifecycle APIs (the lead's mapping).

## May land early

None.
