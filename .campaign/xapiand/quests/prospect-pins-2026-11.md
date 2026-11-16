# prospect-pins-2026-11: prospect pins to their heads (pin wave)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:f817398 (2026-11-02); its ledger `docs/status/pins-2026-11.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-pins-2026-11`.

## Outcome

prospect's Kronuz pins move to their heads per `cpp-libraries/docs/UPGRADING-2026-10.md` (strata to its main f2d7626, task, docset, bitpack, unicode, snowball, analysis, varint), with fixtures re-pinned for `pre_freeze` 0x8000000D and the hot path unchanged.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Every pin at its head, none in a "Commits not to pin" range; the resolved tree differs only in held fusefilter and docs-only commits: `docs/status/pins-2026-11.md`
- A2 [artifact] `test/background_merge_test.cc` runs its threaded cases on `WorkPool` and `work_pool_executor`: `docs/status/pins-2026-11.md`
- A3 [artifact] Fixtures, `pre_freeze.txt`, the prefix CRC and the reordering hash re-pinned for 0x8000000D (C17): `docs/status/pins-2026-11.md`
- A4 [artifact] Release ctest 51/51: `docs/status/pins-2026-11.md`
- A5 [artifact] Hot path `tools/ab_compare.py` 20 rounds under `quiet exclusive`: geomean 1.002, A/A 1.001, rows 0.988 to 1.024: `docs/status/pins-2026-11.md`
- A6 [artifact] CI on the branch: `docs/status/pins-2026-11.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Xapiand repins prospect to this branch's merge (Xapiand `pins-2026-11`).
- Risk normal: dependency moves that change segment bytes (pre_freeze bump) (the auditor's mapping).

## May land early

None.
