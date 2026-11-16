# journal-replaces-wal: Every shard a group at rf = 1; chunked commands, volumes in checkpoints, RESTORE; the WAL deleted

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status journal-replaces-wal`.

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

- A1 E2E and the docs suite green with every shard a group (the `build-test` and `e2e` checks)
- A2 `harness/power_loss_check.sh` replaces `wal_recovery_check.sh` (and its `run_all.sh` entry):
  a `dm-log-writes` replay at D14's sampled marks on the VM tier, 20 runs, with no acknowledged
  `quorum` write missing (jobs of at most 15 minutes each)
- A3 T11, the journal's cost against a WAL that writes: PUT from concurrent clients, with the
  journal at `ack=none` against master's WAL at its async default, and the journal at
  `ack=quorum` against master's WAL at `DB_SYNCHRONOUS_WAL` (`src/database/flags.h:42`; no request
  sets it, so a harness patch kept in `harness/` does); in each pair the journal costs at most half
  the WAL's CPU per document and of its critical-path time (`profile_bulk.sh --put`, `exclusive`
  jobs, the lead's)
- A4 Q14, throughput per acknowledged write: the journal at `ack=none` at least as fast as the
  WAL's async default, and at `ack=quorum` at least as fast as the synchronous WAL (`exclusive`
  jobs)
- A5 `git grep -n DatabaseWAL -- src` is empty (the `no-wal` check), and the WAL's tests and
  harness scripts are deleted or rewritten onto the journal
- A6 T12, RESTORE docs/s on D17's path: at `rf = 1` at least 1.0× pre-7c master (x7-gate A12's
  bar), and at `rf = 3` on three processes, stated; the profile shows the checkpoint share; a miss
  goes to the owner with its profile, not waived (`exclusive` jobs)
- A7 Checkpoint cadence (F26): during a 4 GiB streamed upload and during a RESTORE of the
  release-rows corpus, the number of checkpoints, their total time and their share of the run are
  stated; if `log_bytes` is raised for these paths, the install-size cost of the larger log is
  stated beside it (jobs)
- A8 `kill -9` at each crash point of a stored-blob write and of a chunked upload around a
  checkpoint: every acknowledged blob reads back byte-equal; a mutant that skips D16's truncation
  fails (the `crash` gate check, `--fails-without`)
- A9 Chunking: a command above 1 MiB is proposed as chunks, each entry with its header within
  `command_bytes` (1 MiB); the final entry carries the command's header, not its bytes, and applies
  it; a mutant that proposes the command whole is refused by `command_bytes` (a test with values,
  `--fails-without`)
- A10 Abandoned chunks: a voter that applies an entry of a higher term deletes the staging file and
  the volume of every open chunked command of a lower term, and the upload answers an error
  (a test)
- A11 Read-your-writes on one node: with the lead default below, a get-by-id and a search right
  after a PUT on a lone voter see the write; a mutant that serves the last periodic refresh fails
  (a test, `--fails-without`)
- A12 [decision] Read-your-writes default: pending owner (Q-F27)
- A13 [decision] The default `ack` and D54's answers: cluster-release Q1 and Q6, pending owner;
  lead defaults `quorum`, and Q6 as written
- A14 [artifact] `docs/mechanisms/journal-durability.md`

## Contracts

Provides (bytes, reviewed by `format-freeze`): `xapiand:command` (D3), `xapiand:chunked-command`
(D12), `xapiand:volume-file-set` (D16), `xapiand:restore-batch` (D17); and
`xapiand:ack-read-levels`, the HTTP semantics of `ack`, `read`, `refresh` and `?commit`. Consumes
`strata:group` and `strata:checkpoint-bundle`, and `xapiand:cluster-harness` for A2. The command
embeds `engine::Document` bytes, so `sortable-encoding`'s value encoding is inside it.

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
- D13: pre-release data directories holding a WAL are not converted; users reindex or restore
  (D87).

## May land early

None: it deletes a durability mechanism and defines bytes on disk.
