> Status: CURRENT (2026-11-02) the wide-search developer's ledger (phase 7c: bounded-concurrency searches in batches); local branch `phase7c/wide-search` from master `Xapiand:11e3d82a4`, not pushed.

# phase7c wide search (build): a search walks its indexes in batches

Branch `phase7c/wide-search` in `Xapiand-wt/wide-search`. Source: `strata/docs/status/road-to-release.md`, "Wide searches"; mechanism `docs/mechanisms/wide-search.md`.

## State

Built and proved; master (gate-prep, then drop-threadpool Xapiand:88892b4cb, strata pin 3f564dc5) is merged in and the whole proof reran green on the merged head. Local only; ready for review.

## Done criteria

- [x] `--max-indexes-per-search` removed (code, test, harness, docs, changelog); `--concurrent-search-indexes` (K) and `--max-search-indexes` (100,000, 400 above) added with `options.md`, `reader-views.md`, `wide-search.md` and a changelog bullet. Old status and review files keep the old name as history.
- [x] A search walks its endpoints in batches of at most K, merging top-k as each batch ends, behind one seam (`engine::BatchRunner`; `DatabaseHandler::search_batch` is the one function that pins a batch).
- [x] Identity test `tests/test_wide_search.cc` over 1, K-1, K, K+1, 3K+2 indexes (K = 4): hits, ids, order, ranks, totals, spy results, against one search of all and against the engine's own search; match_all and term queries, keyed sorts with ties, both docid orders, windows, collapse. 2817 checks. A non-vacuity block asserts the stored values reorder (key) and drop (collapse) hits.
- [x] `harness/wide_search_check.py check`: the same over HTTP between a server that reads all in one batch and one with K = 4; plus the cap (`--max-search-indexes 7`: 7 served, 8 is 400) and the peak gauge (14 vs 4).
- [x] Gate harness `harness/wide_search_check.py gate` (creates N indexes, searches across all, prints p50, p99 and `xapiand_search_pinned_indexes_peak`). Smoke at 1,000 indexes (Release, `quiet strict`, host loaded by other agents, 20 searches): p50 49 ms, p99 52 ms, peak 100 pinned (K = pool 200 / 2 loops). The 10,000 run is the lead's.
- [x] Proof on the merged head: see Proof.

## Proof (merged head, identical results before and after the merge; Release and ASan+UBSan builds in `.scratch/build-rel`, `.scratch/build-asan`)

| | Release | ASan+UBSan (LLVM 22, `detect_leaks=0`) |
|---|---|---|
| ctest | 35 of 35 | 35 of 35 |
| docs E2E (newman 6.2.3) | 374 requests, 644 of 644 assertions | 644 of 644, no sanitizer line in the node log |
| `wide_search_check.py check` | PASS | PASS, no sanitizer line |

- `async_check.sh --strict --owed --extern strata=<pin>`: 247 hits, owed 0, unclassified 0, exit 0; `test_gate_tools.py` OK.
- Mutants on `src/engine/search.cc` (each makes `test_wide_search` fail, and the source is restored): the merge's docid tie-break flipped (fails the `did` checks of every ordered case); `collapse_max + 1` in the merge (fails every collapse case: 37 checks with the window mutant); the window truncated by one (fails every windowed case); the missing-shard renumbering disabled (fails `test_a_missing_index_keeps_the_numbering_of_the_endpoints`).
- Revert check: with `search_batched` forced to one search (no batching), `test_wide_search` fails 26 checks (`widest <= K`).
- No fuzz target: nothing here decodes untrusted bytes (the merge reads engine results).

## Fix round (the lead's four findings)

1. **Wildcards and `_all` expand.** `search_view` and `count_view` expand `_all`, `*` patterns and `-x` exclusions against the node's directories (`src/index_pattern.{h,cc}`), in a `query` chore of the work pool (the reactor never lists a directory), and count the names against `--max-search-indexes` as the walk goes (400 above it, naming the option). An index is a directory holding `strata` or a shard `.__N`; `.`-led names are not listed; a pattern that matches nothing answers 200 with no hits. The resolved endpoints are sorted once (`Endpoints::add` inserted one by one, quadratic for a wide list). E2E examples with assertions are in `reference-guide/api.md`, "Multiple Indexes" (list, wildcard, inside a name, exclusion, `_all`, `COUNT`, no match); `harness/wide_search_check.py` checks patterns against name lists and the cap through `_all`.
2. **Results do not depend on K.** prospect can give per-index weights without a change: `term_table` takes any span of segments, so `strata::run` builds one term table and plan per index (`SearchRequest::index_of` names the index of each shard; the handler groups the endpoints by `unsharded_path`) and each segment builds with its index's. `search_batched` takes whole indexes per batch (an index wider than K is a batch alone). The weight of a document is that of a search of its own index alone (`test_a_weight_is_that_of_the_documents_index`), and the hits, order, weights, percents, totals and spy results are the same for K = 1, K-1, K, K+1, 3K+2 and one batch (`test_the_hits_do_not_depend_on_the_batch_size`), for relevance queries (term, OR, AND, wildcard, synonym), keyed sorts, collapse, both docid orders and windows. A single search over several indexes now weighs per index too, as Elasticsearch does by default.
3. **64-bit hit ids.** `engine::hitid` (uint64) is the id across the shards: `Hit::did`, the merge, `Document` and `DatabaseHandler::get_document`; `split_hit_id` decodes it (and refuses a document no shard can hold). A shard's own docid stays 32 bits. `test_an_id_past_32_bits_is_exact` and `test_a_hit_id_decodes_past_32_bits` sit on the old edge (42,949 documents of each of 100,000 shards).
4. **A long index list.** Decided: patterns, not a body list. A URL holds 64 KiB (about 7,000 six-character names) but `_all` or `wi*` is a few bytes, so the 100,000 cap is reachable by a request: `_all` over 100,000 indexes. A body list would be a second way to say what the path says (with a rule for both), and the Elasticsearch API this follows takes indices in the path and as patterns only. The harness proves the cap through `_all`, `w00*` and an exclusion list at `--max-search-indexes 7`; the gate now searches `/w*/`.

**K default, against the design's reader cap.** `readers_per_index` (16 searches per reactor, `manager.cc`) bounds the readers of one index; a batch takes one record per index it holds, so K does not enter it. K bounds the indexes pinned: one wide search per loop holds at most pool / loops, so the loops together hold the pool's budget (sized from the file-descriptor limit). More wide searches on one loop than one add their batches; the pool is soft (a used entry is never evicted, an overflowing idle one goes after 60 s), so that is a larger working set and shows in `xapiand_search_pinned_indexes_peak`, not a failure. The default stands; written in `wide-search.md`.

## Next

Review (code reviewer; the lead decides on security: no untrusted decoder here). The lead runs the 10,000-index gate.

## Blocked on

Nothing.
