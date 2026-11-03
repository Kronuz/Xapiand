> Status: CURRENT (2026-11-02) the wide-search developer's ledger (phase 7c: bounded-concurrency searches in batches); local branch `phase7c/wide-search` from master `Xapiand:11e3d82a4`, not pushed.

# phase7c wide search (build): a search walks its indexes in batches

Branch `phase7c/wide-search` in `Xapiand-wt/wide-search`. Source: `strata/docs/status/road-to-release.md`, "Wide searches"; mechanism `docs/mechanisms/wide-search.md`.

## State

Built and proved; the four findings of the first review round are fixed (below). Master (gate-prep, then drop-threadpool Xapiand:88892b4cb, strata pin 3f564dc5) is merged in. Local only (never pushed); ready for review. Head Xapiand:f71f1dd46 (fix round Xapiand:5110af2ec to Xapiand:f71f1dd46 over the proved head Xapiand:dfbcc88e9).

## Done criteria

- [x] `--max-indexes-per-search` removed (code, test, harness, docs, changelog); `--concurrent-search-indexes` (K) and `--max-search-indexes` (100,000, 400 above) added with `options.md`, `reader-views.md`, `wide-search.md` and a changelog bullet. Old status and review files keep the old name as history.
- [x] A search walks its endpoints in batches of whole indexes, at most K shards, merging top-k as each batch ends, behind one seam (`engine::BatchRunner`; `DatabaseHandler::search_batch` is the one function that pins a batch).
- [x] Identity test `tests/test_wide_search.cc` (22,301 checks): the same hits, ids, order, ranks, weights, percents, totals and spy results for 1, K-1, K, K+1 and 3K+2 shards at K = 4, and for K = 1, K-1, K, K+1, 3K+2 and one batch over the same 3K+2 shards, against one search of all and against the engine's own search; relevance queries (term, rare term, OR, AND, wildcard by prefix with a SYNONYM and an OR combiner, synonym) next to match_all, keyed sorts with ties, both docid orders, windows, collapse. Non-vacuity: stored values reorder and drop hits; at least 5 relevance queries weigh differently when the indexes are read as one; every case at 3K+2 shards finds something.
- [x] A document weighs as in a search of its own index alone (`test_a_weight_is_that_of_the_documents_index`, 5 queries, up to 126 documents each).
- [x] Hit ids are 64 bits; tests at the old 32-bit edge, one batch and two (`test_an_id_past_32_bits_is_exact`), and the decode (`test_a_hit_id_decodes_past_32_bits`).
- [x] Patterns: `tests/test_index_pattern.cc` (glob, expansion over a directory tree, the limit), `tests/fuzz_index_pattern.cc` (the glob against a table reference: 1.25 million runs in 90 s under ASan+UBSan, no finding), E2E examples in `api.md`.
- [x] `harness/wide_search_check.py check`: two servers (all in one batch, K = 4) agree on sorted and relevance searches (with weights and ids), by name list and by pattern; the cap (`--max-search-indexes 7`) through a list, `_all`, a prefix and an exclusion. `gate` searches `/w*/`.

## Proof (fix round, head Xapiand:f71f1dd46; Release in `.scratch/build-rel`; Linux ASan+UBSan on the shared lease, clang 20, commit Xapiand:9c8da59a0, which differs from the head in harness and docs files only)

| | Release (macOS) | ASan+UBSan (Linux, `detect_leaks=0`) |
|---|---|---|
| ctest | 36 of 36 | 36 of 36 (`test_text` needs `allocator_may_return_null=1`: it decodes a msgpack array header that asks for 128 GiB, and ASan refuses the allocation; unrelated to this change) |
| docs E2E (newman 6.2.3) | 386 requests, 656 of 656 assertions | 656 of 656, no sanitizer line in the node log |
| `wide_search_check.py check` | PASS | PASS, no sanitizer line |

- `async_check.sh --strict --owed --extern strata=3f564dc5`: 255 hits, allow 242, owed 0, compiled_out 13, unclassified 0, exit 0 (the pattern walk is classified: a query chore); `test_gate_tools.py` OK.
- Mutants and reverts (each restored from git; the test or harness named fails):

