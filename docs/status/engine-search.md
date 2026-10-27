# engine-search (X4, phase 0.5)

Branch `phase0.5/engine-search` from Xapiand master `1ab0ce077`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X4), D11, D13, gate row T2. Builds on
X2 and X3 (`docs/status/engine-core.md`, `docs/status/engine-query.md`).

## State

X4a (reads) is merged. X4b (search) is built and proven on `phase0.5/engine-search-b`; its last commit removes `Shard::db()` and
needs the lead's edit to the remote server before it builds ("For the lead"). X4c is planned below.

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

Done when:

1. `engine::search` and its result exist (`src/engine/search.h`): hits with id, weight, rank and percent, the
   estimate, and a request with the window, `check_at_least`, sorter, collapse, cutoffs, docid order, spy and the
   `nearest`/`fuzzy` expansions. The Xapian side owns the `Enquire`, `RSet` and `ESet` work.
2. `match`, `get_mset` and the MSet consumers (`search_views.cc`, `manager.cc`) run on it; `get_rset` and
   `get_edecider` are gone.
3. `Shard::db()` and `Shard::get_document(docid, flags)` are removed and no MIT file calls them; the remote server
   reaches the database through `native<>` with the lead's edit.
4. Unit tests under ASan+UBSan; the corpora (`qdiff.py` 2,007, `qnf.py` 104, `qread.py` 911) byte-identical to
   the baseline; docs E2E 576 of 576; `cluster_range.sh` GREEN with search over the remote protocol.
5. A live counter for each new object, a revert check per adapter, and `seam_check.sh` shrunk with new ceilings.

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
- `tests/test_engine_read.cc` (657 checks), with `engine::xapian::live_views()` counting the `DocView`s alive
  (criterion 6; review C1).

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
8. Live view counter (review C1): `test_views_do_not_leak` expects 0 live views after every `find_key`,
   `get` (found and missing), a full scan (1 inside the sink), a scan cut by a throwing sink and resumed (the
   dump retry), and a view that outlives its shard. Mutants (`.scratch/c1-mutants.sh`): `view.release()` in
   `scan` fails 6 checks (the old 591 passed it); a leaked extra view in `get` fails 33. 16 of 16 unit tests
   under ASan+UBSan after the change.
9. `seam_check.sh`: exits 0 on the tree; a new file naming `Xapian::` exits 1; a listed file gaining a line
   exits 1; shrinking asks for `--update`.
9. Seam metric (files / lines naming `Xapian::` or a Xapian header outside `src/xapian` and
   `src/engine/xapian`): master 38 / 704, X4a 38 / 701. `->db()` sites outside the remote server: 30 to 2
   (both in `match`, X4b); in the remote server 22.

Not done: TSan, libFuzzer (no decoder was added: the views read through Xapian), Linux and FreeBSD builds,
benchmarks (`scan` makes one view per document, as the dump made one document).

## Done (X4b)

Commits on `phase0.5/engine-search-b`: Xapiand:d3f914396 engine::search and tests; Xapiand:d160f6ba8 handler and
consumers; Xapiand:d4307953c the removal of `db()` and the seam ceilings (does not build until the remote edit).

- `engine::search(std::span<Shard* const>, const SearchRequest&)` returns `task<SearchResult>`; `engine::prepare`
  reads the query once (a text node is analysed there) so the handler still skips the shards for a query that
  matches nothing. The Xapian implementation (`engine/xapian/search.cc`) combines the shards into one `Database`,
  computes the relevant set and the elite set for `nearest`/`fuzzy` as the handler did, matches with the
  `KeyMakerAdapter`/`MatchSpyAdapter`, and copies the window out; the result holds no Xapian object.
- Always filled: weight, rank and percent (cheap; the ledger said optional). `Hit::percent` is an `int` as Xapian's.
- `DatabaseHandler::match` builds the request, takes `shard->engine()` of each checked-out shard (a missing index
  is skipped), and keeps the retry ladder and reopen. `get_similar` turns the field/type lists into prefixes.
