# phase7c-x5: Task-based caches and control plane

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:d3f6cc1fd (2026-11-02); its ledger `docs/status/phase7c-x5.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x5`.

## Outcome

Schema and settings reads use single-flight loading, and control-plane setup, housekeeping, signals, and autocommits run as tasks. Shutdown uses a bounded wait and reports unfinished work.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Hold no cache lock across I/O and load each concurrent first-use schema/settings once: `docs/status/phase7c-x5.md`
- A2 [artifact] K2 Run control-plane work as tasks without blocking the control loop: `docs/status/phase7c-x5.md`
- A3 [artifact] K3 Classify inventory J with strict async checks: `docs/status/phase7c-x5.md`
- A4 [artifact] K4 Implement C10 chore resume behavior and C14 shutdown bound: `docs/status/phase7c-x5.md`
- A5 [artifact] K5 Prove builds, tests, E2E, sanitizers, recovery, and shutdown behavior: `docs/status/phase7c-x5.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Setup and shutdown-check failures are fatal; signal-reader, housekeeping, autocommit, and per-entry chore failures are logged and retried.
- Retain the second shutdown bound; making the whole stop one bound remains a lead decision.
- `--committers` no longer sizes anything; gauges count watch tasks and waiting keys.
- Risk high: concurrency, shutdown, and shared state (the lead's mapping).

## May land early

None.
