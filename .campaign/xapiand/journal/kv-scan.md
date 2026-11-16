# kv-scan: journal

State: landed
Quest: .campaign/xapiand/quests/kv-scan.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:89f9fd7 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/kv-scan.md`: "[x] Newest wins for a key in several sources (buffer, then newest segment), deletes hide older" |
| A2 | `docs/status/kv-scan.md`: "[x] Empty index, from past the last key, limit 0 and 1, keys on block and restart boundaries." |
| A3 | `docs/status/kv-scan.md`: "[x] The explorer's digest at seed 42 (`81bb0f0b7fe8d333`) unchanged through the core API;" |
| A4 | `docs/status/kv-scan.md`: "[x] Full ctest, ASan+UBSan, a revert check per behaviour, one mutant on the tie rule." |

## Result

Landed at strata:89f9fd7 on 2026-10-21. Fuzz target walks lazy cursor and mutates keys sections; mutant seeking from start traps, restored replay covers 252 inputs. Review: `docs/reviews/kv-scan.md` -- approve with conditions.

- Merge: Merge remote-tracking branch 'origin/phase4/kv-scan'.
- `strata:ffeccee`: scan, sorted view, guard, thin `kv_strata` caller, and `scan_test`.
- Digest at seed 42, 100k records and 100k ops: `81bb0f0b7fe8d333`; value bytes 5,088,720.
- Full ctest 25 of 25 plain and ASan+UBSan; final rebased tree 31 of 31 ASan+UBSan.
- Final rebased tree: ctest 29 of 29 under ASan+UBSan and TSan, 0 reports; later retention rebase ASan+UBSan 31 of 31.
- CPU per op: explorer median 114.7, core scan median 104.4; quieter unheld run 19.7 us.
- Not merged; review and lead's quiet measurement remain; heap merge and pulled cursor are later work.
- Docs: ledger names `docs/perf/scan-lazy-merge.md`, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
