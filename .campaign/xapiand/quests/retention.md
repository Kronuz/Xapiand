# retention: Default to zero retained predecessors

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:0156b57 (2026-10-21); its ledger `docs/status/retention.md` is the record. Its machine state is in `state.json`: `campaign status retention`.

## Outcome

The default `retained_predecessors` is 0, and damage to the current manifest or newest segment is refused and reported. Opens that fall back to a predecessor report data loss.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `770866d`: set default to 0; report fallback; update D4 text and add `test/retention_test.cc`: `docs/status/retention.md`
- A2 [artifact] (no explicit criteria; from the Done list) RET-1: retain every pending merge's sources and test the race at 0, 1, and 2 predecessors: `docs/status/retention.md`
- A3 [artifact] (no explicit criteria; from the Done list) RET-2: run lifecycle, index, reclaim, and media at default; confine predecessor settings to specified recovery fixtures: `docs/status/retention.md`
- A4 [artifact] (no explicit criteria; from the Done list) RET-3: D4 reports 47-108% more settled disk and cites retention measurements: `docs/status/retention.md`
- A5 [artifact] (no explicit criteria; from the Done list) RET-4/RET-6/RET-7: clarify local-manifest scope, recovery tool status, and in-memory reporting: `docs/status/retention.md`
- A6 [artifact] (no explicit criteria; from the Done list) RET-8: test a pinned-checkpoint gap that catches `discarded = newest - to`: `docs/status/retention.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner decision (2026-10-20): `retained_predecessors` defaults to 0; damage to current manifest or newest segment is refused and reported; 1 or 2 remain options; fallback reports data loss.
- Risk high: Changes recovery and predecessor-retention durability policy (the lead's mapping).

## May land early

None.
