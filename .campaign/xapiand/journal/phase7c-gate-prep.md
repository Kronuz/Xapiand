# phase7c-gate-prep: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-gate-prep.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:11e3d82a4 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-gate-prep.md`: "Every row is now `allow` or its code is deleted; the list has no owed row, and `--strict --owed` exits 0." |
| A2 | `docs/status/phase7c-gate-prep.md`: "Reachable over HTTP, and from the CLI. Fixed: 400 / usage error." |
| A3 | `docs/status/phase7c-gate-prep.md`: "folded in: SR1 and its root cause, `GET /`, the clean 400s and the newman preflight" |
| A4 | `docs/status/phase7c-gate-prep.md`: "`Xapiand:4e38c40a6` the dead `thread.hh` and `threadpool.hh` includes." |

## Result

Landed at Xapiand:11e3d82a4 on 2026-11-02. Release and ASan+UBSan ctest: 32 of 32; docs E2E: 641 of 641. Review: `docs/reviews/phase7c-gate-prep-security.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase7c/gate-prep' (owed rows gated; D16 restated; index paths refuse escapes, NUL and malformed bytes; root routes answer; fs quarantine fix).
- `async_check`: 244 hits, allow 231, owed 0, compiled out 13, unclassified 0.
- SR1 proof: ctest 34 of 34; docs E2E 644 of 644; path boundary check 143 ok lines.
- `Xapiand:6c820df05`, `4e38c40a6`, `c38c35ffb`, `9fcdf2dd1`, `a9efae7f9`, `a8167224c`, `bfd3431ff`, `788c3cc22`, `5fbad2b76`, `5ecbf190b` landed.
- `fs:291842c` fixes the quarantine buffer, but remains on an unpushed local branch; pinned fs still aborts `xapiand_test_quarantine`.
- Follow-ups remain for `:wal` memory/shard behavior, reserved `.xapiand/node`, and other Apple libc++ `char[N]` log arguments.
- Docs: `docs/design/phase7c.md`, `docs/mechanisms/control-plane.md`, `docs/dev/testing.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
