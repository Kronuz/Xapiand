# phase7c-s1: Gate tooling and baseline

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:f3bd508f8 (2026-10-29); its ledger `docs/status/phase7c.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-s1`.

## Outcome

The gate tools measure load, thread counts, blocking primitives, and light-request profiles. The pre-7c baseline records load, latency, dispatch, and reactor-lag results for later comparison.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 reactor LagProbe, its tests, bucket edges, stop behavior, and allocation checks: `docs/status/phase7c.md`
- A2 [artifact] K2 Xapiand reactor probes and metrics, including docs E2E evidence: `docs/status/phase7c.md`
- A3 [artifact] K3 stall_load.py load, latency, overload, and dry-run checks: `docs/status/phase7c.md`
- A4 [artifact] K4 thread_census.sh names and counts threads and rejects unallowed prefixes: `docs/status/phase7c.md`
- A5 [artifact] K5 async_check.sh classifies blocking operations and supports strict gating: `docs/status/phase7c.md`
- A6 [artifact] K6 profile_light.sh and trace/hop measure CPU and wall time by group: `docs/status/phase7c.md`
- A7 [artifact] K7 commands for the lead's T5, T11, and T12 baseline on main: `docs/status/phase7c.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Control-loop T4 bound remains for the lead or architect to decide.
- Keep per-request note_activity() when L3 rewrites the loop.
- X6 switches async_check.sh to --strict --owed in CI.
- Risk low: tooling, tests, and measurement only (the lead's mapping).

## May land early

None.
