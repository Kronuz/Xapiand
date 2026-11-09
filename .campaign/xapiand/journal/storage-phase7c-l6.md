# storage-phase7c-l6: journal

State: landed
Quest: .campaign/xapiand/quests/storage-phase7c-l6.md (the lead's; never edit it)

## Doing

Nothing: landed at storage:c3d3efb on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-l6.md`: "`test_roundtrips`: sizes 0..9, 4070..4110, 8170..8200, 1 MiB +-1, 16 MiB +-1, 32 MiB +-1" |
| A2 | `docs/status/phase7c-l6.md`: "`test_compat`: record bytes equal a raw `write()`; flags byte 0" |
| A3 | `docs/status/phase7c-l6.md`: "`storage_stream_big` (sparse; about 150 MB on disk): header-to-footer read-back" |
| A4 | `docs/status/phase7c-l6.md`: "`storage_stream_big`: three 1.6 GiB records, then two small ones" |
| A5 | `docs/status/phase7c-l6.md`: "`test_crash_sweep` (CrashIO): image after each append, every log prefix, torn to 512 B" |
| A6 | `docs/status/phase7c-l6.md`: "`test_abandon` (with an uncommitted record in the tail block)" |
| A7 | `docs/status/phase7c-l6.md`: "`test_reclaim`: a writer killed by `_exit` mid-upload (fork), an unheadered file" |
| A8 | `docs/status/phase7c-l6.md`: "21 of 21 killed (`.scratch/mutants.py` in the worktree)." |
| A9 | `docs/status/phase7c-l6.md`: "CI (workflow_dispatch, run 41035377548, commit `storage:a1e49db`): all 8 jobs green" |
| A10 | `docs/status/phase7c-l6.md`: "Review fixes (docs/reviews/phase7c-l6-code.md)" |
| A11 | open: not done per the ledger in `docs/status/phase7c-l6.md` |

## Result

Landed at storage:c3d3efb on 2026-10-30. the LZ4 follow-up landed as storage:e4c5746.

- Found outside the step: compressors `LZ4DecompressData::next()` heap overflow on a damaged stream, fixed in compressors `fix/lz4-bounds` (dc4474b) and pinned.
- Review: `docs/reviews/phase7c-l6-code.md` -- APPROVE WITH CONDITIONS; the architect's scoped check is named as next, not recorded.
- Docs: `docs/mechanisms/streamed-write.md`.
- Left open: A11.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
