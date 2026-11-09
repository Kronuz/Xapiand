# field-table: Append-only field declaration table

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:c4ea4ce (2026-10-28); its ledger `docs/status/field-table.md` is the record. Its machine state is in `state.json`: `campaign status field-table`.

## Outcome

Strata indexes persist declared fields in an append-only, CRC-encoded table referenced by the manifest. Transactions resolve field references against table marks and reject conflicting declarations.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `core/fields.hh` implements table, codec, generation file, manifest reference replacing slots, version bump, open, chain recomputation, check, and digest: `docs/status/field-table.md`
- A2 [artifact] Transactions carry `fields` and `expected_fields`; stage 1 applies D2 and D7 rules, undo rolls back declarations, and `fields()` returns the marked table: `docs/status/field-table.md`
- A3 [artifact] FK-R1 tests cover stale-table pipelines, rollback/reuse, lost tails, replay, install, checkpoints, replicas, crash points, decoder fuzzing, revert checks, mutants, sanitizers, and CI: `docs/status/field-table.md`
- A4 [artifact] T8 is measured and generation encoding is chosen against it (C6): `docs/status/field-table.md`
- A5 [artifact] `docs/mechanisms/field-table.md` exists; `key_prefix` and segments remain untouched: `docs/status/field-table.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Field-table file is append-only in place; bytes are never overwritten, and a manifest names a prefix.
- T8 chooses append-only commit blocks rather than rewriting the whole table.
- Risk high: Adds persistent field files, manifest references, and a new format version (the lead's mapping).

## May land early

None.
