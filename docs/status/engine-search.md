# engine-search (X4, phase 0.5)

Branch `phase0.5/engine-search` from Xapiand master `1ab0ce077`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X4), D11, D13, gate row T2. Builds on
X2 and X3 (`docs/status/engine-core.md`, `docs/status/engine-query.md`).

## State

X4a (reads) and X4b (search) are merged on master. X4c (kinds and types; the seam closes) is built and proven on
`phase0.5/engine-search-c`: commits Xapiand:b28875f07, Xapiand:94c212d24, Xapiand:01d8b6ccb and the proof commit
that holds this text. The code review (`docs/reviews/engine-search-x4c-code.md`, APPROVE WITH CONDITIONS) is folded in at
Xapiand:84e7e00f9 (C1, C2, N1, N5; see "X4c code review" below). `remote_protocol_views.cc` was not opened or edited; the
lead's task for X4c is only to land it.

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

Plan (phase 1, awaiting the lead's ruling). Inventory by greps scoped as in "GPL boundary"; the GPL file by
`git grep -c` and match-only tokens.

#### 1. Inventory

Catch lines naming a Xapian type, outside `src/xapian`, `src/engine/xapian` and the GPL file: **176** in 12 files
(handler.cc 106, shard.cc 27, schemas_lru.cc 9, http_utils.h 8, search_views.cc 6, replication_protocol_views.cc 6,
manager.cc 5, index_resolver_lru.cc 3, main.cc 2, lua/processor.cc 2, replication_protocol_service.cc 1,
discovery.cc 1). The ledger's 248 counted all `catch` lines naming a Xapiand or Xapian error type (258 with
`engine::Error`). The GPL file has 6 `catch` lines, 3 naming Xapian (`Error`, `NetworkError`, `NetworkTimeoutError`) and
42 lines naming `Xapian::...Error` (throws and catches); it keeps them (floor, below).

By type: DatabaseNotFound 23, NetworkTimeout 21, Network 21, DocNotFound 19, DatabaseOpening 19, DatabaseClosed 19,
DatabaseError 18, DatabaseModified 16, DatabaseNotAvailable 8, Xapian::Error 7, InvalidArgument 2,
DocVersionConflict 2, QueryParser 1. By shape (what the code does):

| Shape | Sites | Where |
|---|---|---|
| A. Read retry ladder: Modified/Opening/Timeout/Network retry (close on the last), Closed closes then retries, DatabaseError closes and throws; then `reopen()` | 13 | handler.cc 807 865 2432 2484 2536 2585 2655 2709 2823, shard.cc 1686 1728, handler.cc 1541 (the match ladder, plus QueryParser/Serialisation to `ClientError` 400) |
| A'. Same without the Modified arm (Modified falls into DatabaseError: close, throw) | 6 | handler.cc 1345 1387 1433 1939 (collect-and-continue: store `eptr`, `--valid`), shard.cc 1783 1827 (metadata reads) |
| B. Missing is data: swallow DocNotFound and/or DatabaseNotFound | 20 | handler.cc 303 452 513 565 992 1011 1244 1622 1735, schemas_lru.cc 278 343 517 526 623 632, manager.cc 720 867, index_resolver_lru.cc 458, search_views.cc 1307 1354 (rethrow unless `is_root`) 1978 2111 |
| C. Missing becomes a client error | 2 | lua/processor.cc 253 (400 "Foreign script ..."), http_utils.h |
| D. NotAvailable: remember and retry or give up | 5 | handler.cc 1252, manager.cc 859, discovery.cc 551, replication_protocol_service.cc 118, replication_protocol_views.cc 336 |
| E. HTTP mapping | 1 (8 arms) | `http_utils.h catch_http_errors` |
| F. Log the description and carry on | 6 | main.cc 1074 1126, shard.cc 859, handler.cc 2857, replication_protocol_views.cc 445, http_utils.h `Xapian::Error` arm |
| G. Replication-specific (NotFound, Network, Timeout, version) | 4 | replication_protocol_views.cc 425 435 741 |

Retry-ladder variance found (A vs A'; and Modified on the last try): `DatabaseModified` retries and closes on the last
try in handler.cc, retries and does NOT close in shard.cc `get_docid_term`/`get_view`, and does not retry (close, throw)
in the six A' sites. `DatabaseNotAvailable` derives from `DatabaseOpeningError` (as does NotFound; checked by a compile-time
`is_base_of` probe, `.scratch/x4c/hier.cc`, no GPL source read), so the ladders already treat it as "opening".

