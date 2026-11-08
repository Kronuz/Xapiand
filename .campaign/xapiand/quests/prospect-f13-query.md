# prospect-f13-query: Format 13 query items (M16, M23, M20, M3t)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at prospect:45a710e (2026-10-19); its ledger `docs/status/f13-query.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-f13-query`.

## Outcome

Format 13's query items land: M16 advance entries at A3@128, M23 ordinal gaps as gap - 1 with header-width checks, M20 dominance pairs with skip blocks, and M3t bound-ordered single-term traversal; M22 (Elias-Fano) is out with codec code 2 reserved.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] M16 advance entries at A3@128 with checked entries; postings_test, segment_test and the new fuzz_postings: `docs/status/f13-query.md`
- A2 [artifact] M23 gaps stored as gap - 1, checked by header width or a 64-bit sum: `docs/status/f13-query.md`
- A3 [artifact] M22 measured and left out; code 2 reserved and load-bearing (063fafa): `docs/status/f13-query.md`
- A4 [artifact] M20 two dominance pairs per k per term; seeds off with deletes: `docs/status/f13-query.md`
- A5 [artifact] M3t bound-ordered traversal for single terms in reordered segments: `docs/status/f13-query.md`
- A6 [artifact] The kept set loses no row against main in four run-level sessions at 1M (21 to 38 winning rows each): `docs/status/f13-query.md`
- A7 [artifact] Revert checks and mutants per item (`.scratch/revert/mutate.py`): `docs/status/f13-query.md`
- A8 [artifact] Gate VM confirmation and the architect's F10 review: `docs/status/f13-query.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Each format change bumps `format::pre_freeze` (C17).
- F10 deviations listed for the architect's freeze-readiness review.
- Risk high: on-disk postings layout before the freeze (the auditor's mapping).

## May land early

None.
