# msgpack-unpacker-limits: journal

State: landed
Quest: .campaign/xapiand/quests/msgpack-unpacker-limits.md (the lead's; never edit it)

## Doing

Nothing: landed at msgpack:88aab56 on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/unpacker-limits.md`: "`unpack.hpp` put back to `origin/main`: `test_hostile_headers_one_chunk`, `_one_shot`" |
| A2 | `docs/status/unpacker-limits.md`: "`tools/unpack_mutants.py`: 13 edits (reserve the declared size, claims ignored, claims never released" |
| A3 | `docs/status/unpacker-limits.md`: "11 of 11 ctest targets pass (after the SR1 change too)" |
| A4 | `docs/status/unpacker-limits.md`: "300 s, 3,224,200 runs on the first version and 999,795 on the SR1 version" |
| A5 | `docs/status/unpacker-limits.md`: "`harness/restore_stream_check.sh` all six rows ok; `harness/run_all.sh e2e` VERDICT GREEN" |
| A6 | `docs/status/unpacker-limits.md`: "SR1 and SR3 are folded in below, SR2 is Xapiand's `fix/restore-depth-limit`." |
| A7 | open: Next 2, no measurement recorded in `docs/status/unpacker-limits.md` |

## Result

Landed at msgpack:88aab56 on 2026-11-03. Fix msgpack:5ee7adc; landed through msgpack:88aab56 (2026-11-03) with the review re-check.

- Per input byte: streamed 23 to 47, in one piece 32 to 70; asserted bound 96 (4 x sizeof(object)).
- Review: `docs/reviews/unpacker-limits-security.md` -- APPROVE WITH CONDITIONS, re-check recorded at msgpack:88aab56.
- Follow-ups named: bump Xapiand's msgpack pin; item 3 of Xapiand `docs/status/phase7c-linux-gate-fixes.md`; the cpp-libraries upgrade note.
- Docs: `docs/mechanisms/unpacker-reservation.md`, `CHANGELOG.md`.
- Left open: A7.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
