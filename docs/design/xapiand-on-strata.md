# Xapiand on strata: the engine seam and the path to Phase 7a

Revision 1 (2026-10-20), designing (ready for preliminary review). Reads Xapiand `855f75281`
(master), prospect `0c895cb`, strata `6e13416`.

Phase 7a puts strata behind Xapiand's engine at `rf = 1`, with Xapiand's own WAL, and passes
the docs-as-tests E2E suite (`AGENTS.md`, "Verifying changes end-to-end") with zero
regressions. Phase 0.5, the seam itself, comes first and passes the same suite with Xapian
still underneath. This design inventories every Xapian facility Xapiand uses, says what covers
each, shapes the interface, and orders the steps. It stops at the first buildable steps.

Clean room: Xapiand's own files (MIT) were read. Nothing under `src/xapian/` and no
`sortable_serialise` file was opened. One exception, disclosed: the first 140 lines of
`src/server/remote_protocol_views.cc` were read (includes and constructor) before checking
that PLAN lists the file as GPL (Phase 8); nothing past line 140 was read, and the rest of its
rows below are grep counts. Xapian behavior comes from public documentation and the oracle
vectors in `strata-oracle` (file names only were listed).

## Summary

1. The seam is a Xapiand-owned, MIT interface in `src/engine/` (D1, D2). Xapian becomes one
   implementation in `src/engine/xapian/`; `Shard::db()` (52 sites in 9 files) is gone, and
   the remote protocol and Xapian replication bind to that implementation only (D11).
2. 7a is a functional step, not the speed step. Values stay opaque byte strings, sorts,
   ranges, geo, and aggregations stay Xapiand's own callbacks, and text queries go through a
   new `queryparser` (D3, D4, D6). Typed slots, points, and native collectors plug into the
   same seam later.
3. Seven gaps block 7a's E2E run: values in segments, term lists, Xapian's remaining query
   operators, a host-callback scorer, key-then-relevance sorting, an exhaustive match visitor,
   and `queryparser`. Everything else is deferrable (the gap table says which).
4. The seam and the strata gaps are independent, so they run in parallel with strata's speed
   work: Phase 0.5 is entirely in the Xapiand repository.

Critical path (the long pole is `queryparser`, then the serial seam chain):

```
 X1 analysis swap -> X2 engine core -> X3 engine query -> X4 engine search -> X5 gate (0.5)
                                                                    \
 P1 values (bytes) --+                                                +--> S1 -> S2 -> S3 -> S4 -> S5 gate (7a)
 P2 term lists ------+-- all independent of X1..X5, start now        /
 P3 operators, P4 collectors ---------------------------------------+
 L1 queryparser design -> build ------------------------------------> S4
```

## What this depends on and changes

| Document | Relation |
|---|---|
| PLAN.md, Phase 0.5 (292-298), Phase 7 (651-668) | Implements 0.5. Makes "`rf = 1`" mean one node (Q1). Adds 7a gate rows T6 to T9. |
| FEASIBILITY.md:101-126, "What Xapiand uses from Xapian" | Superseded by the call-site inventory below; its rows stand. The interface, once landed, is the authoritative list (PLAN). |
| prospect `docs/design/phase-4.md` D3, D4 | Uses D4's column codec. Changes D3 for 7a: an undeclared slot is a single-valued `bytes` column (D3 below). Needs the architect's ruling. |
| phase-4 D6, D8, D11 | Unchanged. 7a runs the same jobs as host callbacks; D6, D8, D11 replace them later behind the same seam nodes. |
| `docs/design/format-13.md:610-616`, F13-R10 | The id term does lead with `Q` (census, D5); the condition becomes row T5, which holds once a numeric id's accuracy terms move off `Q` (A-T5). |
| Xapiand `docs/dev/xapian-fork.md:147` | The 1:1 rule (remote server mirrors Xapian's) is why the remote protocol stays Xapian-only (D11). |

## 1. Seam inventory

Counts are `Xapian::` references in Xapiand's own code (153 headers and sources, excluding
`src/xapian/`, measured with a file list from `git ls-files`): **1,552 references in 50
files**, 125 of them in `remote_protocol_views.cc`. Nothing was compiled or run.

