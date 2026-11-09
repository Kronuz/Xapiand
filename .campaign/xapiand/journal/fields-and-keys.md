# fields-and-keys: journal

State: landed
Quest: .campaign/xapiand/quests/fields-and-keys.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:0078bb6 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/fields-and-keys.md`: "none set yet. The design proposes F1's, X6's, F2's and SF's in section 7." |

## Result

Landed at strata:0078bb6 on 2026-10-27. Probe ratios: bank 0.875x and 0.879x; sparse-kw 1.110x and 1.072x; sparse-num 0.936x and 0.881x; Wikipedia 0.921x. Review: `docs/reviews/fields-and-keys-design.md` (approve with conditions; re-check pending).

- Merge: Merge branch 'design/fields-and-keys' (fields and keys design, revision 1).
- strata:eef97f2 records revision 0; revision 1 folds in FK-R1 to FK-R11 and C1 to C6.
- The probe ran twice end to end; all 18 reports were byte-identical.
- Each shard's wdf sum matched its total length; `dirprobe.py` reproduces 530 B and 18,000 B.
- No strata, prospect, or Xapiand build or test ran.
- Preliminary review re-check and F1/X6 criteria remain open.
- Docs: `docs/design/fields-and-keys.md`, docs/design/fields-and-keys/.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
