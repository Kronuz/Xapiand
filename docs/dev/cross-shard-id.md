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

## Jobs and results

- 261111-072838-d5c6c0 (probe, first version): the choreography reached its end but the replies were not
  printed; DELETE and PUT were not 204 and a read under a held turn threw. No duplicate seen yet; not
  evidence either way.
- 261111-073927-6d9b0b: probe with replies printed, at the code before the fix (commit 33972b6f3, on the
  local branch scratch-before).
- 261111-073929-a46f52: the same probe at the fix (commit 508d1192a).

Fix (prototype): `WriteArgs::must_exist` (src/engine/shard.h), honoured by `Shard::replace`
(src/engine/strata/shard.cc); `DatabaseHandler::replace_document(did, ..., must_exist)` is used by
`replace_document_term` (handler.cc ~1949) and `index(did, ...)` (~473), which place the document
afresh when the shard answers doc_not_found.

## Results

- Before the fix (261111-073927-6d9b0b, Xapiand:33972b6f3): reproduced. After the DELETE and the new PUT,
  the stale PUT's write left the id on the hash shard 0 and on shard 1 (`id twice: hash shard 0 holds 1,
  shard 1 holds 1`); GET did not read the stale write and SEARCH did not total 2. One run, and the
  choreography is deterministic (held turns), not a rate.
- At the fix (261111-073929-a46f52, Xapiand:508d1192a): the id is on the hash shard only (`holds 1`, `holds 0`);
  the only failing check was the DELETE's status, 0 (an empty reply in this choreography, cause not
  chased; the erase did happen). That check is dropped in the next commit and one job reruns it.

Proposed fix: `WriteArgs::must_exist` (src/engine/shard.h), refused by `Shard::replace` with doc_not_found
(src/engine/strata/shard.cc:866-885) and used by `DatabaseHandler::replace_document_term`
(src/database/handler.cc:1949) and `index(did, ...)` (handler.cc:473), which then place afresh.
- 261111-085628-7b4231: corrected rerun at the fix (Xapiand:f8a2d3ac0), DELETE status unchecked; expected to pass.