| Group | What Xapiand uses | Call sites (files) |
|---|---|---|
| A. Open, commit, replication | `WritableDatabase`/`Database` open, create, `Remote::open[_writable]` (`shard.cc:342,455`); `DB_NO_SYNC` (`shard.cc:79`); commit and transactions (`shard.cc:806-813,881,899,917`); `reopen` (`shard.cc:554`); `get_revision` 40 and `get_uuid` 31 (8 to 10 files); `get_lastdocid` 6, `get_doccount` 7; `Database::check` (`handler.cc:284`); `DB_*` flags (`flags.h:29-62`); `Xapian::rev` 109 refs in 18 files; whole-database copy for replicas (`replication_protocol_views.cc`, 6 refs, `db()` 8 sites) | 52 `->db()` sites in 9 files; 248 `catch (const Xapian::...)` lines in 13 files; 49 `DB_RETRIES` lines |
| B. Documents, terms, metadata | `Document` 260 refs in 19 files; `add_term`/`add_boolean_term`/`add_posting` 44 (`schema.cc`, `generate_terms.cc`, `shard.cc`); `add_value` 14; `set_data` 11; `get_data` 26; `termlist_begin` 5, `values_begin` 3, `positionlist_begin` 3 (`handler.cc:2665-2890`); writes per docid and per unique term (`shard.cc:924,1026,1228,1353,1493`) returning `DocumentInfo` {did, version, term}; version check-and-set written five times (`shard.cc:958,1067,1279,1400,1556-1620`) with a `"V"+did+version` boolean term and a slot value; `postlist_begin(term)` 7 (`handler.cc:1587`); metadata get, set, keys 54 refs in 10 files (`shard.cc:1945-2052`); WAL stores `Document::serialise()` (`wal.cc:1057`, read back `:288,558`; also `handler.cc:2487`) | see left |
| C. Values and slots | Slots are `xxh32` of prefix and type, reserved 0 to 20 (`utils.h:41-57`, `utils.cc:59-67`); `get_value` 60 (15 files); `BAD_VALUENO` 16; values are `sortable_serialise` strings or `StringList` blobs, one per slot | |
| D. Queries, parser | `Query` 260 refs in 12 files; nodes: term(wqf,pos), match-all, match-nothing, AND, OR, AND_NOT, XOR, AND_MAYBE, FILTER, SCALE_WEIGHT, ELITE_SET, NEAR, PHRASE, SYNONYM, MAX, WILDCARD, EDIT_DISTANCE, and a posting-source leaf (`query_dsl.cc:273-336,736-772`); `QueryParser` 52 refs, used for every `text` field (`query_dsl.cc:831-877`): `set_default_op`, `set_stopper`, `set_stemmer`, `set_stemming_strategy`, `set_max_expansion`, `parse_query`; flags set: BOOLEAN, PHRASE, LOVEHATE, FUZZY, WILDCARD, WILDCARD_SINGLE, WILDCARD_MULTI, PARTIAL, CJK_NGRAM, WORD_BREAKS (the other flags appear only in a debug printer, `:74-89`). No `add_prefix`, no spelling flag set | |
| E. Enquire, MSet, ranking | `Enquire` 8 refs: `set_query`, `set_collapse_key` (BAD unless the `collapse` URL param, `handler.cc:1159-1166`), `set_cutoff(0,0)`, `set_docid_order(ASC)`, `set_sort_by_key_then_relevance`, `add_matchspy`, `prepare_mset` (fork), `get_mset(first,maxitems,check_at_least)`, `get_eset` (`handler.cc:1276-1300`); several shards are one `Database` via `add_database` (`handler.cc:1267`); `MSet`: `size`, `get_matches_estimated` (`search_views.cc:1979-1980,2110`), iteration, and `get_rank`/`get_weight`/`get_percent` only under `comments` (`:2023-2024`); default BM25, no `set_weighting_scheme` outside the remote server; `RSet`/`ExpandDecider` 12 refs for `nearest` and `fuzzy` | |
| F. Spies, sources, key makers | `MatchSpy` 8 refs (`aggregations.h:107`, `aggregations.cc:338-380`); aggregations read `Document` 129 refs (`metrics.h`, `bucket.h`) through `get_value(slot)` (`metrics.cc:91`, `bucket.cc:127,141`) and the term list by prefix (`metrics.cc:125-137`); `ValuePostingSource` 36 refs (`MultipleValueRange/GE/LE` in `range.cc`, `GeoSpatialRange`); `KeyMaker` 7 refs, `Multi_MultiValueKeyMaker` reads `doc.get_value` per candidate (`keymaker.cc:68-548,712`); all three serialize for remotes through `Registry` (14 refs, 9 files) | |
| G. Geo | HTM coverings built host-side into terms (`generate_terms.cc:298,452`) and a value source with a centroid weight; geo sort key (`keymaker.cc:518,546`). Nothing geo-specific is asked of the engine | |
| H. Analysis, spelling | `TermGenerator` 17 refs (one site, `generate_terms.cc:504-527`), `Stem`, `Stopper` (`stopper.h:45`). Spelling and synonyms: `add_spelling`/`remove_spelling` are reached only from the WAL and the remote server (`shard.cc:1715,1784`, `remote_protocol_views.cc:1583,1607`); no HTTP or indexing path calls them; synonyms appear only in the remote server and the replica file list. FEASIBILITY's "out of scope" holds | |
| I. Remote protocol | Xapian's wire protocol served by Xapiand (`remote_protocol_views.cc`, 125 refs, GPL by PLAN) and used by `Remote::open`; registers the three source classes, the spy, and the key maker | |

