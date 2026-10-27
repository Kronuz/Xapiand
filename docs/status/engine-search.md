# engine-search (X4, phase 0.5)

Branch `phase0.5/engine-search` from Xapiand master `1ab0ce077`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X4), D11, D13, gate row T2. Builds on
X2 and X3 (`docs/status/engine-core.md`, `docs/status/engine-query.md`).

## State

X4a (reads) is built and proven; X4b and X4c are planned below.

## GPL boundary

`src/server/remote_protocol_views.cc` was never opened; `src/xapian/` and `src/sortable_serialise.*` neither.
Every map below comes from greps with `-- . ':!src/xapian' ':!src/server/remote_protocol_views.cc'
':!src/sortable_serialise*'`. For the GPL file only counts and call-site tokens were taken, with
`git grep -c` and `git grep -n -o -E '[A-Za-z_]+(->|\.)db\(\)'` (the match itself, no surrounding line).
The replication views and service are MIT (Dubalu header) and are edited freely.

## Map of `Shard::db()` (52 sites, 10 files)

| File | Sites | Uses | Moves to |
|---|---|---|---|
| `database/shard.cc` | 35 | 31 are `db();` calls that open/reopen the engine before a write or read (`engine_shard` is then used); 4 read `get_uuid()` for the data-storage headers; 8 are the read ladders (`get_docid_term`, `get_document`, `get_metadata`, `get_metadata_keys`) | `Shard::engine()` (same open/closed rule); reads through the engine (X4a) |
| `database/handler.cc` | 9 | dump x2 (postlist scan + get_document), `match` x2 (Xapian::Database for Enquire), `get_docid_term`, `get_doccount` x2 (add_document), `get_database_info` x2 (stats) | X4a except `match` (X4b) |
| `database/schema.cc` | 2 | `get_doccount` | X4a |
| `database/wal.cc` | 3 | `get_uuid`, `get_revision` x2 | X4a |
| `database/pool.cc` | 1 | `get_revision` | X4a |
| `server/discovery.cc` | 1 | `get_uuid`, `get_revision` | X4a |
| `server/replication_protocol_service.cc` | 2 | `get_uuid`, `get_revision` | X4a |
| `server/replication_protocol_views.cc` | 8 | `get_uuid`, `get_revision` only (the file copy goes through file names, not the database) | X4a |
| `server/remote_protocol_views.cc` (GPL) | 22 | all spelled `lk_shard->db()` (lines 373 410 459 491 520 647 674 903 1031 1049 1067 1086 1106 1137 1168 1195 1222 1254 1271 1300 1336 1522) | X4b: `lk_shard->engine().native<engine::xapian::Shard>().database()`; the lead applies it |
| `database/shard.h` | 1 | declaration | X4b removes it |

Other Xapian types that cross the seam outside the remote and replication code, and when they move:
`MSet`/`Enquire`/`RSet`/`ESet` (handler.cc `match`, search_views.cc, manager.cc): X4b. The 248 Xapian
`catch` lines, `Xapian::docid/rev/doccount/valueno` spellings, `exception_xapian.h`, `http_utils.h`,
`schemas_lru`, `lua/processor.cc`, `snowball_stem.h`, `stopper.*`: X4c.

## The split

### X4a: reads on the seam (this branch, first)

Done when:

1. `engine::Shard` has `stats`, `find_key`, `get`, `scan`, `metadata`, `metadata_keys` (each a task, D13),
   `engine::xapian::check(path)`, and `native<T>()`; `DocView` has `values`, `term_details` and `serialise`.
   The Xapian implementation of each is a thin call, tested against Xapian itself.
2. `Shard::engine()` opens or reopens and refuses a closed shard, as `db()` did. `Shard::get_docid_term`,
   `get_document` (now a `DocView`), `get_metadata` and `get_metadata_keys` read through the engine.
3. The dump endpoints (`dump_documents`, both forms), the database info (`get_database_info`), the document
   info (`Document::get_terms`, `get_values`, `get_value`, `get_data`, `serialise`, `hash`), `check`,
   `get_docid_term`, and every `get_uuid`/`get_revision`/`get_doccount` caller use the seam. `Shard::db()`
   remains only for `match` and the remote server.
4. `harness/seam_check.sh` exists, fails when a file outside the allowlist names `Xapian::` or a Xapian
   header or when a listed file's line count grows, and runs in CI.
5. Behaviour is unchanged: unit tests under ASan+UBSan, docs E2E 576/576 and `e2e_diff` GREEN, the query
   corpus (`qdiff.py`, 2,007 answers) and `qnf.py` (104) byte-identical, `cluster_range.sh` extended with
   dump, info, stats and check, GREEN.
6. A revert check per adapter (each new engine read and each `DocView` method fails a test when broken),
   and an ownership test with a live counter for the new view objects.

### X4b: search

`engine::search` (hits, estimated, per-hit docid, optional weight/rank/percent for `comments`), the
`Enquire`/`RSet`/`ESet` work behind it (`nearest`, `fuzzy`, collapse, spies, sorter), `match`,
`get_mset`, `get_rset` and their consumers (`search_views.cc`, `manager.cc`) on the result type. Then
`Shard::db()` is removed and the remote server reaches the database through `native<>`: the exact edit to
`remote_protocol_views.cc` is the single token above, 22 times. Done when no MIT file calls `db()` and the
lead's edit builds; the corpus is byte-identical.

