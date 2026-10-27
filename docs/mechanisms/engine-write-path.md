# The engine write path: documents built in the engine's own form

## What

`engine::Document` (`src/engine/document.h`) is a handle over a document in the engine's own
representation. For the Xapian engine that is a `Xapian::Document` (`engine::xapian::XapianDocument`,
`src/engine/xapian/convert.h`). `Schema::index`, `GenerateTerms` and the handler call `add_term`,
`add_posting`, `add_value` and `set_data` on the handle; each call goes straight into the engine's document.
The shard writes that same object (`engine::xapian::xapian_document(doc)` in `src/engine/xapian/shard.cc`)
and the write-ahead log serialises it (`Document::serialise`, `src/database/wal.cc`).

## How

```
DocPreparer thread (parallel)      DocIndexer thread, one per shard       WAL thread
Schema::index -> Document          lock_shard -> Shard::replace_document  doc.serialise()
  (built in Xapian form)             finalise (id, version), stamp          (same object)
                                     db.replace_document(native(doc))
```

Nothing is converted. The document the parallel stage builds is the one the shard hands to the database
and the one the log serialises. The shard's write section only adds what needs the shard (id term, version
term and slot, rewritten data) through the same handle.

## Why

The first seam kept `engine::Document` as a `std::map` of terms with `std::set` positions and converted it to
a `Xapian::Document` at the database call, inside the shard's write section, and again for the log. Bulk
indexing lost 20 percent: the build ran twice and the conversion ran in the serial part. Converting ahead of
the lock (before the write, then in the parallel stage) recovered most of it; building in place removes the
second build and the conversion, so there is nothing left to move.

## Invariants

1. Each `add_*` call has the semantics of the Xapian call it replaces; an empty value clears the slot.
2. The stored and logged bytes are Xapian's own serialisation, so old log lines stay readable
   (`Document::unserialise` reads them).
3. A document has one owner at a time. It moves between threads through the indexer queue
   (`ready_queues`) and into the log task; it is never copied.
4. Only the engine that made a document reads its native form back (`xapian_document` is in the Xapian
   implementation). A second engine defines its own `Document::Native`, its `Document::Document()` and
   `Document::unserialise`.
5. `harness/seam_check.sh` is unchanged: no file outside `src/engine/xapian` names Xapian for this.

## Numbers

macOS x86_64, Release with LTO, 20000-document `RESTORE` into a fresh `--solo` node, interleaved rounds under
`quiet exclusive`. Docs per second, mean of 6 rounds (A/A pairs in parentheses):

| Binary | docs/s | user / sys CPU s |
|---|---|---|
| `5cce4860d` (before the seam) | 4761 without one 4159 outlier (4768) | 16.7 / 5.4 |
| `cca0011bb` (the seam, document converted in the shard) | 3704 | 17.9 / 5.6 |
| this change | 4635 (4627) | 16.5 / 5.0 |

This change is 1.25 times the seam and 0.97 of the parent: it recovers 87 percent of the gap, and the user
CPU is back at the parent's. A full `SEARCH` dump of the 1000-document bank and the `:info` hash are identical
to `cca0011bb`; the docs E2E is 576 of 576 assertions.

Steps on the way: converting in the indexer thread before the lock, 4466; converting in the parallel
stage, 4622; building in place, 4635 to 4680 across runs. The remaining 3 percent is not the document: the
sampled work inside the shard's write section is the same as the parent's (6499 against 6565 samples), and
the bisect (`docs/perf/gate05-index.md`) put the engine shard's `sync_wait` write layer at about 2 percent.

## Code

| What | Where |
|---|---|
| The handle and `Native` interface | `src/engine/document.h` |
| The Xapian document, `xapian_document`, `from_xapian`, `Document::Document`, `Document::unserialise` | `src/engine/xapian/convert.{h,cc}` |
| Shard writes the object | `src/engine/xapian/shard.cc` |
| Word-break terms added in place | `src/engine/xapian/text.cc` |
| Log task holds an optional document | `src/database/wal.{h,cc}` |
| Tests | `tests/test_engine_document.cc`, `tests/test_engine_shard.cc` |