- Removed: `get_rset` (handler, both), `FilterPrefixesExpandDecider`, `get_edecider`, `Shard::db()`,
  `Shard::get_document(docid, flags)`.
- Not changed: the exceptions are the same Xapian-derived `Failure` types the reads already throw.

## Proof (X4b)

Builds: ASan+UBSan, RelWithDebInfo, `-DLTO=OFF`, `.scratch/build-asan`, `quiet light`, -j4. The corpora, E2E and
cluster ran on the binary built at `Xapiand:d160f6ba8` (before the removal commit, which cannot build without the
remote edit); a `-k` build of the removal commit fails in `remote_protocol_views.cc` only (19 error lines, nothing
else). Baseline: `.scratch/xapiand-base`.

1. Unit tests: 17 of 17 under ASan+UBSan. `test_engine_search` (3,497 checks) compares every field of every hit
   and the estimate with a hand-written Xapian search over one and two shards: ranking, windows, ids across shards,
   sorter, collapse, cutoffs, docid order, spy, `nearest`, `fuzzy`, both, with and without prefixes; empty query,
   no shards, a shard of another engine (`std::bad_cast`), a wildcard error; callbacks that throw.
2. Corpora, `cmp` identical to the baseline: `qdiff.py` 2,007, `qnf.py` 104, `qread.py` 911. New corpus
   `.scratch/qsim.py` (265 answers: `nearest`/`fuzzy` n_rset, n_eset, n_term, field, type and mixes with different
   settings, windows, comments, collapse, several indexes with a missing one, stop-word text, bad parameters):
   stable across two baseline runs, new binary `cmp` identical.
3. Docs E2E: 339 requests, 576 of 576 assertions; `e2e_diff` GREEN against the baseline.
4. `cluster_range.sh 2`: GREEN, 30 checks, 0 failures. It searches from the non-owner node (the remote shards
   answer) with ranges, sort, aggregations and the new section 3e (comments, window, `nearest`, `fuzzy`, both,
   collapse). The 12 saved bodies (`cl3-base`, `cl3-new`) are `cmp` identical to the baseline run.
5. Sanitizer reports in the node logs: only the lz4, rapidjson, `pair.h` and `engine/xapian/shard.cc:72/297` lines
   the baseline prints too (X4a review N5); none from `search.cc`, `handler.cc`, `search_views.cc`; no
   `blocks on database work` line.
6. Live counter: `engine::xapian::live_search_objects()` counts prepared queries, expand deciders and search
   scopes. `test_search_does_not_leak` expects 0 after every search (one and two shards, `nearest` and `fuzzy`,
   sorter and spy), 1 while a prepared query is held and after a search cut short by a throwing key maker or spy.
   Mutants S17 (leaked decider), S18 (leaked scope), S19 (leaked prepared query) each fail 12 checks.
