# key-field: journal

State: landed
Quest: .campaign/xapiand/quests/key-field.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:cef4e59 on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/key-field.md`: "Ticked. Above; every caller in strata tests, benches (field_table, idle_index, kv, shared_reader)" |
| A2 | `docs/status/key-field.md`: "Ticked. `index_test`: keys of all 256 first bytes (plus NUL pair and a Q key)" |
| A3 | `docs/status/key-field.md`: "Ticked. `tools/mutants/keys.py` (8 mutants plus the unmutated copy" |
| A4 | `docs/status/key-field.md`: "Ticked. Fixtures `test/fixtures/{manifest,live,meta}.bin`: an independent script rebuilds each" |
| A5 | `docs/status/key-field.md`: "Ticked. Release 57/57; ASan+UBSan (LLVM 22, Debug -O1, own tree) 57/57; TSan (own tree) 57/57." |
| A6 | open: left unticked; CI pushed, not awaited in `docs/status/key-field.md` |

## Result

Landed at strata:cef4e59 on 2026-10-28. Release, ASan+UBSan, and TSan each pass 57/57. Reviews: `docs/reviews/key-field-security.md` -- APPROVE (decoder safety, 2026-10-28, strata:d4e43a9; one low finding SR1); `docs/reviews/fields-and-keys-design.md` (Re-check 1).

- Merge: Merge branch 'phase8/key-field' (F3: delete key_prefix).
- strata:1c782e9 delivers core, tests, and fixtures; strata:8494ca3 records proofs.
- Manifest fuzz: 9,336,100 runs in 7 minutes, no finding; 187 corpus entries added.
- Six kv trials: all ten rows tie; B/A 0.956 to 1.053; peak RSS and disk equal.
- Fixtures differ only by version byte 0x86 to 0x87, removal of `01 51`, and recomputed CRC-32C.
- CI was pushed, not awaited. The decoder-safety review the ledger left next is `docs/reviews/key-field-security.md` (APPROVE).
- Docs: `docs/design/fields-and-keys.md`, `docs/perf/regression-2026-10-28.md`.
- Left open: A6.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
