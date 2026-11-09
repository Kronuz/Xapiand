# engine-core: Engine document and shard seam

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:566943e9e (2026-10-21); its ledger `docs/status/engine-core.md` is the record. Its machine state is in `state.json`: `campaign status engine-core`.

## Outcome

`engine::Document`, `DocView`, and the write/lifecycle `engine::Shard` provide an engine-neutral boundary backed by Xapian. Existing WAL, E2E, and query behavior remains unchanged in the recorded proofs.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `src/engine/` contains types, errors, `Document`, `DocView`, the `Shard` write and lifecycle slice, and Xapian implementation: `docs/status/engine-core.md`
- A2 [artifact] Migrate schema, document-side term generation, handler prepare/index, shard writes, WAL, pool, flags, and `exception_xapian.h`: `docs/status/engine-core.md`
- A3 [artifact] Preserve `Shard::db()` for reads; E2E and WAL recovery unchanged: `docs/status/engine-core.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Offload is off by default; the separate bounded pool was withdrawn (EO5); `Hop` takes a caller-chosen scheduler (C7).
- The lead chose one guarded `engine::sync_wait` bridge with `DatabasePool` checkout and cleanup.
- `engine::Error` is a mixin, not `std::exception`; wrappers also derive `ClientError`.
- Risk high: Shared public engine APIs and database lifecycle paths changed (the lead's mapping).

## May land early

None.
