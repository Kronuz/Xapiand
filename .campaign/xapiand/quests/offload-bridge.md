# offload-bridge: Guarded database work off reactor loops

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:44f628903 (2026-10-26); its ledger `docs/status/offload-bridge.md` is the record. Its machine state is in `state.json`: `campaign status offload-bridge`.

## Outcome

A guarded bridge keeps blocking database work off reactor loops, and the control-plane reactor uses one worker for database tasks. Replication views release on the serving pool; remote-protocol teardown remains a stated residual.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] C1 Word the D13 rule for the lead to apply in strata: `docs/status/offload-bridge.md`
- A2 [artifact] C2 Keep one sync-wait bridge and guard; assert or log blocking sites: `docs/status/offload-bridge.md`
- A3 [artifact] C3 Offload FLUSH and correct both comments; add changelog bullet: `docs/status/offload-bridge.md`
- A4 [artifact] C4 Finish and drop the restore indexer on its worker on every exit: `docs/status/offload-bridge.md`
- A5 [artifact] C5 Run control-plane database work on one worker; resume state changes on loop; propagate exceptions; join worker before pool finish: `docs/status/offload-bridge.md`
- A6 [artifact] C6 Remove `Offload`, `set_offload`, and `XAPIAND_ENGINE_OFFLOAD_THREADS`; make `Hop` take a scheduler: `docs/status/offload-bridge.md`
- A7 [artifact] Guard fires at each reactor/scheduler blocking site; mutants fail 3 and 1 of 8 checks: `docs/status/offload-bridge.md`
- A8 [artifact] Docs E2E GREEN/GREEN, 0 ASan reports, 0 guard hits, clean exit: `docs/status/offload-bridge.md`
- A9 [artifact] FLUSH hammer 0 timeouts and no 5xx; UAF hunt 10 rounds, node alive, 0 ASan and guard hits: `docs/status/offload-bridge.md`
- A10 [artifact] TSan pool and blocking-guard tests: 3 runs each, 0 warnings: `docs/status/offload-bridge.md`
- A11 [artifact] Q1: no separate cap for Xapian I/O: `docs/status/offload-bridge.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- D13 forbids reactor loops from blocking on database work; remote protocol teardown is an explicit residual.
- The architect ruled against a separate Xapian I/O cap.
- CR2 and CR5 follow-ups were assigned to `fix/offload-followups`.
- Risk high: Changes concurrency boundaries for database and reactor work (the lead's mapping).

## May land early

None.
