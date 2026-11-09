# flush-memory-bound: Stabilize flush memory regression test

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:fb583d8 (2026-10-26); its ledger `docs/status/flush-memory-bound.md` is the record. Its machine state is in `state.json`: `campaign status flush-memory-bound`.

## Outcome

The regression test uses 64 KiB records and asserts that the sampled first-write budget exceeds flush scratch. It still detects a writer that copies borrowed records.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `f0ba032`: adjust test records and assert relation between filter scratch and flush charge: `docs/status/flush-memory-bound.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- None recorded in the ledger.
- Risk low: Changes only a test and its fixture sizing (the lead's mapping).

## May land early

None.
