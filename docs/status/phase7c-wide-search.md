> Status: CURRENT (2026-11-02) the wide-search developer's ledger (phase 7c: bounded-concurrency searches in batches); local branch `phase7c/wide-search` from master `Xapiand:11e3d82a4`, not pushed.

# phase7c wide search (build): a search walks its indexes in batches

Branch `phase7c/wide-search` in `Xapiand-wt/wide-search`. Source: `strata/docs/status/road-to-release.md`, "Wide searches"; mechanism `docs/mechanisms/wide-search.md`.

## State

Code written; building. See Done.

## Done criteria

- [ ] `--max-indexes-per-search` removed (code, tests, docs, changelog); `--concurrent-search-indexes` (K) and `--max-search-indexes` (100,000, 400 above) added with docs and a changelog bullet.
- [ ] A search walks its endpoints in batches of at most K, merging top-k as each batch ends, behind one seam (`engine::BatchRunner`, `DatabaseHandler::search_batch`).
- [ ] Identity test over 1, K-1, K, K+1, 3K+2 indexes: hits, ids, order, totals, aggregations (spy).
- [ ] Gate harness `harness/wide_search_check.py gate` (10,000 indexes: p50, p99, peak pinned indexes); one smoke run at 1,000.
- [ ] Proof: Release ctest; ASan+UBSan ctest; docs E2E (newman 6.2.3); revert check; mutant; `async_check --strict --owed --extern strata=<pin>` at 0 owed.

## Decisions made without the owner (for the lead to confirm)

1. K defaults to the pool size over the HTTP loops ("a reactor's share of the open indexes"), my reading of "the per-reactor reader cap"; `--concurrent-search-indexes` overrides it.
2. Relevance weights are read against a batch's statistics (prospect reads the statistics of the shards it is given, and cannot be handed global ones); a search that fits in one batch is unchanged. Documented in options.md and the mechanism note.
3. Hit ids stay 32-bit (`engine::docid`), so a search whose id `(d-1)*total+position+1` passes 2^32 is refused (`errc::internal`), as before. At the 100,000-index cap that is 42,949 documents per index. Widening `docid` is a separate change.
4. Ids number by endpoint position, which is what `get_document` decodes; the old path numbered by the shards that existed, so a search with a missing index returned ids that read back from the wrong shard. The new numbering fixes that.

## Blocked on

Nothing.