7. Revert checks, adapter (`.scratch/mutants5.py`, judged by `test_engine_search`): 20 of 21 caught (collapse,
   cutoffs, docid order, sorter, spy, check_at_least, elite set size, fuzzy join, prefix filter, rank, percent,
   weight, estimate, one shard only, rset size, rset window, three leaks, shard-local ids). S20 (the no-shard guard
   removed) survives: Xapian answers an empty database with an empty set, so the guard changes no output.
   Integration (`.scratch/mutants5i.py`, judged by `qdiff` + `qnf` + `qsim`): 11 of 13 caught (nearest with fuzzy's
   settings, n_term, offset/limit, spy, sorter, collapse, rank, total, percent, missing shard, one shard). Two
   survive: I2 (the lower-case type prefix: a type filter matches only terms with an empty field prefix, and no
   corpus term has one, so `type=` always answers empty) and I12 (an empty query still searches: Xapian returns the
   same empty set, and the corpora's stop-word text did not reach the early return). The first corpora run missed
   I1 and I11 (`nearest`/`fuzzy` had equal settings; no missing shard in a multi-index): `qsim.py` was added and
   catches both.
8. `seam_check.sh`: exits 0; ceilings lowered (`--update`): handler.cc 211 to 168, handler.h 43 to 24, shard.cc 55 to
   43, shard.h 13 to 11, search_views.cc 17 to 14. Files naming Xapian 38 / lines 701 (X4a) to 622.
   `Shard::db()` sites outside the remote server: 2 to 0. `MSet`, `Enquire`, `RSet`, `ESet` outside
   `src/engine/xapian`: 0, except the remote server, whose header `remote_protocol_views.h` still names
   `Xapian::Enquire` (a member and message comments) and keeps its seam ceiling (review N1).

Not done: TSan, libFuzzer (no decoder or parser was added: `prepare` calls the existing query translation), Linux
and FreeBSD builds, benchmarks, and a build and cluster run with the remote edit applied (the lead's).

X4a review notes: N1, N4, N5, N6 and N7 are untouched (N1 is the dump retry, N4 the seam counter, N5 the UBSan
downcasts, N6 the `DocView` exception contract for X4c, N7 one view per scanned document); N2 and N3 were closed in X4a.

## X4b code review (C1, N1, I2, retry ruling)

1. C1 closed. `search.h` now says how the relevant set for `nearest`/`fuzzy` is built, and `test_engine_search` has
   `test_similar_relevant_set` (4,417 checks, ASan and UBSan, all pass). The reviewer's unit mutants M2 to M5
   (`.scratch/mut_search_c1.py`, the reviewer's script re-rooted): with the new case each fails (M2 244 checks, M3 62,
   M4 3, M5 3); before it, M3, M4 and M5 survived and M2 was caught only by the older case. M5 (the relevant set keeps
   the request's cutoffs) is not reachable from the product, whose thresholds are constants (`handler.cc:1128-1129`);
   it is killed here anyway because it was cheap (a 30-document relevant set under a 90 percent cutoff).
2. Retry ruling (reviewer): the remote `get_document` needs no reopen-and-retry. It can return
   `DatabaseModifiedError` to the client, and every client path has a retry ladder. Injection proof: 5,392 throws
   injected, all 43,566 GETs answered 200.
3. N1 fixed above: the "no `MSet`/`Enquire` outside `src/engine/xapian`" claim excepts the remote header.
4. I2, pre-existing, for follow-up (not caused by X4): `nearest.type=` and `fuzzy.type=` filters never match.
   `get_similar` (the old `get_edecider` body, moved verbatim) tests `starts_with(type_char)` or its lower case, but
   every field term begins with the field's prefix, so only unprefixed terms could match. In the saved corpora every
   `nearest.type=` request answers count 0 (35 for plain `nearest`); `fuzzy.type=` answers the same as `fuzzy`
   without expansion. Either the filter should match the type character inside the prefix, or the feature should be
   dropped; `engine::Similar::prefixes` carries the question into strata. Open for the architect.

## Next

The lead applies `.scratch/remote_edits.sh`, commits the remote file, rebuilds, and reruns `cluster_range.sh 2`
(and the docs E2E) on that head: the remote server then reaches the database through `native<>`. Then X4c.

## For the lead (GPL file)

`src/server/remote_protocol_views.cc` needs, mechanically (`.scratch/remote_edits.sh` does it with `perl`, checks the
counts before and after, and adds the include):

1. All 22 `lk_shard->db()` become `lk_shard->engine().native<engine::xapian::Shard>().database()` (same
   type, `Xapian::Database*`; `engine()` opens, reopens or refuses a closed shard as `db()` did).
2. The one `lk_shard->get_document(...)` becomes
   `lk_shard->engine().native<engine::xapian::Shard>().database()->get_document(...)`. It loses the old
   method's reopen-and-retry loop and its random-error hook.
3. `#include "engine/xapian/shard.h"` after the `database/shard.h` include.

Run it from anywhere in the worktree: `sh .scratch/remote_edits.sh`. The final commit
(`Remove Shard::db()...`) does not build until that edit is committed.

## Blocked on

The remote server edit above (the lead's). Nothing else.
