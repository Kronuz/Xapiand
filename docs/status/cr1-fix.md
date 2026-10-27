> Status: CURRENT (2026-10-27) fix/cr1-replay from explore/cr1-replay (Xapiand:88d37dbf0); local only, not pushed; for the architect's durability check.

# cr1-fix (CR1: WAL replay must be committed)

Prototype and root cause: `docs/dev/cr1-replay.md`. Mechanism: `docs/mechanisms/wal-replay.md`.

## State

Fix built and proven locally; ready for the architect's durability check. No CI run (not pushed).

## Done

1. Xapiand:43b7f8cf5: `reopen_writable` commits a replay through `Shard::commit(true, true)` (replaces the prototype's direct engine commit).
   `tests/test_engine_replay.cc`, `harness/wal_replay_check.sh`.
2. Follow-up commit: cluster step (`WALREPLAY=1 harness/cluster_check.sh`), `/tmp` removed from `wal_recovery_check.sh`, the mechanism note and this ledger.
3. Answers (decided with evidence):
   1. Transactional shards. Pre-seam, a dropped writer with an open transaction discards it (`engine_replay`: visible stays 0, then 1 after the
      transaction's own commit), so replays inside a transaction were never published; the accidental flush covered non-transactional shards only.
      Replayed operations are recovered local writes, so they are committed work: `Shard::commit` uses the shard's mode (flushed or unflushed), publishes
      them and keeps the transaction open (the plain commit is refused in a transaction, `engine_replay`). Reachable only when a live shard reopens
      mid-transaction (a restart is never transactional); not testable end to end.
   2. Idempotence: `wal_replay_check.sh` crash, replay, crash, replay: identical state, second start replays nothing, a write between the crashes
      is replayed once on top, a third crash replays nothing. Mechanism: the glass revision decides (R+1 after the commit, the WAL's highest slot is R).
   3. Replication: the prototype's engine commit left no COMMIT line in the WAL (revision R operations without a commit for a pulling replica, whose next
      changeset fails the revision check) and sent no `DB_UPDATED`. `Shard::commit` writes the line and sends the update. `wal_replay_check.sh` asserts the line
      in all five registry WALs; it fails on the prototype binary.
   4. Failure: the error propagates; `Shard::reopen` resets the shard and rethrows after the retries. With a file-size limit the commit fails with
      `DatabaseError: Error writing block 0 (File too large)`, the node does not come up, the next healthy start recovers everything.
   5. Place: `DatabaseWAL::execute()` has one caller, `reopen_writable`, which owns the open sequence, transaction and flags. Kept there.
4. Proof.
   1. Reverted fix (M0, master's block), `wal_replay_check.sh`: 7 assertions fail (visible, idempotent, new numeric field, failure); `wal_replay_agg_check.sh`:
      avg=0.0, FAIL. Fixed: both PASS.
   2. Prototype (M1, engine commit): `wal_replay_check.sh` fails the walcommit assertion only.
   3. Mutant M3 (commit error swallowed): the failure scenario fails ("no commit error in the log").
   4. Mutant M4 (`Shard::commit` unflushed without `db.commit()` in `src/engine/xapian/shard.cc`): `engine_replay` 3 of 17 fail.
   5. `wal_recovery_check.sh` PASS (50/50); ctest 19/20, E2E 339 requests, 576/576, `e2e_assert.py` GREEN.
   6. `WALREPLAY=1 harness/cluster_check.sh 2` GREEN (kill -9 node1 after PUTs, restart: both nodes serve 40 documents and read the replayed document, no
      WAL error or revision mismatch in the logs).
   7. ASan: see Next.

## Findings (not fixed here)

1. `test_offload_followups` aborts (`condition_variable wait failed`) on this base; master has its fix (76481cc24), which this branch lacks.
2. Two-node cluster, numeric aggregations are wrong with no kill at all: after 40 PUTs (and after a RESTORE of 200 documents) `_sum(age)` is 494 and 297
   on the two nodes (780 expected), `_avg` 19.6 and 18.7 for 19.5. Independent of the replay (same numbers with and without the kill). Likely the same
   class as CR1 (schema in the registry, readers on other nodes) or remote aggregation; needs its own look. The cluster step therefore checks counts and a
   document read, not sums.
3. A killed node cannot rejoin for a while ("Node name node1 already taken"); the cluster step relaunches until accepted.
4. The glass writer opens with `DB_NO_SYNC` while the WAL is on: the replay's commit is not fsynced (the WAL stays the durable record).

## Next

1. Architect: durability check of the replay commit (revision, WAL line ordering, failure path through `Shard::reopen`).
2. Lead: land, rebase on master (picks up 76481cc24), watch CI.

## Blocked on

Nothing.
