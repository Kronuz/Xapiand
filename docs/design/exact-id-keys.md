# Exact id keys: a numeric id's key is its number

> Status: BUILT on quest exact-id-keys (owner D86). A pre-release format change with no migration (D78's
> precedent, as one-id): the keys of numeric ids change on disk, so an index written before needs a dump and a
> RESTORE. It raises one-id's `MAX_NUMERIC_ID` (`docs/design/one-id.md`) from 2^32 - 1 to 2^63 - 1.

A numeric id's key is `N` followed by `sortable::encode(N)` (Kronuz/sortable, MIT, header-only, pinned in
`CMakeLists.txt`). The codec is exact for every int64, uint64 and double, and one number in any of those types is
one set of bytes, so two ids share a key only when they are the same number (5, 5u and 5.0). Before, the key was
the value codec's bytes, exact only for integers below 2^32: 4294967297 and 4311744512 had one key, and a PUT of
the second replaced the first (PLAN P2). String, keyword and UUID ids keep their keys.

Only id keys change. The id's value slot (`DB_SLOT_ID`, used by sorting and ranges on `_id`), every other value
slot and the version slot keep the value codec until sortable-encoding replaces it.

## Where an id key is built or read

Every id key is built by `Serialise::id` (`src/serialise.cc`) and read by `Unserialise::id`. The value codec is
never called for an id key.

| Place | What it does | Encoding |
|---|---|---|
| `Serialise::id(spc_id, id)` | The bytes after the type character of an id of the field's type | integer, positive, floating: `sortable::encode` of the id cast to the type (int64, uint64, double); others: `Serialise::serialise` as before |
| `Serialise::id(N)` | The bytes of the integer id N | `sortable::encode(uint64 N)`, equal to the above for an integer, positive or integral floating N |
| `Serialise::guess_id(id)` | Type and bytes of an id in an index whose id type is not set | `guess_serialise`'s type; a numeric one takes `Serialise::id`'s bytes |
| `Unserialise::id(bytes)` | The integer a numeric id's bytes name | `sortable::decode`, then `to_uint64`: none for a fraction, a negative or bytes encode never writes |
| `Schema::index` (`src/database/schema.cc`) | The key of a written document (PUT, POST with an id, RESTORE, bulk, streamed upload) | `Serialise::id`, or `guess_id` for an untyped index |
| `DatabaseHandler::get_prefixed_term_id` (`src/database/handler.cc`) | The key of every lookup by id: GET, DELETE, PATCH, UPDATE, UPSERT, versioned writes, a RESTORE's placement | `Serialise::id`, or `guess_id` for an untyped index |
| `numeric_key(N)` (`handler.cc`) | The key a PUT of the integer N names | `N` + `Serialise::id(N)` |
| `issue_id` (`src/database/shard.cc`) | The key of an issued automatic id | `numeric_key(issued)` for every placeholder type: an integer's and an equal float's key are one |
| `QueryDSL::get_regular_query` (`src/query_dsl.cc`) | A term search on a numeric `_id` | `Serialise::id`, so it builds the term a PUT builds |
| `numeric_id(key)` (`handler.cc`) | The integer a key names, for its bucket (`id_bucket`) and the counter (`raise_id_high`) | `Unserialise::id`, kept from 1 to `MAX_NUMERIC_ID`, else 0 |

`stored_numeric_id` (`shard.cc`), which raises the counter on the log's replay, reads the stored `_id` and not
the key. `key_placement_hash` hashes the key bytes as before, for ids that do not route by number.

## The counter's bound

`MAX_NUMERIC_ID` is 2^63 - 1: the largest integer every integer id type stores, since an integer field's `_id` is
an int64. The codec is exact past it, up to 2^64 - 1, so a positive id there keeps its own key; it routes by hash
and does not move the counter, as 0, negatives and fractions do. A float `_id` holds every integer only up to
2^53, so `issue_id` refuses an issued id above 2^53 for a float placeholder (`MAX_FLOAT_ID`): the POST fails as
an exhausted counter does. An explicit id of 2^63 - 1 makes every id below it taken for the counter, as 2^32 - 1
did before.

## Proof

- `test_ids_that_shared_a_key_are_separate_documents` (`tests/test_node.cc`): 4294967297 and 4311744512, 2^40 + 3
  and 2^40 + 3 * 2^24, 2^53 - 1 and 2^53 + 1, 2^63 and 2^64 - 1 stay separate through PUT, GET, a term search on
  `_id`, DELETE and RESTORE, across reopens, over 1 and 3 shards. `tests/exact_id_mutants.sh old-codec` puts the
  value codec back for keys and the test fails; `query-old-codec` does it for the term search alone.
- `test_new_numeric_ids_pass_two_to_the_thirty_two`: a counter at 2^32 - 1 issues 2^32 and on, in the shard owning
  each id's bucket, across a reopen; the cap is 2^63 - 1. `exact_id_mutants.sh cap-2-32` makes it fail.

## Not read

The value codec's source (`src/sortable_serialise.*`) and its Python port were not read for this change; ids no
longer call it. Its only use left near ids is the id's value slot, which is a value, not a key.
