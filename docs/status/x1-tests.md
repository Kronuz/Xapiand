> Status: CURRENT (2026-10-29) x1 unit tests on strata only, branch `phase7c/x1-delete-xapian`; workstream x1-tests, developer. All 25 ctest targets build clean in build-debug and pass; no sanitizer run (the lead does).

# x1: C++ unit tests with Xapian gone

## State

Every target under tests/ builds and `ctest --test-dir build-debug` passes (25 of 25). The converted tests
check strata against what the test knows (a model, a count, a literal), never against a second engine.
The ENGINE_CENSUS variant of test_engine_census and the FUZZ-only libFuzzer targets were not run (below).

## Done

| Commit | What |
|---|---|
| Xapiand:1848ed81e | test_engine_strata, test_wal_replay, test_pool, test_offload_followups |
| Xapiand:4f7c37fd2 | tests/engine_support.h; test_engine_callbacks, test_engine_query, test_engine_sort |
| Xapiand:0c6dfda16 | test_engine_document, _fields, _read, _replay, _error, test_generate_terms, test_engine_census |
| Xapiand:a15e0b3f1 | test_engine_search_strata, test_text, fuzz_text |
| Xapiand:31b20ddd8 | pin-test directory name and census field names (found by the first full ctest) |

ctest: 25 of 25 passed. Checks per test (the `OK: N checks passed` line): test_string 1, test_debounce_or_run 18,
test_range_terms 321553, test_fieldparser 32, test_serialise_list 24, test_endpoint 16, test_serialise 9,
test_generate_terms 50, test_geospatial 5, test_engine_document 7734, test_engine_fields 20863, test_engine_census
0 (census is off by default: it prints "census is off"), test_engine_query 16454, test_engine_sort 40851,
test_engine_read 217, test_engine_search_strata 472571, test_engine_callbacks 80763, test_engine_error 131,
test_engine_replay 3, test_engine_strata 9737, test_wal_replay 5, test_pool 109, test_blocking_guard 14,
test_offload_followups 10, test_text 24523. xapiand_fuzz_text (5 s, seed 1: 55200 texts, no violation) and
xapiand_fuzz_body (3 s) ran as a smoke.

Mutants (one line in src/, reverted right after; the tests failed with it, `git status` shows src/ as before):

| Mutant | Caught by |
|---|---|
| src/engine/strata/pattern.cc `wildcard_matches`: a `?` no longer matches any character | test_engine_query (`al?ce`, `a?*e` sets) and test_engine_callbacks (16 failed checks: the single-character patterns of the model) |
| src/engine/strata/shard.cc `Shard::scan`: `did < from` became `did <= from` | test_engine_read (10 failed checks: scan from 1, 2, 4, ... and the resumed scan) |
| src/engine/strata/document.cc `add_posting`: `entry.wdf += wdf_inc` became `+ 1` | test_engine_document (1004 failed checks of the model) and test_engine_fields (2809) |

There is no src/ fix in this branch step, so no revert check applies.

## Conversion rules applied

Kind, XAPIAND_ENGINE, node/replication/pool revision arguments and the removed exception types are gone from
the tests. A shard needs a key and refuses the empty field name, so test documents set a key and name
their fields. The version slot (1) is the shard's, so test documents use other slots (21 and up).
Shared helpers: tests/engine_support.h (a scratch strata shard, `add_keyed`, a search returning ids, a
DocView over a map).

## Deleted tests and the coverage they carried

