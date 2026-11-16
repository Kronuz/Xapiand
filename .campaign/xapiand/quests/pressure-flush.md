# pressure-flush: Bound pressure-triggered flushing

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at strata:5737867 (2026-10-21); its ledger `docs/status/pressure-flush.md` is the record. Its machine state is in `state.json`: `campaign status pressure-flush`.

## Outcome

`needs_flush()` waits for a bounded pressure threshold, controlled by `pressure_flush_divisor`. The benchmark tooling records failed runs and adds pressure workloads.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `165985b`: add `pressure_flush_divisor` (default 4; 0 flushes on any pressure) and bound the pressure gate by budget capacity: `docs/status/pressure-flush.md`
- A2 [artifact] (no explicit criteria; from the Done list) `165985b`, `9536143`: add `pressure_flush_bounded` regression test; 2000 applies produce 14 segments by default and 1999 with divisor 0: `docs/status/pressure-flush.md`
- A3 [artifact] (no explicit criteria; from the Done list) Add `pressure` and `pressure_more` benchmark row sets and record failed runs instead of ending the set: `docs/status/pressure-flush.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- None recorded in the ledger.
- Risk normal: Adjusts ordinary flush timing and adds an option (the lead's mapping).

## May land early

None.