### X4c: kinds and types; the seam closes

Catch ladders (about 110 lines in handler, shard, search_views, schemas_lru, manager, lua, `http_utils.h`)
move to `engine::Error` kinds, the ten raw `throw Xapian::DocNotFoundError` sites to `DocNotFound`, the
`Xapian::docid/rev/...` spellings to `engine::` types, `exception_xapian.h` over the kinds. `seam_check.sh`
allowlist shrinks to the remote and replication files (T2). Done when it exits 0 with that allowlist.

## Done (X4a)

All six criteria ticked; evidence in "Proof (X4a)".

- `engine::Shard` reads: `stats` (uuid, revision, doc count, last id, total length, length bounds, average
  length, positions), `find_key`, `get` (a `DocView`), `scan(from, sink)`, `metadata`, `metadata_keys`, the
  cheap `doc_count()`, and `native<T>()` (throws `std::bad_cast` for another engine). `engine::xapian::check`
  returns `{errors, failure}` as values, so the check endpoint's body keeps Xapian's description text.
- `DocView` gained `values`, `term_details` (term, wdf, frequency, positions) and `serialise`.
- `Shard::engine()` replaces `db()` for the opened-or-reopen-or-closed rule. `get_docid_term`,
  `get_metadata`, `get_metadata_keys` and the new `get_view` read through it. `Shard::get_document` (Xapian
  `Document`) stays only for the remote server, with `db()`.
- Migrated: `dump_documents` (both), `get_database_info`, `check`, `get_docid_term`, `Document::{serialise,
  get_value, get_data, get_terms, get_values, hash}` (the document info endpoint), and every
  `get_uuid`/`get_revision`/`get_doccount` caller (storage headers, WAL, pool, discovery, replication service
  and views, schema.cc, handler add_document).
- `harness/seam_check.sh` with `harness/seam_allowlist.txt` (per-file line ceiling, shrink-only, `--update`),
  first step of the `ci.yml` build job.
- `tests/test_engine_read.cc` (591 checks).

## Proof (X4a)

Builds: ASan+UBSan, RelWithDebInfo, `-DLTO=OFF`, x86_64, `.scratch/build-asan` (2.2 GiB, left in place),
`quiet light`, -j4. Baseline binary: the step 2 head (`xapiand-base`, answers identical to master).

1. Unit tests: 16 of 16 under ASan+UBSan; `test_engine_read` 591 checks compare every read with Xapian itself
   (stats, keys, documents, values, terms with frequency and positions, serialisation, scans from 9 starts,
   metadata with NUL bytes, check on a good and a missing path, `native<>`), a view that outlives its shard,
   and a throwing scan sink.
2. Docs E2E: 339 requests, 576 of 576 assertions GREEN; `e2e_diff` against the baseline capture GREEN;
   `bodydiff` differs in 3 volatile bodies (generated id, node/build revision, metrics).
3. `qdiff.py` 2,007 answers and `qnf.py` 104 answers: `cmp` identical to the baseline.
4. New corpus `.scratch/qread.py` (911 answers: DUMP, INFO and CHECK of 1, 3 and 5-shard indexes, INFO of
   documents 0 to 45 and missing, raw and human, GET and HEAD of every id, the metadata keys, missing
   indexes), stable across two baseline runs; the new binary is `cmp` identical.
5. `.scratch/cluster_range.sh 2` extended with section 3d (DUMP of 30 documents, INFO of the index and of a
   document, CHECK, all from the non-owner node, so the remote shards answer): GREEN, and the normalised
   bodies are `cmp` identical to the baseline run.
6. ASan/UBSan reports in the node logs of the E2E and corpus runs: only the `lz4.c` and `rapidjson` lines the
   baseline also prints; none in `src/engine`, `shard.cc`, `handler.cc`.
7. Revert checks, adapters (`.scratch/mutants4.py`): 12 of 12 caught by `test_engine_read` (stats bounds
   swapped, find_key missing answer, scan start, value slot, positions, term frequency, serialise, one
   metadata key, check text, doc count, wrong document (aborts), metadata cut at NUL). Integration
   (`.scratch/mutants4i.py`, judged by the read corpus): 6 of 6 caught (doc_del, value slot, positions,
   get_docid_term, get_metadata, dump start). The first run of the corpus missed doc_del (no 1-shard index)
   and get_metadata (no readable metadata); the 1-shard index and the key listing were added and both caught.
8. `seam_check.sh`: exits 0 on the tree; a new file naming `Xapian::` exits 1; a listed file gaining a line
   exits 1; shrinking asks for `--update`.
9. Seam metric (files / lines naming `Xapian::` or a Xapian header outside `src/xapian` and
   `src/engine/xapian`): master 38 / 704, X4a 38 / 701. `->db()` sites outside the remote server: 30 to 2
   (both in `match`, X4b); in the remote server 22.

Not done: TSan, libFuzzer (no decoder was added: the views read through Xapian), Linux and FreeBSD builds,
benchmarks (`scan` makes one view per document, as the dump made one document).

## Next

X4b: `engine::search`. Then the lead applies the remote edit below with X4b.

## Blocked on

Nothing.
