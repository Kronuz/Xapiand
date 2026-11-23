> Status: in progress (quest journal-replaces-wal). Built: the prepared write, the command codec and the
> group's application. Pending: the group on the catalog slot's index (D169, strata quest group-catalog),
> ShardEndpoint's writes through `propose`, the checkpoint cadence, and the WAL's deletion.
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

## Proof

- `tests/test_group_prepare.cc`: a voter applying the leader's prepared writes from their bytes holds the same
  ids, versions, data and field table; a stale expectation is refused at apply on both; prepare writes nothing.
- `tests/test_group_command.cc`: round trips, every entry within the limit at limits around a command's size,
  refusals over the limit, and corrupt bytes at every prefix refused.
- `tests/test_shard_group.cc`: offsets from the state and idempotent effects, rotation in log order, a chunked
  command applied at its final entry, an upload into its own volume, a higher term retiring open commands,
  entries outside their id's term applying nothing, an abort retiring its upload's volume, and the state's
  bytes.
