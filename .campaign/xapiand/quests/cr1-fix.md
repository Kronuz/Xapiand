# cr1-fix: Commit WAL-replayed operations

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:6c5c1a40d (2026-10-27); its ledger `docs/status/cr1-fix.md` is the record. Its machine state is in `state.json`: `campaign status cr1-fix`.

## Outcome

A writable shard commits pending replay work through `Shard::commit`, including the revision, WAL COMMIT line, and update notification. Transactional shards retain replay work in the open transaction; the WAL durability holes remain tracked.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Commit replay through `Shard::commit(true, true)` from `reopen_writable`: `docs/status/cr1-fix.md`
- A2 [artifact] Add engine replay test and WAL replay check; add cluster step and mechanism note: `docs/status/cr1-fix.md`
- A3 [artifact] Commit only when `!is_transactional()` so replay inside a transaction remains cancellable: `docs/status/cr1-fix.md`
- A4 [artifact] Verify replay idempotence, replication COMMIT line, failure propagation, and recovery: `docs/status/cr1-fix.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Transactional replay remains in its transaction; committing it would publish operations a later cancel must discard.
- `DatabaseWAL::execute()` keeps the commit in `reopen_writable`, its sole caller and owner of open state.
- Replay commit advances glass revision; the glass revision determines whether replay is skipped.
- Risk high: Changes durability and recovery behavior (the lead's mapping).

## May land early

None.
