# prospect-fields: prospect onto first-class fields (F2)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at prospect:3aeabbb (2026-10-28); its ledger `docs/status/fields.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-fields`.

## Outcome

A term is (field id, bytes): prospect's dictionary, writer, tuple buffer, merge, reader, check and digest work per field, with the key field routed to `keys`, statistics by field name across shards, and segment version 0x8000000C (fields-and-keys F2).

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] F1 `fielddict.hh` with mutants A, B, C, I: `docs/status/fields.md`
- A2 [artifact] F2 T1 sizes on the real builder: `docs/status/fields.md`
- A3 [artifact] F3 field parts, stage 1 rejections; mutants D, E: `docs/status/fields.md`
- A4 [artifact] F4 writer, tuple buffer, merge, reader, check, digest on (field, bytes): `docs/status/fields.md`
- A5 [artifact] F5 D5 host length and search identity with main: `docs/status/fields.md`
- A6 [artifact] F6 the key field routes to `keys`; mutant G: `docs/status/fields.md`
- A7 [artifact] F7 term table, `snapshot_df` and `multi_snapshot::df` by name; T5b 720 comparisons; mutant F: `docs/status/fields.md`
- A8 [artifact] F8 T3b parity against Xapian on the bank, 240 queries, worst error 4.1e-16 (vectors `strata-oracle:8e6611a`, local): `docs/status/fields.md`
- A9 [artifact] F9 per-field min/max, counts at merge, live statistics: `docs/status/fields.md`
- A10 [artifact] F10 T6 open cost for 49- and 2,100-field segments: `docs/status/fields.md`
- A11 [artifact] F11 strata bump 0x8000000C, fixtures re-pinned, `fuzz/fuzz_fielddict.cc`, mutants A-M: `docs/status/fields.md`
- A12 [artifact] F12 ASan+UBSan and TSan ctest 47 of 47; CI green on prospect:8588afe and strata:32b6800: `docs/status/fields.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner ruling (2026-10-28, R-P2): no term lists; P2 abandoned.
- Seven shape decisions (internal key, dictionary layout, plan before write, norms, version, a snapshot without a field table throws, tests declare `text`).
- Risk high: on-disk dictionary format and search identity (the auditor's mapping).

## May land early

None.
