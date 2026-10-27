> Status: CURRENT (2026-10-27) fix/cr1-replay from explore/cr1-replay (Xapiand:88d37dbf0); local only, not pushed; for the architect's durability check.

# cr1-fix (CR1: WAL replay must be committed)

Prototype and root cause: `docs/dev/cr1-replay.md`. Mechanism: `docs/mechanisms/wal-replay.md`.

## State

Architect: land with conditions; L1 and L2 done (reruns below). No CI run (not pushed).

## Done

1. Xapiand:43b7f8cf5: `reopen_writable` commits a replay through `Shard::commit(true, true)` (replaces the prototype's direct engine commit).
   `tests/test_engine_replay.cc`, `harness/wal_replay_check.sh`.
2. Follow-up commit: cluster step (`WALREPLAY=1 harness/cluster_check.sh`), `/tmp` removed from `wal_recovery_check.sh`, the mechanism note and this ledger.
3. Answers (decided with evidence):
   1. Transactional shards (revised after the durability review, L1). Pre-seam, a dropped writer with an open transaction discards it
      (`engine_replay`), so the accidental flush never published a replay made inside a transaction. A transactional commit is `commit_transaction`
      plus `begin_transaction`, which would publish operations replayed into a reopened transaction and stop a later cancel from discarding them, and the
      WAL cannot tell a transaction's operations from committed work. So `reopen_writable` commits only when `!is_transactional()`; a replay inside a
      transaction stays in it. A restart is never transactional, so CR1 needs no commit there. No test: the only way in is a live `Shard` reopening
      mid-transaction (a write or commit retrying through `Shard::reopen`), which needs the manager's WAL writer and a failing engine call; the engine API
      alone cannot reopen a `Shard` with its transaction flag set, and the other transaction caller is in `remote_protocol_views.cc` (clean room).
      `engine_replay` pins the engine behavior the guard rests on.
   2. Idempotence: `wal_replay_check.sh` crash, replay, crash, replay: identical state, second start replays nothing, a write between the crashes
      is replayed once on top, a third crash replays nothing. Mechanism: the glass revision decides (R+1 after the commit, the WAL's highest slot is R).
   3. Replication: the prototype's engine commit left no COMMIT line in the WAL (revision R operations without a commit for a pulling replica, whose next
      changeset fails the revision check) and sent no `DB_UPDATED`. `Shard::commit` writes the line and sends the update. `wal_replay_check.sh` asserts the line
      in all five registry WALs; it fails on the prototype binary.
   4. Failure: the error propagates; `Shard::reopen` resets the shard and rethrows after the retries. With a file-size limit the commit fails with
      `DatabaseError: Error writing block 0 (File too large)`, the node does not come up (the startup shards, `.xapiand/nodes` and the registry; index shards replay lazily and fail on the request), the next healthy start recovers everything.
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
   7. After L1/L2 (Release): `wal_replay_check.sh`, `wal_replay_agg_check.sh`, `wal_recovery_check.sh` PASS; E2E 576/576 GREEN; ctest 20/20.
   8. ASan (Apple clang, RelWithDebInfo, `ASAN=ON`, halt_on_error=1): `engine_replay` 17/17, `wal_replay_check.sh` PASS, `wal_replay_agg_check.sh` PASS,
      `wal_recovery_check.sh` PASS, no report. Not run: cluster check and E2E under ASan.

## Open (review conditions, `docs/reviews/cr1-replay-durability.md`)

1. C1: a kill after the glass commit and before the WAL writer appends the COMMIT line loses the line for good (every commit, pre-existing); a replica at R
   then fails the revision check at the first R+1 line. Repro: `.scratch/wal_hole_done4.txt` in the review worktree. Fix: repair the WAL on writable open
   (append the missing COMMIT line when glass is one past the WAL's last revision).
2. C2: a RESTORE checkin deletes the WAL while glass is not fsynced (`DB_NO_SYNC`); pre-existing durability hazard, shared by a replay commit.
3. C3: unbounded `commit -> reopen -> reopen_writable -> commit` recursion on a persistent `errc::closed` from the engine commit; bound it when the
   retry loops are next touched.

## Findings (not fixed here)

1. `test_offload_followups` aborted on the old base; master's fix (76481cc24) is cherry-picked (Xapiand:5bfc9c4da).
2. Two-node cluster, numeric aggregations are wrong with no kill at all: after 40 PUTs (and after a RESTORE of 200 documents) `_sum(age)` is 494 and 297
   on the two nodes (780 expected), `_avg` 19.6 and 18.7 for 19.5. Independent of the replay (same numbers with and without the kill). Likely the same
   class as CR1 (schema in the registry, readers on other nodes) or remote aggregation; needs its own look. The cluster step therefore checks counts and a
   document read, not sums.
3. A killed node cannot rejoin for a while ("Node name node1 already taken"); the cluster step relaunches until accepted.
4. The glass writer opens with `DB_NO_SYNC` while the WAL is on: the replay's commit is not fsynced (the WAL stays the durable record).

## Next

1. Lead: C1 to C3 are tracked in Open; none gates gate 0.5.
2. Lead: land, watch CI.

## Blocked on

Nothing.