| fix | mutant (also the revert) | killed by |
|---|---|---|
| 2 per-index weights | `strata::run` builds one scope for all the shards (the old per-batch statistics) | `test_wide_search`: 1,931 weight checks, 1,551 id checks, 312 oracle checks; the harness (every relevance and sorted search over 5 and 14 indexes) |
| 2 whole indexes per batch | `batch_end` ignores the index boundaries | `test_a_batch_holds_whole_indexes`, `test_the_hits_do_not_depend_on_the_batch_size` (522 weight checks) |
| 3 64-bit ids | the merge narrows the id to 32 bits | `test_an_id_past_32_bits_is_exact` |
| 3 decode | `split_hit_id` truncates | `test_a_hit_id_decodes_past_32_bits` |
| 1 expansion | the view does not expand | docs E2E (5 assertions of "Multiple Indexes") and the harness (patterns, caps through `_all`) |
| 1 closing slash | the last name keeps its `/` | the same E2E assertions and harness checks (found by the first E2E run: a pattern ending a path never matched) |
| 1 walk | no descent into containers; exclusions ignored; no limit in the walk | `test_expand`, `test_the_limit` (9, 4 and 2 checks) |
| 4 cap reachable | (the `_all` / prefix / exclusion cap rows of the harness) | the no-expansion mutant above |

- Fuzz: one target (the glob). The directory listing reads names, not bytes of a format, and takes no decoder of its own.
- Earlier mutants (merge tie-break, collapse window, window truncation, missing-shard renumbering, and the unbatched revert of Xapiand:dfbcc88e9) are unchanged; `test_wide_search` still holds those checks.

## Fix round (the lead's four findings)

1. **Wildcards and `_all` expand.** `search_view` and `count_view` expand `_all`, `*` patterns and `-x` exclusions against the node's directories (`src/index_pattern.{h,cc}`), in a `query` chore of the work pool (the reactor never lists a directory), and count the names against `--max-search-indexes` as the walk goes (400 above it, naming the option). An index is a directory holding `strata` or a shard `.__N`; `.`-led names are not listed; a pattern that matches nothing answers 200 with no hits. The resolved endpoints are sorted once (`Endpoints::add` inserted one by one, quadratic for a wide list). E2E examples with assertions are in `reference-guide/api.md`, "Multiple Indexes" (list, wildcard, inside a name, exclusion, `_all`, `COUNT`, no match); `harness/wide_search_check.py` checks patterns against name lists and the cap through `_all`.
2. **Results do not depend on K.** prospect can give per-index weights without a change: `term_table` takes any span of segments, so `strata::run` builds one term table and plan per index (`SearchRequest::index_of` names the index of each shard; the handler groups the endpoints by `unsharded_path`) and each segment builds with its index's. `search_batched` takes whole indexes per batch (an index wider than K is a batch alone). The weight of a document is that of a search of its own index alone (`test_a_weight_is_that_of_the_documents_index`), and the hits, order, weights, percents, totals and spy results are the same for K = 1, K-1, K, K+1, 3K+2 and one batch (`test_the_hits_do_not_depend_on_the_batch_size`), for relevance queries (term, OR, AND, wildcard, synonym), keyed sorts, collapse, both docid orders and windows. A single search over several indexes now weighs per index too, as Elasticsearch does by default.
3. **64-bit hit ids.** `engine::hitid` (uint64) is the id across the shards: `Hit::did`, the merge, `Document` and `DatabaseHandler::get_document`; `split_hit_id` decodes it (and refuses a document no shard can hold). A shard's own docid stays 32 bits. `test_an_id_past_32_bits_is_exact` and `test_a_hit_id_decodes_past_32_bits` sit on the old edge (42,949 documents of each of 100,000 shards).
4. **A long index list.** Decided: patterns, not a body list. A URL holds 64 KiB (about 7,000 six-character names) but `_all` or `wi*` is a few bytes, so the 100,000 cap is reachable by a request: `_all` over 100,000 indexes. A body list would be a second way to say what the path says (with a rule for both), and the Elasticsearch API this follows takes indices in the path and as patterns only. The harness proves the cap through `_all`, `w00*` and an exclusion list at `--max-search-indexes 7`; the gate now searches `/w*/`.

**K default, against the design's reader cap.** `readers_per_index` (16 searches per reactor, `manager.cc`) bounds the readers of one index; a batch takes one record per index it holds, so K does not enter it. K bounds the indexes pinned: one wide search per loop holds at most pool / loops, so the loops together hold the pool's budget (sized from the file-descriptor limit). More wide searches on one loop than one add their batches; the pool is soft (a used entry is never evicted, an overflowing idle one goes after 60 s), so that is a larger working set and shows in `xapiand_search_pinned_indexes_peak`, not a failure. The default stands; written in `wide-search.md`.

## Next

Review (code reviewer; the lead decides on security: `index_pattern` reads a pattern from the URL and lists directories, bounded by a 256-byte pattern, the name cap and `index_path::valid` on every name it lists, but it is the one new path from a request to the file system). The lead runs the 10,000-index gate (`harness/wide_search_check.py gate`, which now searches `/w*/`).

## Blocked on

Nothing.
