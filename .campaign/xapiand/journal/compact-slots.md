# compact-slots: journal

State: landed
Quest: .campaign/xapiand/quests/compact-slots.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:8fcd4e3 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/compact-slots.md`: "8-byte slots (no switch), 7/8 growth, docid map compares the buffered docid, `buffered_docs_cap`" |
| A2 | `docs/status/compact-slots.md`: "hashes named in index_detail plus a spread test; limits.md row" |
| A3 | `docs/status/compact-slots.md`: "No tombstones: erase shifts later entries back (tested vs std::map with hashes squeezed to 1, 5, 97 values" |
| A4 | `docs/status/compact-slots.md`: "The borrow/lease rules of flush-memory are untouched: the maps are not borrowed from and clear with the buffer." |
| A5 | `docs/status/compact-slots.md`: "the writer flushes before a transaction would pass `buffered_docs_cap`" |
| A6 | `docs/status/compact-slots.md`: "full ctest 32/32 Release, 32/32 ASan+UBSan, 32/32 TSan, no reports." |

## Result

Landed at strata:8fcd4e3 on 2026-10-21. `limits.md` gains a row; index_test budget case drops from 200 to 128 KiB. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/phase4/compact-slots'.
- `strata:23b9a3f`: 8-byte slots, 7/8 growth, docid comparison, cap; `strata:049e835`: named hashes and spread test.
- Full ctest 32/32 Release, ASan+UBSan, and TSan; no reports.
- Mutants for comparisons, growth, cap, erase, wrap, and hash folding are caught by `compact_slots_test`.
- Peak RSS median 75.2 -> 63.1 MiB (-16%); run-phase ops/s 1.255 M -> 1.364 M (+8.7%).
- Get digest 20d487ed1b591925 and file hashes identical in all 28 runs.
- Not merged; review/landing by lead and CI remain.
- Docs: ledger names `docs/perf/buffer-bytes.md` and `limits.md`, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
