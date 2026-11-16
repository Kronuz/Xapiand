# shared-reader-core: Add shared-reader hazard and snapshot core

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:c576997 (2026-10-21); its ledger `docs/status/shared-reader-core.md` is the record. Its machine state is in `state.json`: `campaign status shared-reader-core`.

## Outcome

Hazard records, copy-on-write deleted bits, and owner-side merge cleanup support shared readers and snapshots. The index and snapshot share segment-reading implementations.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Step 1 item 1: hazard records, lock-free protection pass, bounded retired pointers, and hazard/litmus tests: `docs/status/shared-reader-core.md`
- A2 [artifact] (no explicit criteria; from the Done list) Step 1 items 2 and 4: copy-on-write deleted bits, owner-side merge cleanup, shared segment readers and snapshot support: `docs/status/shared-reader-core.md`
- A3 [artifact] (no explicit criteria; from the Done list) Add T3 single-index acquire/release and get benchmark arms: `docs/status/shared-reader-core.md`
- A4 [artifact] (no explicit criteria; from the Done list) Fix review CR1-CR3 and CR6 with revert checks: `docs/status/shared-reader-core.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- SR-D18: protection pass takes no lock; claim-time table/high-water mark are stamped under a try-lock afterward.
- CR4: cross-thread-release cost is restated against a hand-off control; architect asked to revise T3's row.
- CR5: T3's 1.1x same-thread release threshold cannot be met by cross-thread hand-off; review asks architect to restate it.
- Risk high: Adds lock-free concurrency, reclamation, and shared-read surfaces (the lead's mapping).

## May land early

None.
