# engine-query (X3, phase 0.5)

Branch `phase0.5/engine-query` from Xapiand master `201414347`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X3), D4, D13. Builds on X2
(`docs/status/engine-core.md`).

## State

X3 is split in two landable steps. Step 1 is built and proven; the code review (`docs/reviews/engine-query-code.md`, APPROVE WITH CONDITIONS) found a leak and test gaps, and the fix round for CR1 to CR5 is done (below), ready for a re-check. Step 2 is not started.

## The split

X3's done-when is "`engine::Query` and the three callback bases; `query_dsl`, `range`,
`geospatialrange`, `keymaker`, `generate_terms` (query side) and `aggregations` migrated to them
and `DocView`; the Xapian implementation translates and registers sources". The producers of a
`Query` (query_dsl, range, geospatialrange, generate_terms, one site in search_views) are a closed
set that ends at one consumer (`DatabaseHandler::get_mset`), so they move together. The sort and
spy side (keymaker, aggregations) is a second closed set that ends at the same function.

### Step 1: queries and posting sources (this branch)

Done when:

1. `engine::Query` (a value tree, `src/engine/query.{h,cc}`) and the three callback bases
   (`src/engine/callbacks.h`: `PostingSource`, `KeyMaker`, `MatchSpy`) exist, with no Xapian header.
2. The Xapian implementation (`src/engine/xapian/{query,callbacks}.{h,cc}`) translates a `Query` to the
   `Xapian::Query` the same Xapian calls build, and adapts the three callbacks to Xapian's bases
   and its `Registry`.
3. A test guards the translation shapes it names: leaves, binary and list operators (which translate
   to the same query, so the seam does not rely on keeping them apart), windows, expansions and their
   limit kinds, wildcard pattern flags, text, append chains, emptiness over random trees; and each
   adapter: match path, registry, serialisation error, weight and termfreq bounds, source ownership.
4. The `Query` producers are migrated: `query_dsl`, `range` (three sources), `geospatialrange`,
   `generate_terms` (query side), and the index-path query in `search_views.cc`. They name no
   `Xapian::Query`, `Xapian::termcount`, `Xapian::QueryParser` or `Xapian::ValuePostingSource`.
5. `DatabaseHandler::get_mset` and `get_rset` take an `engine::Query`; the one place that reads the query
   text (`to_xapian`) is where the old code read it, outside the retry loop and before any checkout.
6. The remote server registers the three sources through the adapter, so a remote shard rebuilds them
   by name as before.
7. Behaviour is unchanged: unit tests pass; docs E2E has the same 339 requests and 576 of 576
   assertions before and after, with no status difference, and the same response bodies apart from
   volatile fields; a 301-answer query corpus is byte-identical before and after.
8. ASan+UBSan: unit tests, E2E and the query corpus show no report beyond the third-party lz4 and
   rapidjson lines the baseline also shows.
9. A revert check for each guarded translation that is observable: the mutants below, each caught by a
   test except the binary-as-list form, which is equivalent (test_binary_equals_list shows the same
   description, matches and weights for every binary operator over operands that include empty ones).

### Step 2: sort keys and spies (not started)

Done when:

1. `Multi_MultiValueKeyMaker` derives from `engine::KeyMaker` and reads a `DocView`; `AggregationMatchSpy`
   derives from `engine::MatchSpy`; `metrics.h`, `bucket.h`, `bucket.cc`, `metrics.cc`, `aggregations.{h,cc}`
   and `keymaker.{h,cc}` take a `DocView`, not a `Xapian::Document` (about 190 `Xapian::` references).
2. `DatabaseHandler::get_mset` takes the sorter and the spy as engine types and wraps them in the
   adapters at the one call into Enquire; the remote server registers them through
   `register_key_maker` and `register_match_spy`.
3. Aggregations, sort, collapse and remote-search behaviour is unchanged: E2E, the query corpus extended
   with sorts and aggregations, `cluster_check.sh 2`.
4. ASan+UBSan and revert checks as in step 1.

## Decisions

1. `Query` is a flat immutable node tree behind a shared pointer, built by named factories
   (`term`, `combine`, `append`, `scale`, `wildcard`, `edit_distance`, `source`, `text`), not by
   constructors that copy Xapian's overload set. Each factory has the semantics of the Xapian call it
   replaces. The binary and list forms of `combine` stay distinct because Xapian builds them differently.