Raw throws of Xapian types outside the engine: `throw Xapian::DocNotFoundError` x8 (handler.cc 1572 2414 2466 2518 2568
2619 2689 2793), `Xapian::NetworkError` x4 (manager.cc 893-904), `Xapian::InvalidArgumentError` x2 (generate_terms.cc 578,
snowball_stem.h 38), `Xapian::AssertionError` x2 (lock.h 69, manager.h 273).

Other files that still name Xapian, with what is left in each after the catches go:

| Group | Files (lines) | Moves to |
|---|---|---|
| Spellings `Xapian::rev` 108, `docid` 74, `termcount` 19, `doccount` 5, `valueno` 2 | handler.cc/h, shard.h/cc, wal.cc/h, pool.cc/h, schemas_lru.cc/h, index_resolver_lru.h, discovery.cc/h, search_views.cc, main.cc | `engine::rev/docid/...` (already in `engine/types.h`); sed |
| Include `xapian.h` only for those types | lock.h, manager.h, schema.h (a comment), node.cc, wal.h, ... | delete the include; `engine/types.h` |
| `Xapian::TermGenerator` (cjk_words under `USE_ICU`), `Stopper`, `Stem` | generate_terms.cc 16, stopper.cc/h 6, snowball_stem.h 7 | `src/engine/xapian/text.{h,cc}` (`index_text_word_breaks`), `stopper`/`snowball_stem` Xapian halves move there; `getStopWords` (MIT) stays |
| `xapian/common/serialise-double.h` (GPL header) | length.h (unused include), metric_serialise.h | length.h: drop; metric_serialise.h: `engine::serialise_double` (exists in `engine/codec.h`) |
| `Xapian::major_version()` banner | main.cc 752 | `engine::xapian::version()` string |
| `exception_xapian.h` | 17 | see 4 |
| the wrapper `src/xapian.h` | 34 | move to `src/engine/xapian/xapian.h`; the remote files' include line changes (the lead's script) |

Floor after X4c (what `seam_check.sh` keeps): `remote_protocol_views.cc` 125, `remote_protocol_views.h` 5,
`remote_protocol_service.h` 1, `replication_protocol_views.h` 2 (the `Xapian::rev` members move: 0), and
`replication_protocol_views.cc` 7 -> 1 if its six catches move (they are MIT: I recommend moving them). T2 reaches 0
for every file outside the remote server (plus the one replication include); it cannot reach 0 with the GPL file in
the tree, which is the floor the ledger already named.

#### 2. Kinds and mapping

The 13 kinds of `engine::errc` already cover every Xapian type; X4a/X4b built `Failure<Kind, XapianType>` for each. One
addition and one cut:

1. Add `errc::syntax`: a query or serialised input the engine cannot parse; HTTP 400. Needed because `Xapian::QueryParserError`
   (client error 400 today, via handler.cc 1541) and `Xapian::InvalidArgument/InvalidOperation` (500 today) are both
   `invalid`; merging them would turn today's 500s into 400s. `exception_xapian.h QueryParserError` and the translation of
   `Xapian::QueryParserError` (today passed through raw by `rethrow_translated`) become `syntax`.
2. `Error` gains `description()` (the full text `get_description()` gave, for logs and 500 bodies) and `detail()` (the OS
   string `get_error_string()` gave; the network arm of `catch_http_errors` switches on it). A constructor with both.
   `Failure(const X&)` fills them from the original. Any other `Xapian::Error` (Range, Unimplemented, Assertion, Internal)
   is translated by `rethrow_translated` to `Failure<internal, Xapian::Error>` (copy-constructed) so no raw Xapian
   exception leaves the adapter.

