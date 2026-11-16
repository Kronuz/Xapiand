# no-stalls: Research write-stall avoidance strategies

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:18febff (2026-10-30); its ledger `docs/status/no-stalls.md` is the record. Its machine state is in `state.json`: `campaign status no-stalls`.

## Outcome

The research note compares strata and RocksDB under sustained ingest and evaluates admission, refresh, merge scheduling, and checkpoint ideas. It documents measured tradeoffs and recommends a follow-on design.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] No explicit done criteria set: `docs/status/no-stalls.md`
- A2 [artifact] (no explicit criteria; from the Done list) Bound, survey, harness, calibration, core matrix, pause pairs, many-index matrix, losers, recommendations, findings, and reproduction method documented: `docs/status/no-stalls.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- None recorded in the ledger.
- Risk low: research and measurement only (the lead's mapping).

## May land early

None.
