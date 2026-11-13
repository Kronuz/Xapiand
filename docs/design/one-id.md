# One id: a document's id is its only id

> Status: BUILT on quest one-id (owner D84; lead D85, owner pending). A pre-release format change with no
> migration (D78's precedent): a shard's docids change meaning, ids route through buckets, and each shard
> persists an id counter.

A document's id is its only id. Strata assigns docids in append order, so every shard's docids stay dense
whatever ids users choose. Xapiand never derives a docid from an id, nor an id from a docid. Before this, a new
numeric id N took shard docid `(N - 1) / n + 1` (`args.new_did`), and an automatic id was computed from the
docid the shard assigned (`NumericId`). Under the T4/T5 mix, explicit PUTs at ids 11,000,001+ landed near docid
2.2M, and a later merge turned the whole base segment monotone, with a docid span about 540x its count
(docs/perf/restore-cap.md on explore/restore-cap, P1).

## Routing: B buckets, a table to shards

- `ID_BUCKETS` (B = 1024, `src/database/handler.h`) is the one constant.
- `id_bucket(key)`: an integer id N from 1 to 2^32 - 1 (`MAX_NUMERIC_ID`) goes to bucket `(N - 1) % B`. Any other
  id, including 0, negatives, fractions and larger integers, goes to `key_placement_hash(key) % B`. The bound is
  where the numeric key stops naming integers exactly: 2^32 + 1's key names 2^32 + 2^24, and 2^40 + 3's names
  2^40 + 3 * 2^24 (job 261113-081918-7c337f), so an id issued above it could share another id's key.
- `bucket_shard(bucket, n)` is the bucket-to-shard table. Today the table is `bucket % n`, written in one place.
  A reshard is the only thing that would write a different table, so persisting the table lands with the reshard
  quest (see "Open").
- `id_shard(key, n)` combines the two. Every new document is placed with it: by PUT, RESTORE (slices or serial),
  a streamed upload's blob, a delete of a missing id, and the automatic UUID and keyword ids that schema.cc picks
  to land in the least used shard.
- Lookups read the routed shard first: `find_docid_term` (GET, PUT, PATCH and DELETE of an existing id, version
  checks) and a RESTORE slice's placement read each key in `id_shard`'s shard, so a hit is one read
  (`DatabaseHandler::key_reads` counts them). A miss reads the other shards in order, as before: every placement
  since this quest routes by `id_shard`, but an index written under an earlier rule (numeric ids by `(N - 1) % n`,
  other ids by `hash % n`; they differ from the buckets whenever n does not divide 1,024, as with the default 5),
  or a document written straight to a shard, can hold an id off its routed shard, and cross-shard-id's handling
  (docs/dev/cross-shard-id.md) still covers it. Routed-only lookups would need such indexes ruled out.

## The id counter

Each shard keeps `id_high`: the largest integer id (1 to 2^32 - 1) it holds or has issued. A bucket's counter is
derived from it: the next id in bucket b is the smallest id above `id_high` with `(id - 1) % B == b`. So bucket
sequences keep stride B, and an automatic id is above every numeric id in its shard (D84: "a per-shard counter
that explicit numeric ids advance").

- Issue (`next_auto_id`): a POST to an index with numeric ids still goes to the least used shard, as before. The
  same pass over the shards (`least_used_shard`, one turn each, as before) also reads every shard's `id_high`
  and passes the largest as a floor. The chosen shard, in its turn, takes the smallest id above both its own
  `id_high` and the floor whose bucket it owns, then raises `id_high` to that id. So a new automatic id is above
  every numeric id in the index, not only in its shard. With one shard the ids run 1, 2, 3, ... as before. Shard s
  of n issues the ids whose bucket `% n` is s.
- Advance: every write of a numeric key (PUT, RESTORE, replay) raises `id_high` to N, so an explicit id and a
  later automatic id never collide. An automatic id is also written with expected version 0 (must not exist), so
  a counter that fell behind fails with a version conflict and never overwrites a document.
- Persist: the metadata key `_id_high` (a MessagePack integer) is written in the shard's commit, just before the
  engine commit, and only when the counter moved since the last commit. The counter is therefore durable exactly
  when the documents that raised it are. An open reads the key and never writes or syncs it. D60c holds: the
  first commit syncs what it inherited, and the counter goes with it.
- Replay: the WAL replays a write by docid. The replayed document's stored `_id` raises the counter
  (`ReplayTarget::replace_document`), so writes the last commit missed advance it again. The issued id is
  written into the record (`issue_id`) before the blobs' push, whose copy of the record the log keeps, so the
  logged record names it too.
- Exhaustion: once a shard's counter, or the floor, reaches 2^32 - 1 and no id of its buckets is left, a POST of a
  new numeric id fails (invalid), as an explicit id of 2^32 - 1 makes every id below it taken for the counter.
- Key: an issued id's key is the one a PUT of that id names: the integer encoding for integer and positive id
  fields, the float encoding when the placeholder `_id` is a float.
- RESTORE: every restored line names its id, so it advances its shard's counter like a PUT. A dump carries
  automatic ids as explicit ids, and a fresh index restored from it issues above them.

The counter is one high-water mark per shard, not 1024 stored counters. Each bucket's counter is a function of
that mark, and every id the shard writes raises all of its buckets' counters together. That is what keeps new
automatic ids above every numeric id present. A reshard that moves bucket b from shard s to shard t raises t's
mark to s's. Two shards never issue the same id, because they never share a bucket. The floor only keeps new ids
ascending across the index; a write racing the pass in another shard can still land above a just-issued id.

## Every former docid-to-id use and its replacement

| Former use | Replacement |
|---|---|
| shard.cc `NumericId` and `finalise_write`: the automatic id `(shard_did - 1) * n + s + 1`, an id derived from a docid | `next_auto_id` issues the id from the shard's counter before the write, and `issue_id` sets it in the stored data, the id slot and the key; then `replace_by_key`. `finalise_write` only stamps the version |
| shard.cc `do_replace_term`: `args.new_did = (N - 1) / n + 1`, a docid derived from an id | Removed. `replace_by_key` numbers a new document after the last docid |
| engine/shard.h `WriteArgs::new_did`, strata shard.cc `replace_by_key` placing at it | Removed, along with the `replace_docid` placement |
| handler.cc `numeric_docid(key)` | `numeric_id(key)`, used only by `id_bucket` and the counter |
| handler.cc `upload_stored`, `delete_document_term`, `replace_document_term`, bulk `apply_shards`: `(N - 1) % n`, else `hash % n` | `id_shard(key, n)` |
| schema.cc UUID and keyword automatic ids: candidate placed by `hash % n` | `id_shard(candidate, n)` |
| handler.cc `replace_document("&N")`: created a document at a docid the caller names | Removed (it had no callers) |

These stay, because they are docid handles and never ids. A search hit's multi-shard docid `(shard_did - 1) * n
+ s + 1` (`split_hit_id`, `_#docid`/`_#shard` in search, GET and INFO comments) picks a hit's shard and its
docid there. `GET` and `DELETE` of `&N` address that handle. Neither is ever turned into an `_id`.

## Open

- The bucket-to-shard table is not persisted yet. It is `bucket % n`, a function of the shard count, which an
  index never changes today. The reshard quest writes the first table that differs from it, so it persists the
  table in the index settings beside `number_of_shards`.
