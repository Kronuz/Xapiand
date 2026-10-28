# The strata engine (write slice): how it works

## What

`engine::strata::Shard` (`src/engine/strata/`) is the second implementation of the engine seam
(`src/engine/shard.h`). It keeps one shard as a prospect index (the search layout of the strata segment
core) in the shard's `strata/` directory. This slice is the write side: open and close, add, replace and
erase by docid and by key, the version check, metadata, stats, `get` by docid with its stored data, a scan
in docid order, transactions, and replay of the write-ahead log. Search, values and term lists are later
steps (S2, P1, P2).

## How

```
Schema -> engine::Document(kind)        StrataDocument: terms, positions, values, data, key
Shard::replace_document  -> engine::Shard::replace_by_key(key, doc, args)
   strata::Shard: find_key -> version -> finalise(doc, result) -> prospect::operation -> index.apply
                  (one operation per transaction; the verdict's docid and version must equal the
                   ones the hook saw, or the write fails as internal)
WAL: Document::serialise() -> line -> Document::unserialise(kind, line) -> replace(did, doc)
```

1. **One engine per index.** `engine::kind_of(path)` says which engine a shard has: its own files
   (`strata/` or `iamglass`), else `XAPIAND_ENGINE` (`xapian` or `strata`, read once, default `xapian`), and
   the registry (`.xapiand/...`) is always Xapian. `Document(kind)` is built in the engine's own form, the
   shard opens through `engine::open_local(kind, ...)`, and `DatabaseWAL` unserialises with the shard's
   kind (the `:wal` dump has no shard and takes it from the log's files). The remaining half of D8 is an `IndexSettings` field saved with the index; until then a new index
   takes the process default.
2. **The key is explicit.** `Document::set_key(term)` names the id term (D5); a Xapian document takes it as
   a boolean term, a strata document also remembers it. The index key is `\xff` followed by the term. The
   design's key prefix `Q` cannot be the index's key prefix: strata refuses any term that starts with it, and
   a field named `Quote` indexes terms `Quote.word`. No term starts with a byte that UTF-8 never produces.
   The id term stays an ordinary term too, so queries on it work.
3. **Versions.** A version belongs to a key in strata and the engine checks it itself under the shard's
   mutex, because a delete by docid carries no key and the check needs the stored version either way:
   `find_key` for a key, `locate(did)` for an id. `add` is version 1, a replace is the stored version plus 1.
   There are no `V` terms and no version slot. A document with no key has version 0 and refuses an
   expected version.
4. **Reads by id.** strata reads a record by key; by docid it reads the segments (`ordinal_of`, the live
   bit, the stored record). The shard flushes the write buffer first (`refresh`) when it holds writes, so a
   read sees every acknowledged write (D7). `get`, `scan`, `stats` and `doc_count` do that; `find_key`
   does not need to.
5. **Transactions.** A strata index has no rollback, so a transaction is a point to return to: opening a
   flushed one commits first, committing it commits, and cancelling it reopens the index from its last
   commit. An unflushed one takes the writes before it along, as Xapian does.
6. **Handles.** strata locks its directory, and Xapiand holds a writable and a readable `Shard` per
   endpoint, so the handles of one shard in a process share one index (`Handle`, `Core`); the last one to
   go commits what is pending and closes it, as a Xapian writer does when dropped.
7. **Values (P1) and term lists (P2).** `Document` holds values, WAL lines carry them, and `DocView::value`
   returns nothing until prospect's values plugin stores them (`src/engine/strata/values.h` is the one place
   to wire it, `tests/test_engine_strata.cc` `values_gap` flips then). `DocView::terms`, `term_details` and
   `serialise` raise `not_available` until the term-list plugin. Searching a strata shard raises it too (S2).

## Replay

A WAL line is `Document::serialise()`; replay is `Document::unserialise(kind, line)` then `replace(did, doc)`.
A replayed id above `last_docid` is a create (no lookup); one with a key finds its version by the key. Replay
runs at a writable open, as with Xapian, so a restart serves the committed state until a write opens the
shard; a strata shard exists from its creation, so unlike an uncommitted new Xapian shard it answers (empty)
instead of failing the read. A transaction replays inside itself and cancels with it.

Replay is idempotent per line. The index assigns a version as the stored one plus one and cannot be told a
version, so the shard stamps the version it wrote into the document before the log serialises it. A document read
back from the log with a stamp is applied only when the index is behind it: a line the index already holds, or an
older one after a newer write, changes nothing (`tests/test_engine_strata.cc` `replay`). Across restarts the
revision check in `DatabaseWAL::execute_line` against the manifest revision picks the lines to replay; pending
writes are lost with the process, so a replay always starts from the committed state. An erase has no stamp: a
replayed erase of a document that is already gone raises `doc_not_found`, and relies on that revision check.
`-DCRASH_POINTS=ON` builds let `XAPIAND_STRATA_CRASH=point:n` kill the process at the n-th pass through
`commit.before`, `commit.flushed`, `commit.after` or `replay`; `harness/strata_write_check.sh` uses them.

## Invariants

1. Only `src/engine/strata/` names strata or prospect (`harness/seam_check.sh` does not need an entry).
2. A shard is used by one task at a time; the shard's mutex serialises the handles that share an index.
3. A write that the index rejects leaves no trace; a version or key conflict is `errc::version_conflict`,
   an invalid document `errc::invalid`.
4. The stored form of a document is the engine's own (`StrataDocument::serialise`, version byte 1); a line
   that does not decode is `errc::syntax`.

## Code

| What | Where |
|---|---|
| Engine kinds, the per-shard choice | `src/engine/kind.{h,cc}`, `src/engine/open.{h,cc}` |
| Document in the engine's form, the codec | `src/engine/strata/document.{h,cc}`, `src/engine/document.cc` |
| The shard | `src/engine/strata/shard.{h,cc}` |
| Values adapter (gated) | `src/engine/strata/values.h` |
| Tests | `tests/test_engine_strata.cc` (both engines); fuzz target `tests/fuzz_strata_document.cc`; `harness/strata_write_check.sh` (a node on strata: documents by id, `:wal`, kill -9 replay) |
