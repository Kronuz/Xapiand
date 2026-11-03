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

## Findings for the lead

1. **A path holds 64 KiB** (`http_parser_url` field lengths), so a search by index list reaches about 7,000 six-character names (`wi0` to `wi6999` answers 200; 10,000 answers 400 "Invalid path"). The 100,000 cap is therefore not reachable through the URL. The gate names its indexes in four characters to fit 10,000.
2. **Wildcards and `_all` do not expand in this tree** (`SEARCH /wi*/` and `/_all/` answer 200 with 0 hits), though `api.md` documents them. Pre-existing: `expand_paths` only splits the path. A wide search by pattern needs the catalog or a directory listing.
3. Relevance weights are per batch (decision 2 below).

## Decisions made without the owner (for the lead to confirm)

1. K defaults to the pool size over the HTTP loops ("a reactor's share of the open indexes"), my reading of "the per-reactor reader cap"; `--concurrent-search-indexes` overrides it.
2. Relevance weights are read against a batch's statistics (prospect reads the statistics of the shards it is given, and cannot be handed global ones); a search that fits in one batch is unchanged. Documented in options.md and the mechanism note.
3. Hit ids stay 32-bit (`engine::docid`), so a search whose id `(d-1)*total+position+1` passes 2^32 is refused (`errc::internal`), as before. At the 100,000-index cap that is 42,949 documents per index. Widening `docid` is a separate change.
4. Ids number by endpoint position, which is what `get_document` decodes; the old path numbered by the shards that existed, so a search with a missing index returned ids that read back from the wrong shard. The new numbering fixes that.

## Next

Review (code reviewer; the lead decides on security: no untrusted decoder here). The lead runs the 10,000-index gate.

## Blocked on

Nothing.
