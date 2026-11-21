# journal-replaces-wal: Every shard a group at rf = 1; chunked commands, volumes in checkpoints, RESTORE; the WAL deleted

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status journal-replaces-wal`.

Whole feature (replan-2026-11-20, section 1 F4). Carries `barrier-warning`'s switch onto the journal.
A single node ships on it alone: the durability half of the cluster, landed before the cluster.

## Outcome

Every Xapiand shard is a `strata::group`, a lone voter at `rf = 1`, and the journal is its only
durability path (design D2): `src/database/wal.*` and `src/xapiand_fsyncher.h` are deleted, and
`commit()` becomes the group's internal checkpoint. The leader prepares one command per write (D3:
op, id, expected version, schema version, `engine::Document` bytes, blobs), and every voter applies
it in log order, the `DataStorage` push moving from prepare to apply. A command whose encoding
exceeds 1 MiB is chunked (D12). Volume numbers are a function of the log and the checkpoint names
them (D16). RESTORE proposes batch commands through the group (D17). The HTTP levels `ack`
(`quorum`, `none`), `read` (`prefix`, `linearizable`), `refresh` and `?commit` (= `ack=quorum&
refresh=true`) ship with their semantics (D4).

## Acceptance criteria

Gate: the suite (`build-test`), the process tier (`e2e`), the crash model (`crash`, because this
changes a durability mechanism: "Direct path first"), `no-wal` and CI (`asan`, `tsan`).

- A1 Every shard is a lone-voter group, E2E and the docs suite are green, `git grep -n DatabaseWAL --
  src` is empty (the `no-wal` check), and the WAL's tests and harness scripts are deleted or
  rewritten onto the journal (the `build-test` and `e2e` checks)
- A2 `kill -9` at each crash point of a stored-blob write and of a chunked upload around a
  checkpoint: every acknowledged blob reads back byte-equal; a mutant that skips D16's truncation
  fails (the `crash` check, `--fails-without`)
- A3 A command above 1 MiB is proposed as chunks, each entry with its header within `command_bytes`
  (1 MiB), the final entry carrying the header and applying it, and a mutant proposing it whole is
  refused; a voter applying an entry of a higher term deletes the staging file and the volume of
  every open chunked command of a lower term and its upload answers an error: tests with values, the
  mutant a `--fails-without` job
- A4 Read-your-writes on one node: a get-by-id and a search right after a PUT on a lone voter see
  it; a mutant that serves the last periodic refresh fails (Q-F27's lead default (a); a test,
  `--fails-without`)
- A5 `ack`, `read`, `refresh` and `?commit` behave as D4 says and are documented with docs E2E
  examples; RESTORE goes through the group (D17); `--full-flush-ordering` (off by default, D10)
  reaches the journal's IO and the per-volume barrier warning strata reports is logged once with the
  switch's name, the option documented and the changelog naming both: tests and the `e2e` check
- A6 [decision] Read-your-writes default (Q-F27) and the default `ack` (Q1 and Q6): lead defaults
  `quorum` and Q6 as written, pending the owner; `docs/design/cluster-release.md`
- A7 [artifact] `docs/mechanisms/journal-durability.md`

## Tasks

One agent each, merged into the quest branch on their own tests. Strong, high (durability).
`src/database/{shard,handler,pool}.*` are shared, so the tasks that touch them run in order.
`pool-from-memory` (a quick fix on `pool.*`) lands on main before the first of them.

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `core` | `src/database/{shard,wal,pool}.*`, `src/database/flags.h`, `src/xapiand_fsyncher.h`, `src/xapiand_storage_io.h`, `tests/test_group_*.cc`, `tests/test_pool.cc` | implementation, strong, high | Every shard a lone-voter group; the leader's one command per write (D3), apply in log order; `commit()` as the checkpoint; the WAL and the fsyncher deleted (A1). Builds against `group-core`'s contract with `--ref strata=../strata@quest/xapiand/group-core` |
| 2 | `chunks` | `src/database/handler.*`, `tests/test_group_chunk*.cc`, `harness/blob_crash_check.sh` | implementation, strong, high | Chunked commands over 1 MiB, abandoned-chunk cleanup, volume numbers from the log and the checkpoint (D12, D16), the crash points (A2, A3) |
| 2 | `restore` | `src/manager.*`, `tests/test_wal_replay.cc`, `harness/{wal_recovery_check,wal_replay_check,wal_replay_agg_check}.sh`, `harness/profile_bulk.sh` | implementation, strong, high | RESTORE as batch commands (D17); the WAL's tests and scripts deleted or rewritten onto the journal (A1, A5) |
| 3 | `levels` | `src/opts.*`, `src/server/search_views.cc`, `tests/test_reactor_requests.cc`, `tests/test_wide_search.cc`, `tests/test_engine_error.cc`, `harness/{power_loss_check,run_all}.sh`, `harness/README.md`, the docs pages | implementation, strong, high (docs economical, medium) | `ack`, `read`, `refresh`, `?commit`, read-your-writes, `--full-flush-ordering` onto the journal's IO and its warning (A4, A5); `harness/power_loss_check.sh` replaces `wal_recovery_check.sh` |

The quest's own `files` set is the union; the lead narrows each task's globs at `campaign start`.

## Contracts

Provides (bytes, reviewed by `format-freeze`): `xapiand:command` (D3), `xapiand:chunked-command`
(D12), `xapiand:volume-file-set` (D16), `xapiand:restore-batch` (D17); and
`xapiand:ack-read-levels`, the HTTP semantics of `ack`, `read`, `refresh` and `?commit`. Consumes
`strata:group` and `strata:checkpoint-bundle`, and `xapiand:cluster-harness`. The command embeds
`engine::Document` bytes, so `sortable-encoding`'s value encoding is inside it.

## Decisions

- Design D2, D3, D12, D13, D16, D17; F1, F2, F5; F10 (this file set covers every WAL user,
  `run_all.sh` and the harness tooling included).
- Design D4: get-by-id at `linearizable` reads the live version map through `group::read_applied`
  with no refresh (strata `docs/REPLICATION.md:134`); a search pays a refresh that covers that
  index. `?volatile` means `read=linearizable`.
- Q-F27, pending owner; lead default (F27 option a): master's read-your-writes is kept where the
  reader holds the leader's replica: a read refreshes a dirty shard first, which a lone voter
  always is (master: `src/engine/strata/shard.cc:311-316`, `:1555`). `prefix` keeps its D10
  meaning on followers. Options (b) document the narrowing, (c) default `refresh=wait_for`.
- D13: pre-release data directories holding a WAL are not converted; users reindex or restore (D87).
- Risk lowered from critical to high (owner 2026-11-20): bytes are pre-release (D30, D87), the crash
  model still gates, and `format-freeze` stays critical.
- Starts now, `after group-core` (it builds against `group-core`'s quest-file contract); lands once
  `group-core` is on strata's main and pinned (the lead's `campaign pin --push`). Without
  `CMakeLists.txt`: tests are globbed.

- Lead, 2026-11-21 (msg-204, msg-205): RESTORE's batcher (D17) lives in new files
  `src/database/restore_batch.{h,cc}` (task restore), with `encoded_size(const CommandWrite&)` added
  to `command.h` so packing encodes each write once. RESTORE's writes are prepared as `replace_key`
  with no expected version, as RESTORE overwrites today (`replace_document_term(..., version=false)`):
  docids and versions are assigned at apply and read back per op from the batch verdict
  (`docids[i]`), so batches pipeline per shard without depending on an earlier batch's apply.
  The application's `transaction()` keeps a batch write's version from the document. The WAL's
  names go: `tests/test_journal_replay.cc`, `harness/journal_replay_check.sh` (folding
  `wal_replay_agg_check.sh`), and `wal_recovery_check.sh` is deleted (levels' power_loss_check.sh
  replaces it). `src/manager.*` moves from task restore to core, for the group loop and the
  journal host (step c). Wiring the batcher into `DocIndexer::apply_shards` and deleting
  `Shard::apply_slice` is core's step d.

## When done

Runs once, when the feature is complete and before it lands; findings are fixed on the quest branch,
each with a test that fails without it. A measurement miss goes to the owner with its profile.

- Review: one reviewer, lens `durability`, carrying correctness.
- `--later`: `harness/power_loss_check.sh`, `dm-log-writes` at D14's sampled marks, 20 runs on the
  local Linux QEMU VM under host holds, each at most 15 minutes, no acknowledged `quorum` write
  missing.
- Measurements, one `exclusive` job of at most 10 minutes each on the macOS host: T11 (the journal's
  CPU and critical path per document against the WAL: `ack=none` against master's async default,
  `ack=quorum` against `DB_SYNCHRONOUS_WAL` via a harness patch; at most half the WAL's CPU per
  document and of its critical-path time, `profile_bulk.sh --put`), Q14 (throughput per
  acknowledged write, at least as fast as the WAL in each pair), and T12 (RESTORE docs/s on D17's
  path at `rf = 1` at least 1.0x pre-7c master; `rf = 3` on three processes stated, the profile
  showing the checkpoint share). Checkpoint cadence (F26) during a 4 GiB upload and a RESTORE of the
  release-rows corpus, as a light job: counts, total time, share, and the install-size cost if
  `log_bytes` is raised. The cluster gate's R7 cites these.

## May land early

None: it deletes a durability mechanism and defines bytes on disk.