| Test | Coverage it carried | What stands in for it |
|---|---|---|
| test_engine_document: test_matches_xapian | built the same serialised bytes as the Xapian calls | test_matches_model: random calls against a model of terms, values and data, plus the serialise/unserialise round trip |
| test_engine_document: test_classify, test_translation | Xapian exception to errc kind classification, `rethrow_translated` | test_engine_error: test_translation of `strata::rethrow_translated` for std exceptions (no per-Xapian-class table) |
| test_engine_document: test_view, test_word_breaks | Xapian make_view; ICU word-break term generator | none (ICU and the adapter are gone) |
| test_engine_error: test_accessor_table, test_translation_passes_others | one row per Xapian exception: kind and the three texts | none (strata has its own `rethrow_translated` test above) |
| test_engine_error: test_view_throws_engine_errors | every DocView method throws errc::closed after `database()->close()` | none; the fenced and cancel tests in test_engine_strata cover the strata analogue (errc::database) |
| test_engine_error: test_http_answers_unchanged | the old Xapian-typed HTTP dispatch as an oracle | test_http_answers: a table of status and text for each errc kind and the client-error classes, from http_utils.h |
| test_engine_error: test_double_bytes_unchanged | `engine::serialise_double` equal to `::serialise_double` | test_double_bytes: re-pinned to the plain little-endian IEEE form, with literals |
| test_engine_fields: test_xapian_terms_are_joined, test_marker_and_joined_ambiguity, key_field joins, word-break fields | field+term joined byte for byte into Xapian terms | test_field_terms_match_model, test_marker_and_field_boundary, test_key_field (strata keeps field and term apart) |
| test_engine_fields: query-node descriptions | `to_xapian(Query)` description equals the hand-built Xapian query | test_query_nodes: the node fields (field, str, wqf, pos, limits, flags, distance) |
| test_engine_query: test_random_trees | the seam simplifies like Xapian over 2000 random trees | test_emptiness: a table of operator by operand emptiness |
| test_engine_query: test_text | `parse_text` against Xapian::QueryParser descriptions | test_text here finds the documents a text names on a shard; test_text.cc checks the recorded decisions |
| test_engine_query: test_adapter_bounds | PostingSourceAdapter reports max weight, term-frequency bounds | none (the adapter is gone); ranking by the source's weight is in test_posting_source |
| test_engine_query: registry and remote-protocol parts of the sources, key makers, spies and source ownership | serialise to a Xapian registry and back, NetworkError on garbage | the same objects rebuilt through `unserialise`, `engine::Error` on garbage, live-count checks on searches |
| test_engine_callbacks: every Xapian side | ranking, weights and estimates equal Xapian's | the model of the corpus (documents, estimate, spy contents, sort orders) and the weights of a search over shards equal to those of one shard holding every document; the exact weights stay in test_engine_search_strata's BM25 model |
| test_engine_sort: adapter and registry parts | KeyMakerAdapter/MatchSpyAdapter/registry ownership | test_callback_ownership counts live callbacks across searches over one and three shards |
| test_engine_read: test_views_do_not_leak, test_native | `xapian::live_views()` counter, `native<T>()` | the scan-resume test stays; no live-view counter exists on strata, so view leaks are covered only by the ASan run |
| test_engine_replay: test_plain_handle, test_flushed_transaction, test_unflushed_transaction | a Xapian writer's pending writes are invisible to other handles until it is dropped; a plain commit is refused inside a transaction | none as such: strata handles share the writer's index; test_engine_strata covers commit_on_drop, drop_mid_transaction and cancel_transaction |
| test_engine_strata: the Xapian runs of shared, replay, drop_mid_transaction, check, version_slot; the foreign-document refusal; the `kind_of` probe | the same scenarios on Xapian | the strata runs stay |
| test_engine_search_strata: the Xapian index and its comparison; the Xapian syntax-error answer for alternatives; the two-engines-in-one-search refusal | agreement with Xapian | the model checks of every query stay |
| test_offload_followups: test_last_seen_refreshes_while_worker_busy, test_touch_sender_ignores_local | `Node::touch_sender` and liveness (cluster) | the drain tests stay |
| test_text: test_lowering_is_the_tree (weakened, not deleted) | the lowered query equals the tree's description via Xapian | the lowered query has the leaves queryparser's description names (a term for each `@position`, a node for each WILDCARD and EDIT_DISTANCE) over the 5000 rows; the 706 recorded decisions are checked against the parser's own description. This is weaker than equality of the whole tree. |
| fuzz_text | same weakening: the leaves of the lowered query against the leaves of the tree text | |

## Test findings

1. src/engine/strata (document and shard): `Document::field("")` is accepted by the builder, but a shard write with
   an empty field name fails with `errc::invalid` ("The index rejected the operation"), and
   `Document::unserialise` of such a document fails with `errc::syntax` ("field name"). The builder should refuse
   it when the field is declared, or the shard should accept it. Not patched; tests avoid empty field names where
   they write or round-trip.

## Not run

- test_engine_census compiles with `-DENGINE_CENSUS` (syntax-only compile of the test) but was not linked or run
  with the census on: that needs a build with ENGINE_CENSUS=ON. Its shard test now spells the terms as a field and
  a term ("_id." and a number; "w" and "ord") because of finding 1; the expected counters were not re-derived.
- fuzz_strata_document.cc and fuzz_strata_pattern.cc (FUZZ=ON only) and strata_check_tool.cc were syntax-checked
  with the test compile flags; the libFuzzer targets were not built or run.

## Next

- The lead decides on the weakened lowering check and the dropped Xapian-writer replay semantics (rows above).
- Run the sanitizers on the converted engine tests (the lead does).
- Build once with ENGINE_CENSUS=ON and run test_engine_census.

## Blocked on

Nothing.
