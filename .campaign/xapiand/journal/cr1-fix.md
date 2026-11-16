# cr1-fix: journal

State: landed
Quest: .campaign/xapiand/quests/cr1-fix.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:6c5c1a40d on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cr1-fix.md`: "`reopen_writable` commits a replay through `Shard::commit(true, true)`" |
| A2 | `docs/status/cr1-fix.md`: "Follow-up commit: cluster step (`WALREPLAY=1 harness/cluster_check.sh`)" |
| A3 | `docs/status/cr1-fix.md`: "So `reopen_writable` commits only when `!is_transactional()`; a replay inside a transaction stays in it." |
| A4 | `docs/status/cr1-fix.md`: "Idempotence: `wal_replay_check.sh` crash, replay, crash, replay: identical state" |

## Result

Landed at Xapiand:6c5c1a40d on 2026-10-27. WAL recovery 50/50; E2E 339 requests, 576/576; ctest 20/20 after L1/L2. Review: `docs/reviews/cr1-replay-durability.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'fix/cr1-replay' (CR1: commit a WAL replay).
- Xapiand:43b7f8cf5 -- commit replay through `Shard::commit`; follow-up adds cluster check and docs.
- Reverted fix: 7 assertions fail; fixed `wal_replay_check.sh` and `wal_replay_agg_check.sh` PASS.
- ASan: engine replay 17/17; WAL replay, aggregate, and recovery checks PASS; no report.
- Cluster check: two nodes serve 40 documents and the replayed document after restart.
- Failure probe: `DatabaseError: Error writing block 0 (File too large)`; startup shards fail, next healthy start recovers.
- Open: missing COMMIT after glass commit; RESTORE can delete WAL before glass fsync; persistent `closed` retry recursion unbounded.
- Docs: `docs/dev/cr1-replay.md`, `docs/mechanisms/wal-replay.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