Also in the inventory: `exception_xapian.h` wraps 8 Xapian exception types into Xapiand's
`ClientError` hierarchy, and `database/flags.h` re-exports Xapian's open flags as ints.

## 2. What covers each group

"E2E" means needed for the docs suite to pass on strata. Doc mentions are an upper bound
(the suite has about 303 requests and 73 failures that already fail on Xapian,
`harness/e2e_check.sh:6-8`); step X5's census (T1) replaces mentions with measured use.

| Group | Covered by | Missing (gap) | E2E? |
|---|---|---|---|
| A. Lifecycle | strata core: `create/open/apply/commit/refresh/close`, manifest `revision`, `last_docid`, uuid, metadata ops (`files.hh:98`, `index.hh:44,414,458,555`) | Xapiand-side only: engine selection (D8), refresh before search (D7), commit cost under `DB_NO_SYNC` behavior (risk) | yes |
| B. Documents | prospect `document` (terms, wdf, positions, stored, key, version); `replace_key`/`delete_key`, docid ops, expected version (`index.hh:44`); real-time get | G1 values, G2 term lists, G11 stats (doc-length bounds, average length, `has_positions`) exposed | yes |
| C. Values | `docvalues.hh` codec only (kinds u64, i64, f64, bytes; not wired to segments); `ordered.hh` | **G1** values plugin wired to flush, merge, open, check, digest, with a `bytes` single-valued column per undeclared slot | yes |
| D. Queries | prospect scorers: term, conjunction, disjunction, required-optional, exclusion, phrase and near (`search.hh:316-1061`); BM25 with Xapian's constants (`search.hh:85-97`) | **G3** SYNONYM, XOR, MAX, SCALE_WEIGHT, ELITE_SET; **G8** wildcard, partial, and fuzzy expansion over the dictionary (`scan_dictionary`, `reader.hh:370`; automata is Phase 5); **G9** `queryparser` | yes, G9 certain; G3 and G8 by doc mentions (wildcard 7 files, partial 12, fuzzy 4; the other operators 1 to 2 each) |
| E. Enquire | `search()` with exact count and top-k, docid tie-break (`searcher.hh:204`) | **G5** key-then-relevance collector with a host key; **G7** one snapshot over several shards with Xapian's docid interleave and global statistics; G12 estimates are exact counts, which is Xapian's tier 1 (`phase-4.md:323-341`) | G5 yes; G7 yes: the default node runs 5 shards (`opts.cc:43`, `NUM_SHARDS`), and all 94 census searches spanned 5 (the earlier `num_shards = 1` reading was wrong) |
| F. Spies, sources, keys | `aggregate.hh`, `sortkey.hh`, `collapse.hh` are standalone codecs and collectors; scorer interface fits a host source | **G4** scorer over a host source (`next/skip_to/check/weight`); **G6** exhaustive visitor that hands each match to a callback with a lazy document view | yes |
| G. Geo | terms plus G4 and G5 | nothing; the geo index (phase-4 D15) is deferred | yes, through G4, G5 |
| H. Analysis | `Kronuz/analysis` (`TermGenerator` exact, 966 vectors) and `Kronuz/snowball`, both built | stopper adapter (`analysis::stopper` from Xapiand's word lists); `cjk_words`/ICU absent (the suite only asserts both flags false, `tests/schema/define-prefix.md:53-54`) | yes (adapter); cjk_words deferrable |
| I. Remote | none by design; shard RPC is 7b | D11: strata indexes are single-node | n/a (`--solo`) |

Deferrable for the 7a E2E gate, each a named upgrade behind a seam node: collapse (no doc
mentions), `nearest`/`fuzzy` expansion (`get_eset`; no doc use found), `_percent`, `_weight`,
`_rank` (diagnostic only), delete by query (Xapiand never calls it), typed slots and points,
the geo index, native aggregation collectors, dense slots (removes the hash-collision risk of
`phase-4.md:119-127`), `cjk_words`, shards above one, replicas, spelling, synonyms.

## 3. The seam

Sketch of the shape. Step X2 finalizes signatures; the decisions below bind.

```cpp
namespace engine {
using docid = std::uint32_t; using rev = std::uint64_t; using valueno = std::uint32_t;
enum class errc { reopen, opening, closed, not_found, not_available, network, timeout,
                  doc_not_found, version_conflict, corrupt, invalid, internal };
struct Error : std::runtime_error { errc code; };

class Document {                          // built by Schema, engine neutral, owned data
  void add_term(std::string, termcount wdf = 1);  void add_boolean_term(std::string);
  void add_posting(std::string, termpos, termcount wdf = 1);
  void add_value(valueno, std::string);   void set_data(std::string);
  void set_key(std::string term);         // the id term, explicit (D5)
  std::optional<rev> expected_version;
  std::string serialise() const;  static Document unserialise(std::string_view);   // WAL (D10)
};
class DocView {                           // lazy read of one stored document
  virtual std::string data() const = 0;   virtual std::string value(valueno) const = 0;
  virtual rev version() const = 0;        virtual void terms(std::string_view prefix, TermSink) const = 0;
};
struct WriteResult { docid did; rev version; std::string term; };  // Xapian::DocumentInfo today
class Shard {                             // one per ShardEndpoint checkout
  WriteResult add(Document&&);   WriteResult replace(docid, Document&&);
  WriteResult replace_by_key(const std::string& term, Document&&);
  void erase(docid);             void erase_by_key(const std::string& term);
  rev commit();  void begin_transaction(bool flushed);  void commit_transaction();  void cancel_transaction();
  std::unique_ptr<DocView> get(docid);   docid find_key(const std::string& term);
  Stats stats();   // uuid, revision, lastdocid, doccount, total length, length bounds, avlength, has_positions
  std::string get_metadata(const std::string&);  void set_metadata(...);  std::vector<std::string> metadata_keys();
  void reopen();  void close(bool commit);  std::string check();
};
struct Query;       // variant tree: the 18 nodes of section 1, group D, plus Text{string, flags, field config}
struct PostingSource; struct KeyMaker; struct MatchSpy;      // host callbacks, serializable (D4)
SearchResult search(std::span<Shard* const>, const SearchRequest&);   // hits, estimated, per-hit docid
}
```

**D1. The interface is Xapiand's, not a Xapian-shaped shim.** Alternatives: (a) emulate
Xapian's API over strata, rejected because it drags Xapian's GPL headers and about a hundred
classes into the new engine, and two of Xapian's extension points (Registry, ESet) have no
strata analogue; (b) hide only `Shard`, rejected because `Query`, `Document`, and `MSet` leak
through `handler`, `schema`, `query_dsl`, and `aggregations` (the 260 references in groups B and
D). The cut is at documents, queries, shards, and search results.

**D2. Layout and the Xapian implementation.** `src/engine/` (MIT) holds the interface;
`src/engine/xapian/` holds the first implementation and the translation of Xapian exceptions
to `engine::Error`. A CMake option, on by default until Phase 8, builds it and the GPL remote
and replication code; Phase 8 deletes the directory. A script (`harness/seam_check.sh`)
fails if `Xapian::` or a `xapian` include appears outside the allowlist: this is T2.

**D3. Values are opaque bytes keyed by the existing 32-bit slot.** The seam's
`add_value(slot, bytes)` carries today's `sortable_serialise` strings and `StringList` blobs
unchanged. strata stores an undeclared slot as a single-valued `bytes` column (the dictionary
orders by `memcmp`, so the order Xapiand relies on survives). Alternative: typed slots now,
rejected for 7a: Schema's value generation (14 `add_value` sites), the key makers, ranges,
and aggregations all read these strings, and replacing `sortable_serialise` is Phase 8 anyway.
Typed slots arrive as new seam nodes (native range, typed sort component) with
`declare_slot`; phase-4 D3's rejection rules apply to declared slots only. Blocks P1 (Q2).

**D4. Callbacks stay callbacks.** `PostingSource`, `KeyMaker`, and `MatchSpy` are abstract
bases with the virtuals of today's classes, including `name/serialise/unserialise/clone`. The
Xapian implementation wraps them in Xapian's bases and keeps the `Registry` entries, so the
remote server works unchanged; strata calls them in-process, and the 7b shard RPC reuses the
serialized form. Aggregations, key makers, and ranges read a `DocView`, not a `Document`.
strata adapts a source to a scorer, a key maker to a collector key, and a spy to the
exhaustive visitor (G4, G5, G6). This is not a second mechanism in strata: the callback nodes
are the seam, and each native replacement removes its callback.

`PostingSource` is a value-slot filter (amended at X3, 2026-10-26): `slot`, `accept`, `weight`,
`max_weight`, name, clone and serialise, not a general `next/skip_to/check` cursor. Every source
Xapiand has filters one value slot; strata runs it as a column scan, and `skip_to`/`check` as a
docid lookup then `accept`. `accept`, `weight` and `clone` are const and thread-safe, and `accept`
rejects an undecodable value. A cursor-style source, if one is needed, is a new base.

**D5. Version, id, and docid belong to the engine.** `replace_by_key(term, doc)` carries the
id term, which the handler already holds (`handler.cc:1353-1377`), and `expected_version`.
The Xapian implementation takes over the five copies of the version dance (`V`-terms and the
version slot); strata uses its version map, and never writes a `V` term. The key is passed
explicitly, so strata's `key_prefix` routing needs only: at most one term per document leads
with the key prefix (T5). Inventory facts: `XAPIAND_HASHED_PREFIXES` is defined nowhere
(`git grep` finds it only at `utils.cc:73,83`), so field prefixes are `name.` strings
(`utils.cc:83-90`); the id field is `_id` (`fields.h:27`); its prefix is
`get_prefix(ID_FIELD_NAME)` (`schema.cc:91`). Measured by the census (Xapiand:829a67308,
`docs/dev/engine-census.md`): the id term leads with `Q` (`QK` keyword ids, `QN` numeric), not
`_id.`; 2,623 of 2,625 suite documents carry exactly one `Q` term, and the two with a numeric
`_id` carry seven, the id term plus six accuracy terms that share `Q`. So a numeric id's
accuracy terms move to their own prefix (amendment A-T5, lead, 2026-10-27), which leaves `Q` as
the one key term per document; ranges over numeric ids keep working through the moved terms.

**Amendment A-KP (lead, 2026-10-27, from S1).** strata's key prefix cannot be `Q`: strata rejects any
other term that starts with the key prefix, and Xapiand field prefixes are `name.` strings, so a
field named `Quote` indexes `Quote.word`. The strata engine routes keys under `\xff` followed by the
id term; `\xff` never starts a valid UTF-8 field name, so no other term can collide. T5 still holds
for the Xapian engine's `Q` id terms.

**D6. Text analysis is host-side on both engines; parsing is engine-side.** Index-time text
goes through `analysis` and `snowball` for Xapian and strata alike (step X1), which removes
`TermGenerator` and index-time `Stem` before strata is plugged in and proves, on Xapiand's own
corpus, that the libraries match. The query side keeps a `Text{string, flags, field config}`
node: the Xapian implementation calls `Xapian::QueryParser`, strata calls `queryparser` (L1).
Alternative: run `queryparser` host-side on both engines and drop the node. It is the cleaner
end state (one implementation), but it puts a new 4k-line library (FEASIBILITY:33, an
estimate) on Phase 0.5's path. Revisit after S4.
Ruling (owner, 2026-10-26): once `queryparser` passes T4 inside Xapiand, Xapiand parses host-side
for both engines and the `Text` node leaves the seam (`docs/design/queryparser.md` Q4).

**D7. Reads see every acknowledged write.** A Xapian `WritableDatabase` shows its own pending
writes; the strata implementation calls `refresh()` (searchable, not durable,
`index.hh:500-502`) before a search when the shard has unpublished writes. `commit()` stays tied
to Xapiand's autocommit and explicit commit. Several local shards are one snapshot with global
statistics and Xapian's docid interleave `(did-1)*n + shard + 1` (`handler.cc:1592`).

**D8. Engine per index.** An index setting, fixed at creation and saved with the others
(`IndexSettings`, `index_resolver_lru.h:51`), default `xapian` until the 7a gate. An
environment override lets the harness run the suite on either engine. A strata index forces
`num_replicas = 0` (already the default, `opts.h:90`).

**D9. Errors are `engine::Error` kinds.** The 248 catch lines become catches on kinds; the
`DB_RETRIES` loops stay in `Shard` (they own `do_close` and `reopen`) and run once on strata,
which never raises `reopen`. `exception_xapian.h` is rewritten over the kinds.

**D10. WAL payload is opaque per engine.** The line keeps `(revision, type, payload)`
(`wal.cc:1057`); the payload is `Document::serialise()` and replay is
`Document::unserialise()`. Since an index has one engine, no tag is needed, and the `_wal`
endpoint (`wal.cc:288`) asks the engine to describe a payload. Old Xapian WAL lines stay
readable by the Xapian implementation because it keeps Xapian's own serialization.
Revision equality at replay (`wal.cc:519`) maps to the manifest revision, which bumps once per
commit that changed something (`index.hh:2189`).

**D11. Remote protocol and replication stay Xapian-only.** They reach the database through
`Shard::engine().native<xapian::Shard>()` and are compiled with the option in D2. A strata
index refuses non-local endpoints and replica pulls: 7a is single-node (Q1). The suite runs
`--solo` (`harness/README.md:36`), so 7a's E2E gate does not exercise this.

**D12. The census.** Both implementations count facility use (query nodes, sources, spies,
key makers, `DocView` methods, per request) into a JSON file when built with
`ENGINE_CENSUS`. T1 turns doc mentions into measured need before S2 is scoped.

**D13. The seam is async-shaped from the start (lead, 2026-10-21).** Xapian has no async API
and strata's reads will gain one (Phase 6 residency, 7c), so a synchronous seam would need its
call sites rewritten twice. Every seam operation that can block on I/O (open, commit, add and
replace, get, search, the WAL's fsync) returns `task::task<T>` (Kronuz/task). The Xapian
implementation runs the blocking call on Xapiand's existing offload pool and completes the task,
so behaviour is unchanged; strata's later suspends on real I/O. Until 7c turns a caller into a
coroutine, it bridges with a blocking `sync_wait` on the offload thread, so 7c removes bridges one
call site at a time instead of changing the seam. Cheap, non-blocking accessors (document fields,
`DocView`, metadata already in memory) stay plain calls. The binding from strata's `executor` and
`task` to Xapiand's reactor (an Asio-style adapter) is built at S1, when strata first runs inside
Xapiand, not before. Alternatives: (a) a synchronous seam now and an async one in 7c (two rewrites
of every call site); (b) callbacks instead of tasks (no cancellation, harder composition).

The blocking rule (amended by the offload review's C1, 2026-10-26): no reactor loop thread blocks
on database work: no `sync_wait`, no `DatabasePool` checkout or cleanup, no shard open, commit, or
close. A blocking caller runs on an offload worker (the serving reactor's `pool()`, or an existing
named thread: DP, DI, the committer, WL, the cleanup thread) and the engine call runs inline on it.
A coroutine caller on a reactor `co_await`s the task and never `sync_wait`s (from 7c). No thread
waits, by `sync_wait` or otherwise, on a pool it is a thread of. Xapiand enforces it with
`engine::check_may_block` (`src/engine/sync.h`). Residual: a remote-protocol connection's teardown
(`~RemoteProtocolViews`, destroyed on the loop when `serve_remote_connection` ends) is not moved
to the pool; `DatabasePool::checkin` carries the guard, so a teardown that checks a shard in on a
loop aborts in Debug and is counted and logged in Release.

## 4. The ordered path to 7a

Sizes are estimates for the lead's pacing, not measurements: S small, M medium, L large (one
Sonnet launch, aiming under about 300k tokens; L may need a split). All X steps are in the
Xapiand repository and use `quiet check` before building; none touches strata or prospect.

| Step | Branch | Repo | Done when | After | Size |
|---|---|---|---|---|---|
| **X1** analysis swap | `phase0.5/analysis` | Xapiand | `GenerateTerms::text` (`generate_terms.cc:496-530`) uses `analysis::term_generator` and `snowball`; `analysis::stopper` built from the word lists behind `getStopper`; `schema.h:224-247` strategies map; baseline E2E captured at the branch point; T2-style diff has zero regressions | none | S |
| **X2** engine core | `phase0.5/engine-core` | Xapiand | `src/engine/` with types, errors, `Document`, `DocView`, the `Shard` write and lifecycle slice, Xapian implementation; `schema.cc`, `generate_terms.cc` (document side), `handler.cc` prepare and index, `shard.{h,cc}` writes, `wal.{h,cc}`, `pool`, `flags.h`, `exception_xapian.h` migrated; `Shard::db()` still exists for reads; E2E and WAL recovery unchanged | X1 | L |
| **X3** engine query | `phase0.5/engine-query` | Xapiand | `engine::Query` and the three callback bases; `query_dsl`, `range`, `geospatialrange`, `keymaker`, `generate_terms` (query side), `aggregations` migrated to them and `DocView`; Xapian implementation translates and registers sources | X2 | L |
| **X4** engine search | `phase0.5/engine-search` | Xapiand | `engine::search`, result type, `get/find_key/stats/metadata/check`, dump and info endpoints, `Shard::db()` removed, remote and replication behind `native<>`; `seam_check.sh` in CI | X3 | M |
| **X5** gate 0.5 | main | lead | T1 to T4 | X4 | the lead |
| **P1** values | `phase4/values-bytes` | prospect | values plugin: `bytes` column per slot, slot directory per segment, flush, merge, open, check, digest record, reader (`value(ord, slot)`, presence iteration), fuzz target, fixtures, pre-freeze bump | none (Q2) | L |
| **P2** term lists | `phase4/term-lists` | prospect | term-lists plugin (all fields in 7a), reader by ordinal and prefix, merge, check, fuzz | none | M |
| **P3** operators | `phase4/xapian-ops` | prospect | SYNONYM, XOR, MAX, SCALE_WEIGHT, ELITE_SET scorers with oracle vectors; host-source scorer | none | M |
| **P4** collectors | `phase4/host-collectors` | prospect | key-then-relevance collector with host key; exhaustive visitor; one snapshot over several indexes with docid map and global statistics (G7, may follow S2); doc-length bounds | none | M |
| **L1** queryparser | `design/queryparser`, then its own repo | new | design with oracle vectors from `strata-oracle` (`parse` vectors exist); subset below | none | design M, build L |
| **S1** strata engine, write | `phase7a/strata-engine-write` | Xapiand | `engine::strata::Shard`: lifecycle, add, replace, erase, by key, version check, metadata, stats, `DocView` data and values; WAL replay; hits by docid | X4, P1 | M |
| **S2** strata engine, search | `phase7a/strata-engine-search` | Xapiand | `engine::search` for term, boolean, phrase, near nodes; estimates; `refresh` before search; docid tie-break | S1, P4 | M |
| **S3** callbacks | `phase7a/strata-engine-callbacks` | Xapiand | operators (P3), expansions over the dictionary (G8), sources, key makers, spies; term lists for `_terms` and info | S2, P2, P3 | L |
| **S4** text | `phase7a/strata-engine-text` | Xapiand | `Text` node through `queryparser` | S3, L1 | M |
| **S5** gate 7a | main | lead | T6 to T9 | S4 | the lead |

`queryparser`'s subset is what Xapiand sets, no more: default operator, stopper, stemmer and
strategy, expansion limits, a default prefix, and the ten flags of section 1, group D.
Building it first against the Xapian implementation of `Text` (D6) is not required.

**Parallelism.** X1 to X4 and L1 do not touch strata. P1 to P4 touch prospect's writer,
reader, and merge files, where the speed branches (`perf/*`, `phase4/f13-*`) also work, so
each P step puts its code in new files behind the plugin interface (`phase-4.md:19-50`),
rebases daily, and builds under `quiet check`. P steps never block speed work and can wait
behind it if quota is short; X steps cost no strata compile time.

**First steps a developer can start now:** X1 (smallest, produces the E2E baseline everything
else compares to), then X2. P1 can start once Q2 is answered. L1's design can start now.

## 5. Gate rows

Measurement rows (T3) are the lead's, on `main` commits only; a branch makes functional
claims only.

| Row | Target | Procedure |
|---|---|---|
| **T1** census | Per E2E request, the facilities used, committed as `docs/dev/engine-census.md` in Xapiand | Build with `ENGINE_CENSUS`; `harness/e2e_capture.sh`; the census file is written per request. Classes in section 2 are updated from it before S2 is scoped |
| **T2** seam closed | Zero files outside the allowlist contain `Xapian::` or a `xapian` include | `harness/seam_check.sh` (`git grep -l 'Xapian::\|xapian/\|"xapian.h"' -- src ':!src/xapian' ':!src/engine/xapian'` minus the remote and replication allowlist) exits 0 |
| **T3** parity (0.5) | Zero assertion regressions against a baseline captured at the branch point; response bodies differ only in volatile fields; harness throughput within the noise band | `e2e_capture.sh` then `e2e_diff.py ours base`, `bodydiff.py base ours`, `loadtest.py` then `perfdiff.py` (`harness/README.md:63`). PLAN: "zero E2E assertion regressions (passing assertions out of 303 requests)" |
| **T4** cluster and WAL (0.5) | `cluster_check.sh 2` and `3`, `wal_recovery_check.sh` pass unchanged on the Xapian implementation | Run as written |
| **T5** key prefix | At most one term per document leads with the key prefix, and it is the id term; none of the accuracy, partial-path, or `exists` terms do | A debug assertion in `engine::Document` under `ENGINE_CENSUS`, counted over the suite; closes F13-R10's condition |
| **T6** E2E on strata | Zero assertion regressions against the Xapian baseline, single node, engine forced by the environment override | Same procedure as T3 with D8's override |
| **T7** bodies | Every body difference between engines listed and justified (scores, ties, estimates) | `bodydiff.py` xapian against strata; the list is the report |
| **T8** crash recovery | After `kill -9` during indexing, restart replays the WAL to the same document set as a Xapian run of the same input; the strata digest check passes | A strata variant of `wal_recovery_check.sh` |
| **T9** migration | `_dump` on one engine, `_restore` on the other, equal `_dump` output | Script over the `bank` dataset |
| **T10** concurrency | Concurrent index and search on one strata shard is clean under ThreadSanitizer; the adapter takes snapshots without a data race (`index.hh:685`) | Xapiand sanitizer build running a mixed load for a stated time |
| **T11** replacements win by a lot | Each replaced layer's replacement costs at most half of what it replaces, per document, on the same bulk-index run: strata's engine share against Xapian's (7a), the group journal against Xapiand's WAL (7b); parity fails the row (owner, 2026-10-27: "beat by a whole lot") | `harness/profile_bulk.sh` on both binaries, the replaced layer's per-document CPU and critical-path time from its table |

Every gate also owes Xapiand's extraction ledger rows (`docs/dev/extraction-ledger.md`): the
five version dances collapsing to one engine call is X2's first row.

## 6. Open questions for the owner

**Q1. Is 7a single-node?** The Xapian remote protocol and replication cannot serve strata, so
a strata index lives on one node, with no replicas, until 7b. The cluster lane
(`cluster_check.sh`) stays on Xapian indexes. Options: (a) single-node 7a; (b) build a
temporary shard RPC for 7a. Recommendation: (a). PLAN's "rf = 1" then means one copy on one
node, and 7b is where distribution arrives. Does not block X1 to X4.

**Q2. Opaque `bytes` slots for 7a, or typed slots first?** Options: (a) opaque now, typed
later behind new seam nodes (D3); (b) design `declare_slot` and typed schema generation into
7a. Recommendation: (a). It changes phase-4 D3 for undeclared slots and ties the speed win on
ranges, sorts, and aggregations to a post-7a step; the 7a gate is functional. Blocks P1.

**Q3. Pull `queryparser` from Phase 5 onto the 7a path?** Every `text` field query goes
through Xapian's parser (`query_dsl.cc:831-877`), so 7a cannot pass without an equivalent.
Options: (a) build it now with the Xapiand subset; (b) keep Xapian's parser reachable from
strata (not possible without Xapian). Recommendation: (a), designed from `strata-oracle`'s
`parse` vectors (the harness records Xapian 2.1.0 while Xapiand vendors 2.0.0, so the E2E
gate arbitrates differences). Blocks S4, not X or P steps.

**Owner answers (2026-10-20).** Q1: (a), 7a is single-node. Q2: (a), opaque `bytes` slots
first; this amends phase-4 D3 for undeclared slots, and P1 may start. Q3: (a), `queryparser`
moves onto the 7a path; L1's design starts in parallel with the X steps.

## 7. Risks and what is not covered

- **Behavior long tail.** Estimates, `OP_SYNONYM` statistics, and parser output can differ
  where the E2E suite is silent; T7 lists them.
- **Commit cost.** `commit()` is durable (`F_FULLFSYNC` on macOS, `index.hh:555`) where
  Xapiand runs `DB_NO_SYNC` behind an async WAL (`shard.cc:79`). The debounced autocommit
  decides the cost. Not measured here; the lead measures on `main`.
- **Many small indexes.** Open and idle cost per shard through the pool is `small-indexes.md`'s
  subject and is not on this path.
- **Seam overhead.** `Document` is copied once into Xapian's on write and `DocView` is lazy on
  read; T3's throughput row catches a cost.
- **Not read.** `schema.cc` beyond the cited ranges, Lua, `raft_delegate.h`, and the HTTP
  layer; none calls Xapian beyond the counts in section 1.

**Ruling R-P2 (owner, 2026-10-28): no term lists in strata or prospect.** "Xapiand/Xapian features
(and much less bugs) alone should not shape strata or prospect." Step P2 (the term-lists plugin,
prospect branch `phase4/term-lists`, strata branch `phase4/term-lists-rank`) is abandoned: it made
the index 71% larger to serve `_terms` aggregations and `DocView::terms`. Xapiand's `_terms`
aggregation is served from stored values (P1), as Elasticsearch's terms aggregation reads doc
values; `DocView::terms`, `term_details` and the `:info` term view are not offered on the strata
engine (refused with a clear error). S3 drops its term-list dependency.

**Ruling R-RM (owner, 2026-10-28): remove, don't refuse.** "If we won't have them naturally, remove
them completely from Xapiand (not simply "refuse" them). No point in having features that are
"disabled" or "refused"." A feature the strata engine does not serve from its own primitives is
deleted from Xapiand master on both engines: code, DSL keyword, docs examples and tests. This
supersedes R-P2's "refused with a clear error": `DocView::terms`, `term_details` and the `:info`
term view are deleted, as are ELITE_SET and `_terms` over text fields (no stored values to read).
`_terms` over value-backed fields stays. The `xapiand` branch, the final Xapian-only release,
keeps them. T11's target stays at most half (owner, same day): 0.80× CPU at gate 7a fails it.

