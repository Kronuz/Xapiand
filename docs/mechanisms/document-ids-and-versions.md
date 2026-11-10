# Document ids and versions on the write path

## What

A document's id is its key: the id's type character and its serialised value (`document_key`, `src/database/utils.cc`),
`N` and a sortable double for a number. Every write of a document with an id finds it by that key
(`engine::Shard::replace_by_key`); the docid is the engine's. A write's `?version=N` is a precondition on the
document's version, checked by the engine in the index's turn, where the write applies.

## How

```
POST (no id), numeric ids    key NEW_NUMERIC_KEY ("N")   eng.add: docid = last + 1, id from the docid (finalise_write)
PUT /i/k, numeric k          key N<k>                    replace_by_key, new_did = k when 1 <= k <= 2^32-1 whole
PUT /i/k, any other id       key K<k>, U<k>, N<0>...     replace_by_key, numbered after the last docid
DELETE /i/k                  key                         erase_by_key(key, expected)
```

A new key with a `new_did` takes that docid when no document holds it, else the next after the last. Multi-shard,
a new numeric id that names a docid goes to shard `(k - 1) % n` with `new_did = (k - 1) / n + 1`; other ids go
to the shard their key hashes to; a new numbered id to the least used shard. A write or delete of an existing id
looks its docid up across the shards before the turn, but only to pick the shard: the delete erases by key in that
shard's turn, so its `?version` is compared with the document the key names then, never with whatever holds the
docid.

`?version=N` reaches the engine as `WriteArgs::expected_version` (the document's version slot, `prepare`) or
`erase(..., expected)`. The engine compares it with the stored version (0: none) before anything else, and refuses
with `engine::version_conflict(current)`, whose `current_version` the HTTP error body carries as `_version`. With
N = 0 a missing document meets the precondition: PUT and UPSERT create it, PATCH, UPDATE and DELETE are 404.
PATCH, UPDATE and UPSERT read the document first and compare early; the engine compares again in the turn.

## Why

The id 0 was "no id": a new numeric id was written under `N\x80`, id 0's key, and the shard numbered any key whose
value was 0. Numeric ids also were docids, so 0, negatives, fractions and ids past 2^32 had no docid, and an id
whose docid held another document replaced it. Writing by key gives every id one way in, and `new_did` keeps the
one property docid = id gave: the ids the shard numbers, past its last docid, are never an explicit id's. An
explicit id k is at docid k unless docid k was taken when k was first written, and then k is at most the last.

The check is in the engine because only the turn orders a write's read of the version against other writes: a
check before it lets two writers that read one version both apply.

## Invariants

1. No explicit id has the key `NEW_NUMERIC_KEY`; a numeric key always has a value after its `N`.
2. A replace by docid whose key is not found never overwrites the document at that docid: it is a version
   conflict reporting 0 (the keyed document does not exist), and an unversioned write retries by key.
3. A write applied with `?version=N` lands at version N + 1; a refused one changes nothing.
4. A delete is by key in the turn, on one shard or many: the docid found before the turn picks the shard only.
5. Ids past 2^53 that round to one double share a key (the numeric serialisation's limit).

## Code

`src/database/schema.cc` (`Schema::index`), `src/database/handler.{h,cc}` (`has_document_id`, `numeric_docid`,
`NEW_NUMERIC_KEY`, the writes and deletes), `src/database/shard.cc` (`do_replace_term`, `do_delete*`),
`src/engine/strata/shard.cc` (`check_version`, `replace`, `replace_by_key`, `erase*`), `src/engine/error.h`
(`version_conflict`), `src/server/search_views.cc` (`on_error`). Tests: `tests/test_engine_strata.cc`
(conflicts, placement) and `tests/test_node.cc` (ids, preconditions, writers held at the turn, the model).
