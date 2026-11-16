# Cross-shard id races

Question (hunt row "Cross-shard id races", placement-without-flush review N3): can an off-hash
id, racing a DELETE and a PUT, end up stored on two shards?

## The interleaving

An id is placed by `numeric_docid`, else by `key_placement_hash`, but a document written under an
earlier rule can sit on another shard (the off-hash `X` on shard A; its hash shard is H). Every
write on a multi-shard index looks the id up shard by shard (`find_docid_term`, one turn per
shard, released after each) and then writes in a later turn. Nothing holds the id between them.

```
T1 PUT X    lookup finds X on A (did d)                       write replace(d) on A
T2 DELETE X                       lookup finds A, erases X on A
T3 PUT X                                        lookup misses, writes X on H
```

T1's write is by docid and the shard's `replace` creates the document when the docid is free, so
it puts X back on A while T3 put it on H: two copies. It needs a legacy off-hash document; with
the placement rules as they are now every new write of an id goes to one shard, and a stale write
on the hash shard only fails ("The key belongs to another document").

## Probe

`test_a_stale_write_does_not_put_an_id_on_two_shards` (`tests/test_node.cc`): held turns, which
queue FIFO, order the three requests (DELETE's lookup, a hold, PUT's lookup, a second hold queued
while the first is held; the DELETE's erase and the PUT's write queue behind the lookups), so the
PUT's write runs after the DELETE and after the new PUT's write (made directly on H).

## Results

- Before the fix (261111-073927-6d9b0b, Xapiand:33972b6f3): reproduced. The stale PUT's write leaves
  the id on hash shard 0 and on shard 1 (`id twice: hash shard 0 holds 1, shard 1 holds 1`); GET and
  the SEARCH total disagree. The held turns make the order deterministic, so one run decides.
- At the prototype fix (261111-073929-a46f52, Xapiand:508d1192a): the id is on the hash shard only.

The DELETE's empty reply (status 0) in both runs is the probe's, not the server's: its `?commit`
waits for a turn behind the probe's third hold, and the probe joined the DELETE before releasing
that hold, so the client's 10 s read timeout ended the call. The test checks that the DELETE has
not replied while the hold is held, joins it after the release, and asserts 204.

## Fix

`WriteArgs::must_exist` (src/engine/shard.h): `Shard::replace` (src/engine/strata/shard.cc) answers
doc_not_found when the key is no longer at the docid, and `replace_document_term` and
`index(did, ...)` (src/database/handler.cc) then place the document afresh.

Placing afresh must not carry a version read from the erased document: a PATCH whose document was
erased would overwrite a new document of the same id at the same version (new documents start at 1,
so versions often match). So the engine first checks the write as one against no document, and a
versioned write gets version_conflict and retries. Quest cross-shard-id carries the tests, each
with a job that fails without its part of the fix.
