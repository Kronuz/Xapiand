# Document ids and versions on the write path

## What

A document's id is its key: the id's type character and its serialised value (`document_key`, `src/database/utils.cc`),
`N` and a sortable double for a number. Every write of a document with an id finds it by that key
(`engine::Shard::replace_by_key`); the docid is the engine's. A write's `?version=N` is a precondition on the
document's version, checked by the engine in the index's turn, where the write applies.

## How

```
POST (no id), numeric ids    key N<issued>               the shard issues the id (next_auto_id), then replace_by_key
PUT /i/k, numeric k          key N<k>                    replace_by_key; k advances the shard's id counter
PUT /i/k, any other id       key K<k>, U<k>, N<0>...     replace_by_key
DELETE /i/k                  key                         erase_by_key(key, expected)
```

A new document takes the docid after the shard's last, whatever its id: docids are in append order and never derive
from an id (`docs/design/one-id.md`). An id goes to one of 1,024 buckets (`id_bucket`: an integer k from 1 to
2^32 - 1 to `(k - 1) % 1024`, any other key by its hash) and the bucket to shard `bucket % n` (`bucket_shard`). A
new numeric id goes to the least used shard, which issues the smallest id of a bucket it owns above its counter and
every other shard's (`least_used_shard` passes the largest as a floor). The counter, `_id_high` in the shard's
metadata, is the largest numeric id the shard holds or issued; every numeric write raises it, the commit persists it
and the WAL's replay raises it again. A write or delete of an existing id
looks its docid up in the shard its id routes to and, on a miss, in the others, as their writes left them (`placement.md`), before the write's turn, but only
to pick the shard: the delete erases by key in that
shard's turn, so its `?version` is compared with the document the key names then, never with whatever holds the
docid.

`?version=N` reaches the engine as `WriteArgs::expected_version` (the document's version slot, `prepare`) or
`erase(..., expected)`. The engine compares it with the stored version (0: none) before anything else, and refuses
with `engine::version_conflict(current)`, whose `current_version` the HTTP error body carries as `_version`. With
N = 0 a missing document meets the precondition: PUT and UPSERT create it, PATCH, UPDATE and DELETE are 404.
PATCH, UPDATE and UPSERT read the document first and compare early; the engine compares again in the turn.

## Why

Writing by key gives every id one way in, and the id 0 is an id like any other. An id is only an id: a docid named
by an id would put ids far above the others (PUTs at 11,000,001 after a RESTORE of 1 to 4,000) millions of docids
apart, and a merge would turn the whole base segment monotone. The counter gives a new numeric id what docid = id
gave, that it is never an explicit id's, without tying the two.

The check is in the engine because only the turn orders a write's read of the version against other writes: a
check before it lets two writers that read one version both apply.

## Invariants

1. No explicit id has the key `NEW_NUMERIC_KEY`; a numeric key always has a value after its `N`.
2. A replace by docid whose key is not found never overwrites the document at that docid: it is a version
   conflict reporting 0 (the keyed document does not exist), and an unversioned write retries by key.
3. A write applied with `?version=N` lands at version N + 1; a refused one changes nothing.
4. A delete is by key in the turn, on one shard or many: the docid found before the turn picks the shard only.
5. A shard never numbers past the largest docid: the write is an error instead of a wrap to docid 0.
6. Integer ids past 2^32 - 1 can share a key: 2^32 + 1's key is 2^32 + 2^24's (the numeric serialisation's limit).
7. A new numeric id is above every numeric id of its shard and of the others' counters when it was issued, in a
   bucket its shard owns, and is written with expected version 0: a counter behind an existing id conflicts.

## Code

`src/database/schema.cc` (`Schema::index`), `src/database/handler.{h,cc}` (`has_document_id`, `numeric_id`, `id_bucket`, `next_auto_id`,
`NEW_NUMERIC_KEY`, the writes and deletes), `src/database/shard.cc` (`do_replace_term`, `do_delete*`),
`src/engine/strata/shard.cc` (`check_version`, `replace`, `replace_by_key`, `erase*`), `src/engine/error.h`
(`version_conflict`), `src/server/search_views.cc` (`on_error`). Tests: `tests/test_engine_strata.cc`
(conflicts, placement) and `tests/test_node.cc` (ids, preconditions, writers held at the turn, the model).