2. `Query::empty()` follows Xapian's simplification of operators over empty subqueries
   (2000 random trees agree with `Xapian::Query::empty()`). A text node is never empty there: only the
   engine's parser knows. So `query_dsl`'s "skip the accumulator while it is empty" is a node,
   `Query::append(op, accumulated, next)`, which the Xapian translation resolves after parsing, with the
   rule the old code had (only the accumulator is skipped).
3. `text` carries a `TextConfig` (prefix, default operator, `text::` flags, stop language, stem language
   and strategy, expansion limits). Flags and strategies are the seam's own constants; the Xapian
   implementation maps them, so no Xapian flag value leaks into `query_dsl`.
4. `PostingSource` is the filter contract: a slot, `accept(value)`, `weight(value)`, `max_weight()`, and
   name, clone, serialise, unserialise. All three sources in Xapiand are value-slot filters, and the
   Xapian adapter keeps `Xapian::ValuePostingSource`'s own iteration, so next, skip_to and check behave
   exactly as before. D4 describes the base as "the virtuals of today's classes" (next, skip_to, check,
   weight); the general cursor has no user in Xapiand and is left out until one exists. See Open
   questions.
5. A parse failure of a text node surfaces from `to_xapian` as `Xapian::QueryParserError`, as before
   (from `get_query`). Kinds for it come with X4.
6. `#xapian_query` in a verbose search response translates again to print Xapian's description.

## Done (step 1)

- `src/engine/query.{h,cc}`, `src/engine/callbacks.h`: `Query`, `Op`, `TextConfig`, `text::` flags,
  `PostingSource`, `KeyMaker`, `MatchSpy`.
- `src/engine/xapian/query.{h,cc}`: `to_xapian(Query)`. `src/engine/xapian/callbacks.{h,cc}`:
  `PostingSourceAdapter` (a `Xapian::ValuePostingSource` that forwards `accept` and `weight`),
  `KeyMakerAdapter`, `MatchSpyAdapter` (owning, or borrowing the host object that holds the results),
  and `register_posting_source`, `register_key_maker`, `register_match_spy`.
- Migrated: `query_dsl.{h,cc}` (no `Xapian::` left), `multivalue/range.{h,cc}` and
  `multivalue/geospatialrange.{h,cc}` (sources derive from `engine::PostingSource`),
  `multivalue/generate_terms.{h,cc}` query side, `server/search_views.cc` (the index-path query),
  `database/handler.{h,cc}` (`get_mset`, `get_rset`), `database/schema.h` (`getQueryParserStemStrategy`
  returns `engine::Stem`), `server/remote_protocol_views.cc` (four registrations).
- Found by the garbage-bytes test: `SerialiseList::size()` was `noexcept` but decodes lazily, so a
  malformed serialised value terminated the process (a remote peer could send one in a source or a
  stored value could carry one). It is no longer `noexcept`; the existing `catch (SerialisationError)`
  sites now see it. Pre-existing on master.

## Proof (step 1)

Baseline is master `201414347` built the same way (ASan+UBSan, RelWithDebInfo, `-DLTO=OFF`,
`-DBUILD_TESTS=ON`, x86_64, `.scratch/build-asan`), binary kept as `.scratch/xapiand-base`.

1. Unit tests, all 14 ASan+UBSan: pass. New `test_engine_query`: 22091 checks (leaves and operators
   against the Xapian calls; expansions, including what `first`, `most_frequent` and `error` do past the
   limit on a database; 2000 random trees agree with Xapian on description and `empty()`; `append`; 16
   text cases against `Xapian::QueryParser` set up as `query_dsl` set it up; the posting source through
   every match path and through a `Registry`; Xapiand's three real range sources by name through a
   registry; key maker and spy; 16000 garbage and mutated serialisations).
2. Docs E2E (`.scratch/e2e.sh`): base 339 requests, 576 of 576 assertions GREEN; final 339 requests,
   576 of 576, `e2e_assert` GREEN, `e2e_diff` against the base GREEN with 0 status differences.
   `bodydiff` over 339 bodies: 2 differ, both volatile (a generated id, the metrics page).
