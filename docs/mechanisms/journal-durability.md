> Status: in progress (quest journal-replaces-wal). Built: the prepared write, the command codec, the group's
> application, each index's group in its catalog slot, the writes, blobs and RESTORE through it, the commit
> as the checkpoint, and the HTTP levels. The WAL, its fsync threads and the scheduler library are deleted. Pending: deleting a group
> with its index (the index DELETE answers 501, and strata's journal pin predates `Host::delete_group`).
> Design: `docs/design/cluster-release.md` D2, D3, D4, D12, D16, D17; strata `docs/mechanisms/replication-group.md`.

# Journal durability (how it works)

Every shard is a `strata::group`: a lone voter at `rf = 1`. The group's journal is the shard's only durability
path; a write is durable once its command is in the log, and `commit()` is the group's checkpoint.

```
 request ─▶ prepare (leader, under the turn) ─▶ Command ─▶ split_command ─▶ propose ─▶ log
                                                                                   │ committed, in order
  every voter, on the group's chore:  effects (blobs, chunks) ─▶ strata write ─▶ advance (state)
```

## The prepared write

`engine::Shard::prepare(op, did, key, doc, args)` checks a write against the index as the writes left it,
resolves its id and version, finalises the document and stamps its version; it writes nothing. It returns a
`PreparedWrite`: op, id, key, and the version apply requires (`expected`, 0 for absent). `apply(prepared, doc)`
writes it. The old `add`, `replace`, `replace_by_key`, `erase` and `erase_by_key` are `engine::Shard`'s wrappers
over the two.

Apply builds the strata transaction with `engine::strata::transaction_of` (or `append_write` for a batch)
against the applying index's own field table, so a field an earlier entry declared resolves the same on every
voter, and passes `expected` to strata as the operation's `expected_version`. A write prepared against a state
that changed before it applied is then refused by strata itself (`version_conflict`), the same on every replica.
A delete by id carries no expectation (strata checks versions only by key); prepare checks it.

## The command

`src/database/command.h` (contract `xapiand:command`, `xapiand:chunked-command`). A log entry is its kind and
version byte, then:

| Kind | Fields |
|---|---|
| command | schema version, writes |
| chunk | command id, stream (staging or upload), offset, bytes to the entry's end |
| final | command id, then a command whose staged bytes are ranges of what the chunks wrote |

A write is op, flags, id, key, expected version, the document's `engine::Document::serialize()` bytes (a
metadata write's value), and its blobs: content type and record as prepare compressed it. Integers are LEB128.
A batch (RESTORE, D17) is a command of many writes in one strata transaction.

An entry is at most `COMMAND_ENTRY_BYTES` (1 MiB, consensus's `command_bytes`, D12) with its header.
`split_command` returns the command alone when it fits; otherwise it stages the largest in-place bytes into the
command's staging stream until the final entry fits, and cuts the stream into chunks. A command that does not
fit with every byte staged is refused (`invalid`), never proposed over the limit.

## The application

`src/database/shard_group.h`, `ShardApplication`, the group's application over the catalog slot's index. Its
state (`ShardGroupState`, contract `xapiand:volume-file-set`) is the current volume and its length, the next
volume number, and the open chunked commands by id with their upload volumes. A chunked command's id carries
its proposer's term in its high 32 bits (`chunked_id`). Every function reads the entry's starting state only:

1. **Effects.** An open command of a lower term is retired at the first entry of a higher one (its staging and
   upload volume). A chunk or a final placed in another term than its id names applies nothing: a leader
   change cut its command, and a deposed proposer's queued chunks must not write a retired file again. A chunk
   writes its bytes at its offset into `stage-ID` or its upload's volume (numbered at its first chunk). A
   command or a final entry writes each in-place blob record at the offset `place_blobs` names, rotating a
   volume past `VOLUME_BYTES` (32 GiB, master's volume bound), and a final reads its staged bytes back and
   retires its staging. A final whose blob names its upload seals the upload's volume; one that names none (an
   abort, a final with no writes) retires it, so an abandoned upload frees its space.
2. **Transaction.** Each write's document with its blobs' locators set from the placement, through
   `append_write`; none for a chunk, a final whose chunks were retired, or a non-command entry.
3. **Advance.** The placement's state, the chunk's open command, the final's close.

Files live in the index's directory under the file set's `app.` prefix, written positionally (a retry rewrites
the same bytes) and never unlinked by apply: the group unlinks a retired file once a bundle that omits it
publishes.

## The group of an index

`src/database/shard_group.h`, `ShardGroups` and `ShardGroup`. The process keeps one journal host over
`.xapiand/journal` in the data directory (Xapiand's own namespace, beside its system indexes) (`DatabasePool::set_journal`, called by the manager), its group loop on a
thread of its own, and two chore threads the groups apply on. The journal's IO completes inline on the loop's
thread, and its `PosixIO` takes `engine::strata::posix_io_options()`, as the indexes' do: the flush mode is
completion's `FlushMode::plain` by default and `FlushMode::full` under `--full-flush-ordering` (D177). On macOS plain
syncs with `fsync` and orders with the barrier, full with `F_FULLFSYNC` at both; the per-volume barrier warning
names the switch (D10). Each index the catalog opens is a lone voter (D2) whose group borrows the slot's
directory and index (strata's group-catalog, D169):

1. **Open.** `open_payload` builds the entry, which opens the group last: the group recovers from its last
   bundle on the index as the open left it, or creates itself. Its id is the first 8 bytes of the index's
   uuid, and its journal identity the uuid.
2. **Catch-up.** Before the open returns, a linearizable read's barrier waits until the group applied what its
   log committed (a replay included). On the chore it reads the chunked
   commands still open and the id counter's metadata. The ready point then proposes an abort for each open
   command (A9): a reopen that resumed its term with no election would otherwise leave an earlier proposer's
   upload open, its volume never freed.
3. **Writes.** Under the index's turn, `ShardEndpoint::write_document` prepares on the engine, encodes the
   prepared write as a command (`command_write`) and proposes it on the group's loop at the write's `ack` level,
   chunked past the entry limit with an id of the term the loop sees; at `quorum` the answer's verdict is the
   write's result, checked against what prepare predicted (`engine::strata::Shard::landed`). Every apply marks
   the engine (`applied_by_group`, from `ShardApplication::applied` on the chore, before the answer): reads owe a
   refresh and the index a commit, whatever the level. A metadata write is a `set_metadata`
   command. A write that moves the id counter past what the log holds carries it as a `set_metadata` write of
   its own command, so the counter is durable with the document. There is no other log: the WAL is gone, and a
   pool with no journal (a test's, the CLI's) is durable only at its commits.
4. **Blobs.** An index with a group has no data storage: its blobs are records in the group's file set. At
   prepare, `stage_blobs` compresses each new stored blob (zstd, kept only when smaller) into its write's
   `CommandBlob` and leaves the document's locator a placeholder at volume 0 with the record's codec
   (`stored` or `zstd_compressed_stored`) and size; apply places it (`place_stored`), so a locator is a function
   of the log. A blob too big to buffer streams as an upload (`upload_stored`): its chunk entries at `none` into a
   volume of its own, then the document as the upload's final entry, proposed once; a failure before it proposes
   the upload's abort. A read opens the record's volume through the slot's directory (`read_stored`): a document
   names only records its entry wrote before it was visible.
5. **RESTORE.** A RESTORE's prepared objects, plain ones and updates of an id (`update`, `merge`, `patch`, no
   script or schema), stay pending across the body's chunks (`DocIndexer::_pending`, input order). They are written
   by shard, as one write each, at the pending bound (4 MiB of document data per writing child, so each shard's
   share fills its entries), before an update of a pending id or of an id an id-less pending object may take, before
   any other stored read or a schema script's objects, and at the closing commit; the answer follows that commit, so
   every acknowledged write is applied and durable. An update reads its stored document on the loop after the
   earlier writes of its id only. `Shard::apply_slice` on an index with a group (`restore_slice`): in the turn,
   chores of 1 ms each prepare the shard's writes to the end of its sub-run, each an unconditional replace by key
   (`do_replace_term` leaves its `CommandWrite`, proposing nothing), into `RestoreBatcher`'s commands of as many
   writes as fit one entry; the last chore closes the last command and adds the id counter's write to it, so the
   counter is durable with the writes that moved it. Each command is proposed at `quorum` in input order, and
   `restore_results` reads its document writes' docids and versions from the verdict. Each proposal waits for its
   entry's syncs (two turn-log syncs, full flushes of ~25 ms each on macOS in full mode), so a proposal carries an entry's worth of
   writes. A write whose document left its placed shard is missed, as before, and takes the serial path.
6. **Commit.** A request's commit (`COMMIT`, a RESTORE's closing commit, the autocommit) awaits the group's
   checkpoint (`commit_group`): its bundle covers what the writes before it applied, and the group truncates its
   log behind it; the engine's view is then refreshed past anything applied since the capture. The catalog's
   commit hook stays a plain strata commit (D169: its chore holds the index's lease, which the checkpoint takes).
7. **Close.** Every catalog close runs `close_payload` before the index's close: the group checkpoints until
   its bundle covers what it committed, and the process keeps its closed record, so a reopen resumes its term
   with no election. The catalog's commit hook stays a plain strata commit.

## The log's bound

A shard's retained log holds at most `LOG_BYTES` (16 MiB) and `LOG_ENTRIES` (65,536) between checkpoints
(`src/database/shard_group.h`, consensus's `log_bytes` and `log_entries`). The bound is what replay re-applies
after a crash and what the group keeps in memory per open index; with millions of indexes per host, it is the
per-index cost, so it is not raised to fit a write. It bounds no single write: an entry is at most 1 MiB and a
larger command is chunked (D12); an upload streams through as many checkpoints as it needs (up to its 4 GiB
limit); a buffered body is at most 16 MiB, as before. 16 MiB is Lucene's default in-memory buffer
(`IndexWriterConfig.DEFAULT_RAM_BUFFER_SIZE_MB`), past which it flushes a segment instead of refusing a write;
Tantivy's per-thread budget is at least 15 MB and flushes the same way; Xapian flushes every 10,000 documents.
None refuses a write at its buffer, and neither does the group:

1. **Pressure.** Each placement counts its bytes and entries on the loop (`ShardGroup::placed_bytes`,
   `placed_entries`); the log's measure is what was placed less what a published checkpoint cut
   (`log_bytes()`, `log_entries()`). Past half of either bound, `ShardGroup::pressure` starts one checkpoint on
   the group's loop (never from a commit hook), holding no pin; the group's close waits for it (`quiesce`, which
   refuses a reactor or task thread). A checkpoint raises the cut to what was placed before it began, never
   lowers it, so overlapping checkpoints (a COMMIT, a writer's, the pressure one) never subtract twice.
2. **A full log.** A proposal that consensus still refuses as `LogFull` (applies behind, or a burst faster than a
   checkpoint) runs a checkpoint itself and proposes again; consensus retires the cut log's entries a
   command's worth per step, so the following attempts drain it. A log still full after that, or a refusal
   that passes with time (busy, not the leader, not ready), answers 503 naming consensus's reason; an entry
   too large or a duplicate request is a proposer's bug and answers 500.
3. **RESTORE.** An object the server left unwritten (any 5xx in its result) makes the RESTORE's own answer that
   status; its items say which objects. A client's object refused for itself (a 4xx) leaves the answer 200, as
   before.

## Refusals at open

An index refuses to open, with one error line naming its cause and the same text as the request's answer, when:

1. **Its journal lost its group.** The journal holds no group for the index (moved, deleted, or lost), and the
   index holds state: strata applied a log position, or the file set has `app.` files. A new group would
   restart the log and the file set over that state: the file set's recovery deletes the committed blob volumes,
   and new records land where old locators point. The open checks before the group is built
   (`refuse_without_journal`) and touches nothing. Recovery: put back the journal's directory this index last
   stopped with, never an older copy (below), or delete the index and RESTORE it from a dump. An index directory copied to another node is this case: its acknowledged
   writes and its group membership live in the source node's journal, so the target refuses it rather than adopt
   a directory that may have lost writes. An index moves between nodes by replication (raise
   `number_of_replicas`, or drain the source node, so a replica on the target catches up, then remove the source)
   or by DUMP/RESTORE.
2. **A volume is damaged.** strata opens the group damaged (a file shorter than its bundle names, a sealed file
   failing its CRC32C) and arms its repair, which at `rf = 1` has no peer to repair from. The open closes the group
   on its loop, which skips the checkpoints, and refuses (`refuse_opened_group`), so the whole index refuses at
   `rf = 1`; master served all but that volume's blobs. Recovery: restore the volume from a copy of the index
   taken at a clean stop and remove the damage marker (`rd` in the index's `strata` directory), or delete the
   index and RESTORE it from a dump.
3. **The index is older than its journal.** The index's directory was put back from an earlier copy while the
   journal kept running: the journal's bundle is past strata's durable head (P_b > P_H), and the log before the
   bundle is cut, so nothing can replay the gap. strata fences its own open before the file set's recovery, and
   the open refuses with strata's reason (`P_b > P_H`); the index's files are untouched, though a lone voter's
   refused open may add a term and a NoOp to the journal (journal binding's gate removes that). Recovery: put back the index as of the journal's
   last stop, or delete the index and RESTORE it from a dump.

Only the journal the index last stopped with may be put back with it. A journal older than the index (an earlier
copy, or a copy of a running data root that took the journal before the indexes) still holds the group, so the
first check passes, and recovery cuts the index's blob volumes to the older bundle's lengths: acknowledged writes
are lost and blobs read other bytes. A power cut in plain flush mode on macOS can leave the same pair: the drive
keeps strata's last commit and drops the journal's last turns. This is a known hazard until strata's journal
binding lands (strata docs/design/journal-binding.md): the index records the (position, term) of the entry it
applied last, and the open refuses, before consensus starts, a log that does not hold it. The first release
waits for it (PLAN D182).

The journal's directory holds the journal's files only. The pool refuses an index path at or inside it
(`errc::invalid`, a 400) before anything is created, with or without a create: first by name, then, on a path
whose `strata` directory does not open, by identity. Its deepest existing ancestor is made physical
(`canonical`) and it and each parent are compared with the journal's directory by device and inode, read once
at `set_journal`; so another spelling on a case-folding volume (APFS: `.Xapiand/journal/x`) or a symlink to the
journal is refused too, and a reopen of an existing index pays no added syscall. A directory found there at
start (left by a build that let an index be made there) is moved to `<journal>.strays` with a warning naming
both paths, so the host's scan never takes it for its own; its bytes are kept and its old path answers 400. A
move the volume refuses (EXDEV: the journal on another volume) ends the start naming the directory to move out.

## Levels

The HTTP levels (D4) are flags of a request's use of an index (`src/database/flags.h`), parsed in
`src/server/search_views.cc` (`write_levels`, `query_field_maker`):

| Level | Flag | Mechanism |
|---|---|---|
| `ack=quorum` (default) | none | `write_document` awaits the proposer's apply of its placement and answers the verdict. |
| `ack=none` | `DB_ACK_NONE` | Answered at `ProposalPlaced` (`202`): the result is the key, with no docid or version. The endpoint marks itself `unapplied_`, and the turn's next holder (`take_turn`) awaits `caught_up` first, so a prepare, a placement read or a commit sees the write. A RESTORE proposes its batches at `none` too, and its results carry no version. |
| `refresh=true` | `DB_REFRESH` | `finish_write` refreshes the engine (`fresh`) before the answer. |
| `refresh=wait_for` | `DB_REFRESH_WAIT_FOR` | `finish_write` awaits the write's apply (`caught_up`, at `ack=none`): a read refreshes a dirty shard first (Q-F27 (a)), so every read on the leader's replica sees an applied write. |
| `read=linearizable`, `?volatile` | `DB_LINEARIZABLE` | `Shard::engine_for_read` awaits `caught_up` (the group's `read_applied` barrier) before its refresh. |
| `?commit` | `DB_REFRESH` | `ack=quorum&refresh=true`; any other level beside it answers 400. |

`read=prefix` reads what has applied, refreshing first when the shard is dirty. The engine's marks are atomic and
cleared before their work (`Core::refresh`, `Core::commit`), so an apply on the chore that lands meanwhile marks
the shard again. A metadata write (a schema's, a setting's) is always at `quorum`. A RESTORE's refresh levels ask
nothing more of its closing commit, which refreshes.

## Proof

- `tests/test_group_prepare.cc`: a voter applying the leader's prepared writes from their bytes holds the same
  ids, versions, data and field table; a stale expectation is refused at apply on both; prepare writes nothing.
- `tests/test_group_command.cc`: round trips, every entry within the limit at limits around a command's size,
  refusals over the limit, and corrupt bytes at every prefix refused.
- `tests/test_pool.cc` (`test_an_index_is_a_lone_voter_group`): the open opens the index's group, the shutdown
  leaves it closed in the journal's table, and a second process recovers it under the same id.
  `test_an_acknowledged_write_survives_a_kill`: a write acknowledged with no commit and no close is read
  back after `kill -9` by the next process. `test_stored_blobs_survive_a_kill`: stored blobs, compressed and
  not, and an upload's blob read back byte-equal after `kill -9` with no commit and no close.
  `test_a_restore_slice_is_batch_commands`: 200 RESTORE writes take a handful of log entries, and each write's
  docid and version are its batch verdict's. `tests/test_loop_yield.cc` (`test_a_restores_writes_share_their_log_entries`):
  400 updates, 400 plain objects and 400 plain objects of numeric ids (~1.2 MB) of a RESTORE take at most 8
  entries each (2 measured; a command per body chunk was 18, one per update 400), and the numeric ids' counter
  takes no entry of its own; `test_an_update_sees_the_earlier_objects_of_its_stream`: an update of a pending id,
  and of the id a pending id-less object takes, sees that write. `test_a_commit_is_the_groups_checkpoint`,
  `test_the_id_counter_is_in_the_log` (issued ids survive `kill -9` with no commit) and
  `test_a_reopen_aborts_an_upload_left_open` (A9's ready point). `test_a_cold_pin_reads_what_the_log_replays`,
  `test_a_fenced_index_is_replaced_and_replays_its_log` and `test_the_id_counter_persists_replays_and_advances`:
  a write whose checkpoint failed on a faulted disk is read back from the journal by the next open.
- `tests/test_node.cc`: `test_ack_quorum_answers_once_applied` (a PUT waits while its apply is held),
  `test_ack_none_answers_at_placement_and_linearizable_reads_wait_for_it` (`202` while the apply is held, a
  `prefix` get misses it, a `linearizable` get waits and sees it), `test_refresh_levels_and_commit` (the shard
  is clean after `refresh=true` and `?commit`, `wait_for` at `none` waits for the apply, unknown levels answer
  400), `test_a_restore_at_ack_none_reports_no_versions`. `tests/test_pool.cc`
  (`test_a_lone_voter_reads_its_writes`): a get and a search right after each write see it.
- `tests/test_pool.cc`: `test_a_log_past_its_bound_checkpoints_instead_of_refusing` (1.5 times the bound at each
  level, a RESTORE slice and a 2.5-times upload with no commit, all read back),
  `test_an_index_inside_the_journal_is_refused` (paths inside the journal answer `invalid` and create nothing; a
  stray directory is moved aside), `test_an_index_without_its_journal_refuses_and_keeps_its_volumes` and
  `test_a_damaged_volume_refuses_its_index` (both refusals, twice each, naming their cause, the volume's bytes
  untouched). `tests/test_loop_yield.cc` (`test_a_stale_marking_failure_ends_its_slice`): a RESTORE with one
  object left unwritten answers 503.
- `tests/test_shard_group.cc` (`test_the_journal_io_takes_the_io_options`): a file the host's IO opens syncs plain
  by default and full under `--full-flush-ordering`, the options carry the barrier warning, and the IO admits a
  group-writable directory, which PosixIO's defaults refuse.
- `tests/test_shard_group.cc`: offsets from the state and idempotent effects, rotation in log order, a chunked
  command applied at its final entry, an upload into its own volume, a higher term retiring open commands,
  entries outside their id's term applying nothing, an abort retiring its upload's volume, the state's bytes, and
  `place_stored`'s two flushes: each leaves the locators viewing the bytes it built, never the ones it replaced.
