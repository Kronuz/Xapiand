> Status: CURRENT (2026-10-27) workstream fix/cr1-replay; the fix for CR1 (`docs/dev/cr1-replay.md`).

# WAL replay (how it works)

A writable local shard logs every write to its WAL before the glass commit. After a crash the next writable open
replays the lines the glass files do not hold, then commits them.

```
Shard::reopen_writable()
   open engine writer  ->  begin_transaction (only if the shard is transactional)
   DatabaseWAL::execute()      lines of revision >= the glass revision, run with wal_=false (no new WAL line)
        -> true when the last line left work pending (a trailing COMMIT line commits it itself)
   Shard::commit(wal_=true, send_update=true)
        storage_commit, engine commit (in the shard's transaction mode), endpoint revision,
        WAL COMMIT line for the replayed revision, DB_UPDATED to the replicas
```

## Why the commit

Replayed operations sit in the writer, invisible to every other handle (the registry readers, a second open of the
shard). The registry `.xapiand/indexes` holds the index schema, so without a commit a restarted node saw indexes
with no schema and numeric values decoded to nothing, until the next write committed them (CR1). The commit used
to happen by accident: `Shard::reopen` ran the open eleven times (a missing `break`), and each dropped writer
flushed its pending replay when destroyed. `engine_replay` pins that flush.

## Properties

1. **Idempotent.** The commit advances the glass revision from R to R+1 and writes the COMMIT line of revision R. The next
   open replays from revision R+1; the highest WAL slot is R, so nothing is replayed. A crash between the glass commit and
   the COMMIT line leaves the WAL without the line; the replay still skips revision R (the glass revision decides).
2. **Replicas.** A replica pulls WAL lines by revision. Operations of revision R without the COMMIT line would never
   commit on the replica, and its next changeset would fail the revision check. The commit goes through
   `Shard::commit`, so the line is written and `DB_UPDATED` is sent (not in solo mode), as for any commit.
3. **Transactional shards.** A shard with an open transaction replays inside it. A plain commit is refused during a
   transaction, so `Shard::commit` uses the shard's mode (flushed or unflushed), which publishes the replay and keeps
   the transaction open. Dropping a writer with an open transaction discards what it holds (it never committed it),
   so before this change a replay inside a transaction was never published either; it is now.
4. **Failure.** A failing commit propagates out of `reopen_writable` into `Shard::reopen`, which resets the shard
   (`engine_shard` null) and retries the open; the final failure reaches the caller. The WAL is untouched, so the
   next open replays again. No shard is left open with the replay pending. Without quorum `Shard::commit` throws
   `NotAvailable`, so a writable open waits for quorum like any write.
5. **Not fsynced.** The writer opens with `DB_NO_SYNC` while the WAL is on; the WAL is the durable record, as for every
   commit.

## Where

`DatabaseWAL::execute()` has one caller, `Shard::reopen_writable`, which owns the open sequence, the transaction and
the state flags, so the commit lives there. `execute()` only knows the shard's engine.

## Tests

1. `tests/test_engine_replay.cc`: the writer-drop flush, the commit during a transaction, a transaction dropped open.
2. `harness/wal_replay_check.sh`: visibility, the WAL COMMIT line, crash-replay-crash-replay, a failing commit.
3. `harness/wal_replay_agg_check.sh`: the CR1 repro (numeric aggregations after replay).
4. `WALREPLAY=1 harness/cluster_check.sh 2`: the restarted node and its peer serve the replayed values.
