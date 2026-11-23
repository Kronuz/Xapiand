> Status: in progress (quest journal-replaces-wal). Built: the prepared write, the command codec, the group's
> application, each index's group in its catalog slot, the writes, blobs and RESTORE through it, and the commit
> as the checkpoint. The WAL, its fsync threads and the scheduler library are deleted. Pending: deleting a group
> with its index (the index DELETE answers 501, and strata's journal pin predates `Host::delete_group`).
> Design: `docs/design/cluster-release.md` D2, D3, D12, D16, D17; strata `docs/mechanisms/replication-group.md`.

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
thread. Each index the catalog opens is a lone voter (D2) whose group borrows the slot's directory and index
(strata's group-catalog, D169):

1. **Open.** `open_payload` builds the entry, which opens the group last: the group recovers from its last
   bundle on the index as the open left it, or creates itself. Its id is the first 8 bytes of the index's
   uuid, and its journal identity the uuid.
2. **Catch-up.** Before the open returns, a linearizable read's barrier waits until the group applied what its
   log committed (a replay included), and the engine owes reads a refresh. On the chore it reads the chunked
   commands still open and the id counter's metadata. The ready point then proposes an abort for each open
   command (A9): a reopen that resumed its term with no election would otherwise leave an earlier proposer's
   upload open, its volume never freed.
3. **Writes.** Under the index's turn, `ShardEndpoint::write_document` prepares on the engine, encodes the
   prepared write as a command (`command_write`) and proposes it on the group's loop at `quorum`, chunked past
   the entry limit with an id of the term the loop sees; the answer's verdict is the write's result, checked
   against what prepare predicted (`engine::strata::Shard::landed`). A metadata write is a `set_metadata`
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
5. **RESTORE.** `Shard::apply_slice` on an index with a group (`restore_slice`): in the turn, a chore prepares
   the slice's writes, each an unconditional replace by key (`do_replace_term` leaves its `CommandWrite`, proposing
   nothing), into `RestoreBatcher`'s commands of as many writes as fit one entry; each command is proposed at
   `quorum` in input order, and `restore_results` reads its writes' docids and versions from the verdict. A write
   whose document left its placed shard is missed, as before, and takes the serial path.
6. **Commit.** A request's commit (`?commit`, the autocommit) awaits the group's checkpoint (`commit_group`):
   its bundle covers what the writes before it applied, and the group truncates its log behind it. The catalog's
   commit hook stays a plain strata commit (D169: its chore holds the index's lease, which the checkpoint takes).
7. **Close.** Every catalog close runs `close_payload` before the index's close: the group checkpoints until
   its bundle covers what it committed, and the process keeps its closed record, so a reopen resumes its term
   with no election. The catalog's commit hook and a request's commit stay plain strata commits.

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
  docid and version are its batch verdict's. `test_a_commit_is_the_groups_checkpoint`,
  `test_the_id_counter_is_in_the_log` (issued ids survive `kill -9` with no commit) and
  `test_a_reopen_aborts_an_upload_left_open` (A9's ready point). `test_a_cold_pin_reads_what_the_log_replays`,
  `test_a_fenced_index_is_replaced_and_replays_its_log` and `test_the_id_counter_persists_replays_and_advances`:
  a write whose checkpoint failed on a faulted disk is read back from the journal by the next open.
- `tests/test_shard_group.cc`: offsets from the state and idempotent effects, rotation in log order, a chunked
  command applied at its final entry, an upload into its own volume, a higher term retiring open commands,
  entries outside their id's term applying nothing, an abort retiring its upload's volume, the state's bytes, and
  `place_stored`'s two flushes: each leaves the locators viewing the bytes it built, never the ones it replaced.
