# rocksdb-spec: journal

State: landed
Quest: .campaign/xapiand/quests/rocksdb-spec.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:d4fec9a on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/rocksdb-spec.md`: "Done. Audit findings A1–A6 in `docs/perf/rocksdb-fairness-audit.md` with raw data" |
| A2 | `docs/status/rocksdb-spec.md`: "This commit: A2's fix (strata's settle commits again only for pending merges), A3's fix" |
| A3 | `docs/status/rocksdb-spec.md`: "fb02183: the clean-room mechanisms spec." |

## Result

Landed at strata:d4fec9a on 2026-10-20. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/research/rocksdb-spec'.
- strata:531c113 adds audit instruments and fixes A1.
- strata:fb02183 adds the clean-room mechanisms spec.
- A2 and A3 fixes, audit record, raw data, and index pointers are recorded.
- The lead is to decide whether to rerun affected rows.
- Linux and FreeBSD gate-tier results remain open.
- Docs: `docs/perf/rocksdb-fairness-audit.md`, `docs/research/rocksdb-mechanisms.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
