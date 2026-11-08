# phase7c-x2: Registry, lease, and chores

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:d25fb70a4 (2026-10-30); its ledger `docs/status/phase7c-x2.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x2`.

## Outcome

The registry serializes index operations through task-based leases, while chores handle engine and storage I/O. Open, close, commit, and shutdown paths have concurrency and crash-recovery checks.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Pin task, threadpool, and strata dependencies: `docs/status/phase7c-x2.md`
- A2 [artifact] K2 Implement registry and lease: single-flight opens, operation turns, deadlines, and close protocol: `docs/status/phase7c-x2.md`
- A3 [artifact] K3 Add host mode and chores; preserve apply/WAL order: `docs/status/phase7c-x2.md`
- A4 [artifact] K4 Run open, refresh, commit, close, merge publication, and reclaim work as chores: `docs/status/phase7c-x2.md`
- A5 [artifact] K5 Partially remove sync_wait from server paths; remaining handler waits are X2b: `docs/status/phase7c-x2.md`
- A6 [artifact] K6 Prove builds, tests, E2E, sanitizers, crash checks, and shutdown behavior: `docs/status/phase7c-x2.md`
- A7 [artifact] K7 Add the mechanism note and changelog: `docs/status/phase7c-x2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- X2 keeps handler sync_wait as a bridge because views, schema code, and Lua are synchronous callers.
- Xapiand does not call check_out; strata defer_io returns owed work rather than doing I/O inline.
- Risk high: concurrency, durability, and shutdown behavior (the lead's mapping).

## May land early

None.
