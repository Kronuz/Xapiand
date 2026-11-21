# journal-replaces-wal--restore: journal

State: blocked on the lead: the batcher's file set (msg-204); in-set work continues
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

Session 1 (2026-11-21), base Xapiand:cf65ae78b (core steps 1-3 and b). In-set work while msg-204 is open:

1. harness/profile_bulk.sh: `--put` (one PUT per document, T11), `--ack`/`--query` passed on each
   request, a journal layer row (strata::group, consensus, journal) beside the WAL's.
2. tests/test_wal_replay.cc rewritten onto the journal: the WAL test's two properties (an erase of
   a missing document is a no-op; a log applied twice ends in the primary's state) against
   ShardApplication, plus recovery's rule (file effects rerun from P_b, strata skips to P_H).
3. harness/wal_replay_check.sh, wal_replay_agg_check.sh, wal_recovery_check.sh: assertions on
   outcomes only (acknowledged writes after kill -9, identical state after a second crash, a failed
   recovery serves nothing), no WAL log lines or `:wal` endpoint.

## Next

1. On msg-204's answer: the RESTORE batcher (D17) in the granted files: pack a shard's prepared
   writes in input order within COMMAND_ENTRY_BYTES, oversized alone through split_command, the
   propose seam for step d, per-object results from the batch verdict; tests with values and a
   mutant (`--fails-without`).
2. Jobs: test_wal_replay (plain, mutant), the harness scripts at the build, profile_bulk on master's
   binary and the branch's.

## Blocked on

msg-204 (lead): where RESTORE's batcher lives. My set (src/manager.*, the WAL test and scripts,
profile_bulk.sh) holds none of RESTORE's code (handler.cc DocIndexer, shard.cc apply_slice).
Recommended (a): new src/database/restore_batch.{h,cc} + tests/test_restore_batch.cc, and
`encoded_size(const CommandWrite&)` in command.h. (b): inside command.*. Also asked: the renames
(test_journal_replay.cc, journal_replay_check.sh with the agg check folded in, wal_recovery_check.sh
deleted for levels' power_loss_check.sh), and ceding manager.* to core's step c.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | none yet |
| A5 | none yet |

## Result

<When ready for review.>

## Rulings I made

<none yet>