3. Query corpus (`.scratch/qdiff.py`, 301 answers: text, stop words, stemming, wildcard, partial, fuzzy,
   ranges on integer, float, date, geo, every compound operator, sorts, `check_at_least`, `?q=`
   strings, relevance order with comments, and 48 error answers): 0 of 301 differ between base and final,
   error messages included.
4. Two nodes with `--replicas 0` (`.scratch/cluster_range.sh`, `harness/cluster_check.sh` plus range
   searches from the node that does not hold the shards): base GREEN, final GREEN. Without replicas the
   search goes over the remote protocol; with the default replicas it is answered locally and the check
   proves nothing about registration.
5. ASan+UBSan on the final node in the E2E run and the corpus run: 0 AddressSanitizer reports; the
   UBSan lines are the lz4 and rapidjson ones the base shows (4 and 4 and 2 and 2 across the two logs,
   the same two kinds as the base's 2 and 2 and 1 and 1).
6. Seam metric (files and lines naming `Xapian::` or including `xapian.h` or `xapian/`, outside
   `src/xapian` and `src/engine/xapian`): master 52 files, 1297 lines; final 46 files, 889 lines.
   `query_dsl`, `range`, `geospatialrange` and their headers, and `generate_terms.h`, are at 0;
   `generate_terms.cc` keeps the 16 of the word-break document path.

### Revert checks

Unit mutants (rebuild the test, run it; `.scratch/mutants.py`, output `.scratch/mutants.out`), each caught:
append stops skipping an empty accumulator (120 of 6068 checks fail), wildcard `first` maps to
`most_frequent` (1), text flag PARTIAL unmapped (1), adapter `check` ignores `accept` (2), adapter
`next` does not filter (4), AND_NOT with an empty left is not empty (28), engine error escapes the
adapter (abort), stem strategy `all_z` maps to `some` (1), key maker adapter returns no key (1), spy
adapter drops `merge_results` (1), scale factor ignored (277), edit-distance prefix length dropped (1).
Two survived the first test version (the limit kind and the stem strategy: Xapian's description shows
neither) and are caught by the database and capital-letter cases added after.

Migration mutants against the query corpus (`.scratch/qmutants.py`): the fuzzy flag dropped from text
(7 of 301 answers differ), `>=` to `>` in the GE source (3), `append` not skipping an empty accumulator
(195). A partial-expansion limit kind swap changes no answer in this corpus (fewer terms than the limit);
the unit test for the limits covers it.

Remote registration: with `--replicas 0`, removing the `MultipleValueRange` registration makes the range
checks fail (2 of 4: counts -1); restored, they pass. The garbage-bytes test aborted the process on the
first input before the `size()` fix (an uncaught `SerialisationError`), and passes after.

Not done: a libFuzzer target (Apple clang ships no libFuzzer, and the sanitizers instrument the whole
tree; the mutational test in `test_engine_query` is the fuzzing here), TSan, Linux and FreeBSD builds,
and a benchmark.

### Fix round (review CR1 to CR5)

1. CR1, the leak: the translation now hands Xapian the adapter with `release()`, as the range sources
   were handed before the seam. New `test_source_ownership` counts live sources across 20 translations
   plus searches, a combined query and a registry round trip: only the seam query's own source stays
   alive, and none after it. Revert (`new PostingSourceAdapter` without `release()`): the live-count checks fail
   (`.scratch/mutants2.out`). Probe `probe_eq leak`: `live=1` while the query lives, `live=0` after.
2. CR2, tests: `test_wildcard_patterns` (revert of PATTERN_SINGLE: 2 checks fail; PATTERN_MULTI: 2),
   `test_adapter_bounds` (termfreq_min(0) removed: 1; max_weight dropped: 3), `test_fuzzy_limit` (limit
   kind ERROR to MOST_FREQUENT: 2), `test_binary_equals_list` (binary as list: survives, equivalent).
   Finding behind R2 and R3: the adapter set both bounds in `init()` and in the constructor, but
   Xapian binds a `ValuePostingSource` through `reset(db, shard)`; the base `init()` throws
   `InvalidOperationError`, and `reset` sets the weight bound to unbounded. So `set_termfreq_min(0)`
   never ran and the `max_weight()` bound never reached the matcher, on master too (the geo source's
   bound included). The adapter now overrides `reset` and sets both after the base call, so
   `matches_lower_bound` can no longer exceed the real count and `get_max_possible` is the source's
   bound. Only bounds change; the 301-answer corpus is still byte-identical.
3. CR3: slot, start and end are read into named locals in order in the three `unserialise` functions.
   No test can fail on this host (clang evaluates left to right); a reversed read (mutant) fails
   `test_registered_sources` (2 checks).
4. CR4: `to_xapian` folds a chain of appends in a loop (the same Xapian tree), and `~QueryNode` frees
   uniquely owned children from a list, not by recursion. `test_append_chain` builds 600 appends on a
   256 KB stack and compares a short chain with the left fold. Reverts: recursive translation and
   recursive destructor both stack-overflow. Probe `deep` on a 512 KB stack: translation and destruction
   pass at 2000 (the branch overflowed in `to_xapian` at 1000 before); a search passes at 1500 and
   then fails inside Xapian's `postlist_sub_and_like` at 2000. `flatmap.py` against a node: 1000 keys
   answer 200 on master and branch, 2000 keys abort in Xapian's `QueryBranch::get_length` on both. The
   branch has master's headroom; what remains is Xapian's own recursion (and `Query::description()`,
   for logs).
5. CR5: `accept` of the three range sources and the geo source returns false on a value that does not
   decode (the geo `weight` falls back to the no-centroid weight). `test_corrupt_stored_value`: 20000
   random values per source, a search over a slot holding one corrupt value finds the other documents.
   Reverts: range `accept` without the catch aborts the process; geo `accept` fails the test. Probe
   `probe_eq garbage`: 200000 random stored values, no terminate. The contract is in `callbacks.h`.
6. Evidence on the fixed head: 14 of 14 unit tests under ASan+UBSan (`test_engine_query` 22395
   checks); the 301-answer corpus identical to master (`cmp` of `qd-base.json` and the new run, 253
   200s, 0 AddressSanitizer reports); docs E2E 339 requests, 576 of 576 assertions GREEN, 0 reports;
   2-node `cluster_range.sh` GREEN (range counts 11, 6, 4, 2). Revert script `.scratch/mutants2.py`,
   output `.scratch/mutants2.out`.

## Rulings and follow-ups

1. PostingSource ruling (review R-A): the value-slot filter contract is accepted. A cursor-style source
   (`next`, `skip_to`, `check`) would be a new base beside it, added when a user needs it. The contract
   (const, thread-safe `accept`, `weight`, `clone`; `accept` false on an undecodable value; a selectivity
   `estimate` planned, not built) is in `callbacks.h`.
2. CR6 (follow-up): `get_mset` translates the same query up to three times for `nearest` and `fuzzy`;
   translate once and pass the `Xapian::Query` down. Belongs with X4, which moves that boundary.
3. CR7 (follow-up): the `KeyMaker` and `MatchSpy` adapters are scaffolding until step 2; revisit the
   `DocView` signatures then.
4. CR8: the disclosure on `remote_protocol_views.cc` (open question 2) is accepted by the review.

## Commits

- Xapiand:801f16f5c Add engine::Query and the callback bases, and migrate the query producers
- Xapiand:868af2635 Test the engine query seam hard, and keep SerialiseList::size() from aborting
- Xapiand:c281e9618 Record the commits of engine-query step 1 in the ledger
- Xapiand:a0926598b Review engine query (code review)
- Xapiand:db0be79ad Fix the engine query review findings CR1 to CR5
- (ledger update in the commit after these)

## Next

1. Step 2 as above.
2. X4 moves the `Xapian::Query` boundary in `get_mset` (the ESet terms for nearest and fuzzy, `Enquire`)
   behind `engine::search`, and error kinds for `QueryParserError`.

## Open questions

1. D4 and the `PostingSource` shape (decision 4): the code review accepted the filter contract
   (see Rulings).
2. Disclosure: while migrating the remote server's four registration lines I listed lines 47, 48 and
   120 to 123 of `src/server/remote_protocol_views.cc` by accident (a `git grep` for the source class
   names). Nothing from them was copied beyond the call shape; the edit is a mechanical rewrite of those
   four calls and one include.

## Blocked on

Nothing.
