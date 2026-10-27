> Status: CURRENT (2026-10-27) fix/cr1-replay; bisect and root cause of CR1, fix in docs/mechanisms/wal-replay.md.
# CR1: numeric values ignored after kill -9 and WAL replay

After `kill -9` and a restart, numeric aggregations and `_sort` ignored the replayed documents'
values until the next write (`_avg(age)` 0.0 instead of 30.042 on the 1000-account bank load).

## Bisect

Repro: `harness/wal_replay_agg_check.sh` (RESTORE without commit, `kill -9`, restart, `_avg`/`_max`).
Release builds of first-parent merges between `5cce4860d` and `cca0011bb`:

| Commit | Result |
|---|---|
| Xapiand:566943e9e (engine-core) | good |
| Xapiand:2466dac5c (fix/pool-concurrency) | bad |
| Xapiand:44f628903, Xapiand:201414347 | bad |

Inside the merge, Xapiand:df87f4e1c ("Stop Shard::reopen after the first successful open") is the
trigger: it adds the missing `break` in the retry loop of `Shard::reopen` (`src/database/shard.cc`).

## Root cause

Before the `break`, `Shard::reopen` ran `reopen_writable` DB_RETRIES + 1 = 11 times (the pre-seam code
had the same missing `break`). Each open replayed the WAL into the writable database and dropped the
previous handle, whose destructor flushed the replayed operations to disk. That accidental commit
made them visible to the separate reader handles used for the registry (`.xapiand/indexes`, which
holds the index schema). With a single open, `DatabaseWAL::execute()` leaves the operations
uncommitted in the writer (`src/database/shard.cc`, `reopen_writable`); readers see an empty registry,
so the index has no schema and the numeric slots decode to nothing. The next write rebuilt the schema.
The documents were never lost.

## Fix

`reopen_writable` commits after a replay that left work pending, through `Shard::commit(true, true)`: the engine commit in the
shard's transaction mode, the endpoint revision, the WAL COMMIT line and the update notification. A failure propagates to
`Shard::reopen`. How it works: `docs/mechanisms/wal-replay.md`.

## Checks

1. `harness/wal_replay_agg_check.sh`: fails on Xapiand:2466dac5c (avg=0.0), passes on 566943e9e and with the fix.
2. `harness/wal_recovery_check.sh`: passes with the fix (50/50).
3. `.scratch/xrep.sh` bank repro: 30.042 with the fix.
4. `harness/wal_replay_check.sh`: visibility, WAL COMMIT line, crash-replay-crash-replay, a failing commit.
