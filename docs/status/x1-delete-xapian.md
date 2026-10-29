> Status: CURRENT (2026-10-29) Phase 7c step X1, branch `phase7c/x1-delete-xapian` (from master `Xapiand:3b0dc433f`); workstream x1-delete-xapian, developer. Local only, not pushed.

# X1: delete the Xapian path

## State

In progress. The source is converted and the docs are rewritten; the debug build, the unit tests and the docs E2E are being brought green.

## Done criteria (set by the lead)

| # | Criterion | Evidence |
|---|---|---|
| K1 | `src/xapian/` gone; no Xapian include, link, CMake option or dependency | pending |
| K2 | The Xapian adapter and the engine switch gone | pending |
| K3 | Remote, replication, election and cluster-only code removed | pending |
| K4 | `sortable_serialise` kept if used | pending |
| K5 | Release and Debug clean, unit tests, docs E2E, one ASan/UBSan build | pending |
| K6 | Changelog, extraction ledger, README, ARCHITECTURE, AGENTS | pending |

## Done

## Next

## Blocked on

Nothing.