| Xapian type | Kind | Retryable (read ladder) | HTTP today -> after (unchanged) |
|---|---|---|---|
| DatabaseModified | reopen | yes (close on last) | 503 |
| DatabaseOpening (not Not*) | opening | yes (close on last) | 500, description |
| DatabaseNotFound | not_found | yes in ladders (as Opening); missing-is-data in B | 404 |
| DatabaseNotAvailable | not_available | yes (as Opening); D remembers it | 503 |
| NetworkTimeout | timeout | yes (close on last) | 504 |
| Network | network | yes (close on last) | 502 for 4 strings (via `detail`) else 500 |
| DatabaseClosed | closed | close then retry; throw on last | 500, description |
| DatabaseCorrupt, other DatabaseError | corrupt, database | no: close, throw | 500, description |
| DocNotFound | doc_not_found | no; missing-is-data in B | 404 |
| DocVersionConflict | version_conflict | no (the write ladders retry it) | 409 |
| InvalidArgument/Operation | invalid | no | 500 (400 when also a `ClientError`) |
| QueryParser, Serialisation (Xapian's) | syntax | no | 400 |
| anything else | internal | no | 500, description |

Policy lives in `engine/error.h`, not in each file: `constexpr bool is_missing(errc)` (doc_not_found, not_found) for B,
and `enum class fate { retry, retry_close, close_retry, close_fail, fail }` with `constexpr fate read_fate(errc)`, used by
one `Shard::retry_read(const engine::Error&, int t)` (the read twin of `retry_or_throw`, which stays for writes). A ladder
becomes `catch (const engine::Error& e) { lk_shard->retry_read(e, t); }`, 13+6 sites of 7 lines to 1.

Decision for the lead (D1): the three Modified variants. Recommendation: unify on the majority (retry, close on the last
try). It changes behaviour only when a snapshot goes stale on exhausted retries: shard.cc stops skipping the close, and the
six A' sites retry a stale snapshot instead of closing and throwing the 503 to the client (a fix; no corpus reaches it).
The exact-preserving alternative is a `modified` parameter on `retry_read`; I will use it if the lead prefers.

#### 3. N6: DocView exceptions

Recommendation: yes. Every `DocView` method (`did`, `data`, `value`, `version`, `terms`, `values`, `term_details`,
`serialise`) catches `(...)` and calls `rethrow_translated()`, as every `Shard` method already does; `doc_view.h` states
"throws `engine::Error` only". The view holds a lazy `Xapian::Document`, so `data()`/`value()` can still hit the database
(a remote shard, or a block overwritten under the snapshot) after `get()` returned: Modified, Network, Timeout. The retry
ladders call `doc->serialise()` inside the `try` (handler.cc 2432 and the rest of A), so without this a raw Xapian
exception would slip past `catch (const engine::Error&)`. `Shard::scan` already translates sink exceptions; a sink that
throws a non-Xapian exception passes through unchanged. Zero cost on the happy path. The ladders recognise a retryable
kind through `read_fate(e.kind)`, nothing else. Test: a view over a document whose database is closed/modified under it
throws `engine::Error` of the right kind from each method.

#### 4. HTTP statuses and messages stay identical

`catch_http_errors` keeps its arm order. One `catch (const engine::Error& e)` replaces the Xapian arms, at the first
Xapian arm's position, and switches on `kind`: doc_not_found/not_found -> 404 "Not Found: " + `reason()` (=`get_msg()`);
version_conflict 409; not_available and reopen 503; timeout 504; network -> the `detail()` string table (502 or 500 +
`L_EXC`); syntax 400 `what()`; the rest -> 500 `description()` + `L_EXC`. An `engine::Error` that is also a
`ClientError` (the `exception_xapian.h` mixins) is answered 400 with `what()` via `dynamic_cast<const ClientError*>`, as
today (the ClientError arm sits before `Xapian::Error`); `DocVersionConflictError` and `NotAvailable` keep their own
status first. `Datetime`/`Cartesian`/`Missing` arms are untouched. Proof in phase 2:

1. A unit test throws each Xapian type through `rethrow_translated` and compares `reason()`, `description()`, `detail()`
   with `get_msg()`, `get_description()`, `get_error_string()` of the original (one table, 13 rows).
2. `catch_http_errors` against the old function copied into the test: same (status, body) for 20 exception values.
3. The four corpora and the docs E2E byte-identical, including error responses (404s, 400s, 409s in `qnf`/E2E);
   `cluster_range.sh` (503/504 paths via injection with `RANDOM_ERRORS_DB_THROW`).
4. Two raw throws change text only if reachable: `Xapian::InvalidArgumentError("Language code ... unknown")` and
   `AssertionError("No manager")`. They become `engine::Error` with the old description string passed in; a probe with an
   unknown `stem_language` is run before and after.

#### 5. Steps (phase 2), each building under ASan+UBSan

1. `engine/error.h`: `syntax`, `description`/`detail`, `is_missing`, `read_fate`; `Failure` fills them; `rethrow_translated`
   adds QueryParser/Serialisation and the residual `Xapian::Error`; `DocView` translation (N6); tests.
2. `Shard::retry_read`; migrate shape A/A' (19 sites) and D; ladders become 1 catch each.
3. Shapes B/C/F, `http_utils.h`, `main.cc` hooks, the raw throws (`Xapian::DocNotFoundError` x8 to `engine::Error`
   `doc_not_found`), `exception_xapian.h` loses its Xapian bases (keeps `ClientError` and the `Error` mixin; `SerialisationError`,
   `CastError`, `InternalError` become `ClientError` only: nothing catches their Xapian bases).
4. Spellings and includes (sed), `serialise-double`, `engine::xapian::version()`, text analysis, `xapian.h` move, replication
   catches; `seam_check.sh --update`. The GPL file: a counting `perl` script for the lead (the `xapian.h` include line, and
   nothing else, if its three Xapian catches stay).
5. Proof as in the task: unit tests, corpora, E2E, cluster, a revert/mutant per mapping kind (13 rows + `fate` arms), floor.

Done when `seam_check.sh` exits 0 with the allowlist above and the proofs hold.

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

## Done (X4c)

Rulings applied: `errc::syntax`; `Error::description()`/`detail()`; any other `Xapian::Error` becomes `internal`; `DocView`
throws `engine::Error` only (N6); `read_fate`/`is_missing` in `engine/error.h`; one `Shard::retry_read`; D1 unified with
the write guard; D2 as proposed except the floor items below.

- `engine/error.h`: `syntax`, `description()`, `detail()`, `is_missing`, `fate` and `read_fate`. `Failure` fills the texts from
  the Xapian original; `rethrow_translated` adds `QueryParser` (syntax) and `Other` (internal) and leaves any
  `std::exception` alone (Xapiand's `SerialisationError` is a `ClientError` and a Xapian type and must stay a 400).
- `DocView`: the eight methods translate (`guarded`); `prepare` translates the query text.
- `Shard::retry_read` (13 read ladders and `get_docid_term`, `get_view`, both metadata reads) and `Shard::collect_failure`
  (the four fan-outs: metadata keys, get/set metadata, commit). 20 missing-is-data catches, the NotAvailable catches, the
  match ladder (no close, as before), `http_utils.h`, the log hooks in main.cc, the eight raw `DocNotFound` throws,
  manager.h/lock.h/manager.cc/search_views.cc raw throws are `engine::Error`.
- Spellings: `Xapian::rev/docid/termcount/doccount/valueno` are `engine::` in every MIT file; `src/xapian.h` is
  `src/engine/xapian/xapian.h`; `Xapian::major_version()` is `engine::xapian::version()`; `serialise-double.h` is
  `engine::serialise_double`; word-break text (`TermGenerator`), `Stopper` and `Stem` live in `src/engine/xapian/{text,
  stopper,snowball_stem}`; `src/stopper.*` keeps only the MIT stop-word list.
- `exception_xapian.h`: `DocVersionConflictError` and `QueryParserError` are `ClientError` plus `engine::Error` (no Xapian
  base); `CastError` is a `ClientError`. `SerialisationError`, `NetworkError` and `InvalidArgumentError` keep their Xapian
  base (see floor).
- New: `tests/test_engine_error.cc` (243 checks), `test_failure_policy` in `tests/test_pool.cc`, `harness/seam_allowlist.txt`
  (7 files, 156 lines, from 38 files, 622 lines).

### D1, the six sites

Checked as ruled. The ledger's list was wrong for shard.cc 1783/1827 (`get_metadata`, `get_metadata_keys`): they already had
the Modified arm. The real sites without it were the four fan-outs in handler.cc. `get_metadata_keys` and `get_metadata`
only read: unified (a Modified that escapes the shard's own retries is now collected and the next shard is tried; the error
is rethrown only if every shard failed, instead of closing and throwing the 503 at once). `set_metadata` and `commit` write
inside the `try`: they keep close-and-throw for `reopen` (`collect_failure(..., writes=true)`); every other kind is
collected as before. Also unified: `get_docid_term` and `get_view` now close the shard on a final Modified (they did not).
The probe is `test_failure_policy` on a real opened shard: for every kind, retry or throw and open or closed, for reads and
writes (reopen is collected by a read and fatal for a write). Forcing a real `DatabaseModifiedError` past the shard's own
ladder needs fault injection (`XAPIAND_RANDOM_ERRORS`, off in this build), which I did not turn on.

### Floor of the seam allowlist (differs from the ruling)

`remote_protocol_views.cc` 125, `.h` 5, `remote_protocol_service.h` 1 as ruled; `replication_protocol_views.cc` 6, not 1: it
serialises any `Xapian::Error` it catches to the peer with the GPL `serialise_error`, so its four catches and its include
stay (its `NotAvailable` catch and its two throws moved). Also kept: `shard.cc` 6 and `shard.h` 4 (the remote server calls
`add_document`, `replace_document` and `replace_document_term` with `Xapian::Document`; I found it only when the build broke,
by match-only tokens), and `exception_xapian.h` 11 (`SerialisationError`, `NetworkError` and `InvalidArgumentError` stay
Xapian-derived: the remote and replication servers catch `Xapian::Error` to answer a malformed message and the remote server
throws `NetworkError` x5 and `InvalidArgumentError` x7; the first cut dropped the last one, review C2). No `xapian.h` include line in the remote .cc needed a change (it reaches the header through
`remote_protocol_views.h`), so there is no script for the lead.

### Changes in behaviour

1. D1 above (stale snapshot on exhausted retries in the read fan-outs and a close in `get_docid_term`/`get_view`).
2. `engine::unserialise_double` throws `engine::Error(invalid)` on bad bytes where `metric_serialise.h` threw
   `Xapian::SerialisationError`; only malformed remote bytes reach it. The wire bytes are the same function.
3. Raw `engine::Error` throws that were Xapian types carry the reason as the 500 text (`manager.h`, `lock.h`, manager.cc's
   endpoint checks, search_views' "Endpoint node not available"); the `NetworkError` mixin and the stem-language error keep
   the old description. Unreachable from a request today (an unknown stem language is a 400 in the schema).
4. `syntax` maps to HTTP 400 `Bad Request: <description>` in `catch_http_errors` (review C1). The raw `QueryParserError` of
   a text query answered 500 before (`prepare` runs outside the match ladder, so it never reached the ladder's 400); 10 of
   242 malformed bodies in the reviewer's probe change from 500 to 400 and nothing else does. The ladder still turns a
   `syntax` from `search` into a `ClientError`.

## Proof (X4c)

Builds: ASan+UBSan, `.scratch/build-asan`, `quiet light`, -j4, at Xapiand:01d8b6ccb. Baseline answers: the saved
`qd/qnf/qr/qs-base.json`.

1. Unit tests: 18 of 18 (`test_engine_error` 243 checks: the 19-row accessor table comparing `reason/description/detail` with
   `get_msg/get_description/get_error_string`, the fates, a view over a closed database throwing `closed` from all 7 reading
   methods, 30 exception values through the old Xapian-typed `catch_http_errors` (copied into the test) against the new
   one, the double bytes; `test_pool` 97 checks).
2. Corpora `cmp` identical to the baseline: `qdiff.py` 2,007, `qnf.py` 104, `qread.py` 911, `qsim.py` 265, including error
   responses (404, 400, 409, 500 with Xapian descriptions). Docs E2E 339 requests, 576 of 576, `e2e_diff` GREEN.
   `cluster_range.sh 2` GREEN, 30 checks, the 12 saved bodies identical to the X4b run (`cl3-new` vs `cl3-x4c`).
3. The corpora caught my own bug: a regex pass moved four case-closing braces past their `break` in handler.cc (stored blobs
   stopped being pushed; `qread` showed 462 differences and a 500 "Bad file descriptor"); a second run showed
   `SerialisationError` answering 500 (`qdiff`), which became the `std::exception` pass-through in `rethrow_translated`.
   Both fixed in Xapiand:94c212d24 and both rows now covered by tests or the corpora.
4. Mutants (`.scratch/x4c/mutants6.py`, each rebuilt and judged by the unit tests): 30 of 30 caught: 8 HTTP kinds (404, 409,
   reopen, 503/504, 504/502, `detail`, ClientError 400, 500 body), 15 translations (each Xapian type thrown as another
   kind), the `std::exception` pass-through, `classify QueryParser`, 3 fates (reopen, closed, corrupt), `is_missing`, and
   `description`. A first run was lost because two copies ran at once and one restored a stale file; the script now
   restores with `git checkout`.
5. Sanitizers: the unit tests and the corpora/E2E/cluster node logs show only the lz4 and rapidjson UBSan lines the
   baseline prints; none from the new code.
6. `seam_check.sh` exits 0 on the new floor.

Not done: TSan, libFuzzer (no decoder or parser added), Linux and FreeBSD builds, benchmarks, `XAPIAND_RANDOM_ERRORS`
injection through the converted fan-outs. `search_views.cc:1987` still prints `#xapian_query` from a Xapian query
description (no `Xapian::` token, same output); queryparser work on `phase0.5/queryparser-ab` should replace it.

For the other developer (queryparser): I did not touch the Text node, `query_dsl`, or CMake except adding
`engine_error` to the unit-test list. `query.cc` still includes `snowball_stem.h` and `stopper.h`, which now resolve to
`src/engine/xapian/` (same directory); `src/stopper.h` keeps only `getStopWords`.

## X4c code review (C1, C2, N1, N5)

Xapiand:84e7e00f9. Builds ASan+UBSan, `.scratch/build-asan`, `quiet light`, -j4. Probes copied from the review worktree
(`.scratch/x4c/p1_syntax.py`, `p2_remote.py`, run by `probes_fix.sh`); mutants in `.scratch/x4c/mutants7.py`
(`mutants7.out`).

1. C1: `catch_http_errors` answers `errc::syntax` 400 with the description (`syntax400.patch`). The `QueryParser` row of
   `test_engine_error` expects `Bad Request: QueryParserError: Syntax: bad` (the case table takes per-row expected values).
   Corpus row `{"title": {"_and": "foo AND"}}` added to `qdiff.py` (13th query, 3 answers: sorted, `check_at_least`, plain):
   500 on the baseline binary, 400 on the branch. The four corpora (qdiff now 2,010 answers, qnf 104, qread 911, qsim 265)
   are byte-identical to the baseline except those 3 answers. The reviewer's probe: 242 answers, exactly 10 differ, all
   500 to 400, base had 10 x 500.
2. C2: `InvalidArgumentError` has its Xapian base and initialiser back. `test_engine_error` has `static_assert(is_base_of_v<
   Xapian::Error, T>)` for `InvalidArgumentError`, `NetworkError`, `SerialisationError` (the three Xapiand types the remote
   and replication servers throw or serialise; found from the MIT side and from `git grep -o` tokens on the remote file:
   `THROW(InvalidArgumentError` 7, `THROW(NetworkError` 5, and `length.h` throws `SerialisationError`) and for
   `engine::xapian::InvalidArgument` (the replication views' throw). The raw-socket probe `p2_remote.py` answers byte for
   byte as the baseline (`diff p2-base.out p2-fix.out` empty). `seam_allowlist.txt`: `exception_xapian.h` 11.
3. N1: `engine::xapian::Shard` is not `final`, so `test_pool` stands in an engine whose `find_key` fails
   (`test_get_docid_term_policy`: corrupt closes and throws, invalid throws and stays open, a stale snapshot reopens and
   retries, an exhausted one closes). The two `writes=true` fan-outs need `XapiandManager` (`lock_shard`), so
   `test_fan_out_call_sites` pins them in `handler.cc`: `set_metadata` and `commit` call `collect_failure(..., true)` and
   only the two read fan-outs call it with `false`. That one is a source check, not a behaviour test.
4. N5: `test_failure_policy` removes `pool_test_policy/` before and after.
5. Reverts and mutants (each rebuilt, judged by the unit test): set_metadata `writes` true to false: caught
   (`test_pool` 392/393); commit `writes` true to false: caught; `get_docid_term` without `retry_read`: caught
   (`test_pool` 358/360); C1 revert (syntax arm off): caught (`test_engine_error` MISMATCH QueryParser old 400 new 500);
   C2 revert (Xapian base dropped): does not compile (`static_assert` in `test_engine_error.cc`); N5 revert leaves
   `pool_test_policy` in the cwd, the fix does not.
6. Unit tests 18 of 18 under ASan+UBSan (`test_pool` 109 checks).
7. `cluster_range.sh 2`: GREEN, 30 checks, 0 failures; the 12 saved bodies are identical to the X4c run (`cl3-x4c` vs `cl3-x4cfix`).

Follow-ups left as they were: N2 (`invalid` in the metadata ladders also covers `InvalidOperation`), N3
(`unserialise_double` on malformed bytes), N4 (log text for a raw Xapian error), N6 (`invalid` is one kind for a caller's
mistake and an internal breach; for the architect).

## Next

The lead reviews and lands `phase0.5/engine-search-c` (never pushed). Review focus: `read_fate` and the D1 write guard,
the floor deviations above, and `rethrow_translated` ordering.

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

Nothing.
