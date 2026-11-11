# Placement: where a write puts a document

## What

A write to a multi-shard index picks its shard before it takes that shard's turn: a new numeric id goes to the
least used active shard, an existing id stays on the shard that holds it, any other id goes where its numeric id
or its key's hash says (`document-ids-and-versions.md`). Placement reads every shard to decide, and reads each one
as its writes left it, buffered or flushed, without flushing it.

## How

```
DatabaseHandler                       Shard (src/database)              engine::Shard (strata)
least_used_shard()  -- per shard -->  written_count()  turn, no refresh  segments' live docs + buffered_count()
written_docid_term()-- per shard -->  written_key()    turn, no refresh  basic_index::find_key: buffers, then segments
get_docid_term()    -- per shard -->  engine_for_read() fresh: refresh   find_key on the published view
```

The written reads take the shard's turn, one shard at a time, and read the strata index from the writer's side:
`basic_index::find_key` looks in the active and frozen buffers and then the segments, with the buffers' pending
deletes applied; `written_count` adds the segments' live documents (their deletes and the pending ones taken off)
to `buffered_count()`. Neither refreshes, so neither writes a segment. Placement uses them in
`replace_document_term` (the least used shard, and the look for an existing id), `add_document`, `index` and
`index_uploaded` (an id's docid before the write) and `delete_document_term` (the shard of the key). Reads that
return a document (`get_docid_term`, GET, PATCH's read) keep the published view and its refresh.

## Why

`engine_for_read` refreshes a dirty shard so a read sees what was applied, and a strata refresh writes the buffer
out as a segment. Placement read every shard that way, so each document of a RESTORE flushed the shards the
previous documents went to: 1,000 documents into 5 shards wrote about 226 segments, each with its own I/O, and
the RESTORE waited behind any other process's fsyncs (50-60 s beside one, 2.4 s alone). Placement needs only an
id's docid and a count, which the writer's side has without a flush. The turn is what makes that side readable:
the index's flushes and merges change it only in a writer call or a chore under its lease, and the write path's
own `find_key` reads it in the turn the same way.

## Invariants

1. Placement never refreshes a shard: a RESTORE into n shards writes the segments its commit and buffer cap ask
   for, not one per document.
2. An existing id is written on the shard that holds it, whether its last write there is buffered or committed.
3. A placement read holds one shard's turn at a time and none while the write runs.
4. `written_count` counts a document rewritten within one buffer once per write: the least used shard is chosen on
   that count, which balances, not an exact live count.

## Code

`src/database/handler.{h,cc}` (`find_docid_term`, `written_docid_term`, `least_used_shard`),
`src/database/shard.{h,cc}` (`Shard::written_key`, `Shard::written_count`), `src/engine/shard.h`,
`src/engine/strata/shard.cc` (`written_key`, `written_count`). Tests: `tests/test_node.cc`
(`test_a_restore_into_shards_writes_a_few_segments`, `test_an_existing_id_written_again_lands_on_its_shard`).
