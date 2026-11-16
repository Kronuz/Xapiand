# prospect-supplied-stats review: correctness

Commit: prospect:2202030d297797929985eddabcafef336d43ce65
Verdict: APPROVE
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261115-232146-62bd5a (A1, at 2202030): `stats_test` passes with 4,170 fan-outs equal to the
  merged index (3,552 with hits), 82 corpus rows (xapian-shards s3 and s5, the release-rows sharded
  vectors), 8 random corpora of 3 and 5 shards (skewed, empty shards), 1 to 4 segments per shard,
  with and without deletes. The comparison is `doc_hit ==`: docid and score bits, plus counts, at
  k 1, 10 and 5000, both weights, Lucene-mode phrases; per-term `idf`, `avg`, `inv_avg` and scores
  at sample tf and length; the gathered sum equals the merged index's own gather. 1,015 queries
  differ without supplied statistics, so the test has power.
- 261115-232149-29b412 (A2) and 261115-232153-05152e: all 18 mutants caught, including the
  collection-only, df-only, cf-only, sum, gather-deletes, field-keying and refusal mutants (log read).
- 261115-232155-edc5bb (suite) and 261115-232208-acfc3c / 261115-232211-560561 (A3 A/A, A/B):
  accepted as listed by the lead; not reread for this lens.

## What I checked myself

By reading, at 2202030 and in Xapiand's consumer (`src/engine/strata/search.cc`, `expand.cc` on
master); no probe job was needed: every question below is settled by `file:line` or by the jobs
above. Directory `.scratch/review-prospect-supplied-stats-correctness/` is empty.

1. Every weighting path. prospect ships two weights, `weight::bm25` and `weight::bm25_xapian`
   (search.hh:77-98); both read only `statistics` (doc count, total length) and a df. Inside
   prospect the statistics reach a builder only through `term_table::stats()` and
   `term_table::df(field, term)`, which now answer from the supplied set (searcher.hh:219-226,
   274-283); `phrase_scorer` takes `stats` and per-term dfs from the builder (search.hh:1151-1164).
   Field scoping is by name (`find`, searcher.hh:162-167), tested across fields in `edges()`.
   Per-segment figures (`df(seg, ...)`, `info`, impacts, `exact_count`) stay local, which is right:
   they size scans and counts, and the count equality in A1 covers it. Xapiand's builder reads
   only `terms.stats()` / `terms.df()` (search.cc:150-153, 189-192; expand.cc:132), so a host
   constructing `term_table(segs, &supplied)` routes every weight, SYNONYM's `synonym_df` N, and
   most-frequent expansion ranking through the supplied set. No silent local fallback exists in
   prospect. Expanded terms are the exception, and it is the host's (F1).
2. Missing term. `resolve_term` (searcher.hh:203-208) throws `std::invalid_argument` only when the
   shard holds a term the set lacks; a named term gathered at df 0 that the shard gains later is
   weighed with the stale supplied figure, no throw. For plain terms the coordinator names every
   leaf, so the throw is unreachable except by a host bug; for wildcard and edit-distance
   expansions it is reachable under D10 (F1, F2).
3. Sum. `gather_statistics` assigns (not adds) per named term, so a name repeated in the list is
   not double counted (searcher.hh:193-198); `+=` adds doc count, total length, df and cf
   separately (searcher.hh:169-180). Deleted documents count in both doc count and df, as in one
   unmerged index (D13), on both sides of the A1 comparison; the gather-deletes and cf-is-df mutants
   are caught. Memory is one map node per named term, linear in the query's terms, with no cap in
   prospect; the host's expansion limit bounds it (F4).
4. Tests compare values: hit ids, order and score bits, counts, weight fields, and the sum itself,
   over the fixtures plus random corpora with deletes and multiple segments.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | For distributed-search, not this quest: Xapiand expands wildcards and edit distances per shard from local segments, so with supplied statistics a shard still weighs differently from the merged index: SYNONYM (the default combiner for wildcard, partial and fuzzy) computes `synonym_df` over the shard's local members only; `most_frequent` keeps the top N of the shard's candidates, not the index's; `Expansion::error` fires on the shard's count. | Xapiand src/engine/strata/search.cc:180-192, expand.cc:67-140, query_dsl.cc:732-749 | distributed-search's stats round trip expands at the coordinator (union of shard expansions, limit applied to the merged df), ships the member list with the statistics, and shards build groups from that list instead of `expand()`. prospect's contract already carries what is needed (`supplied_statistics::terms` by field); add an A-row in distributed-search with a wildcard and a partial on 3 and 5 shards. |
| F2 | no | gap | The miss is `std::invalid_argument`, the type prospect uses for malformed queries, and Xapiand maps it to `errc::invalid` (a client error). Under D10's retry "on another replica with the statistics already gathered", or a refresh between the stats round trip and `search_partial`, a shard that expands to a term the gather never saw fails the request as the user's fault. With F1's fix the miss is unreachable except by a host bug and throwing is right; the type still should let the host tell it apart. | searcher.hh:206; Xapiand src/engine/strata/shard.cc:120-121; cluster-release.md D10 (lines 287-297) | A dedicated type in this API (say `prospect::statistics_missing : std::logic_error`), which Xapiand maps to `errc::internal`, so a host bug is loud and never a 400. Cheap now or in distributed-search; owner's or lead's call which quest carries it. A fallback (df 0 as "absent at gather time") is the alternative if F1's fix is not taken: it matches D10's stated staleness residue for named terms. |
| F3 | no | nit | The set is not validated: a supplied df above the supplied doc count gives `bm25_xapian` a negative idf (tw in (-1, 0) becomes (0.5, 1), log < 0), and negative term scores break the non-negative-sum assumption of pruning (`sum_upper`, `max_score`). Unreachable with honest gathers (each shard's df and doc count come from one snapshot), so under the trusted-cluster model it is documented-residue, not a defect. | search.hh:91-95; docs/mechanisms/supplied-statistics.md "The set is not checked" | Optional: refuse `df > collection.doc_count` in `resolve_term`, one compare. |
| F4 | no | nit | Memory of an expanded query is linear in its terms (map node plus two strings per term per shard response and in the sum), uncapped in prospect, and `term_table::column` is a linear scan per lookup (pre-existing), so a 10k-term expansion is O(n^2) lookups. Bounded today by Xapiand's expansion limits. | searcher.hh:158-160, 259-263 | None here; distributed-search should state the bound it relies on (the expansion limits) in its stats message size. |
| F5 | no | nit | `term_table::column` re-sums df and cf inline instead of reusing `snapshot_term_statistics` (it also keeps the infos, so not a one-line swap); and shards.hh:290-292 still says the builder "must weigh with the snapshot's statistics", now one of two options. | searcher.hh:266-273; shards.hh:290-292 | Reword the shards.hh comment to name `term_table` with or without supplied statistics when the file is next touched. |

## Not run

- No probe jobs: no build, no sanitizer run (CI's; `campaign status` shows ASan and CI not yet run
  at 2202030), no A/B rerun.
- Xapiand-side behavior with supplied statistics (F1, F2) is from reading master's consumer; the
  consumer does not yet pass a supplied set, so nothing there was executed.
- Multi-field and key-field terms are not in the A1 identity (one text field); field keying is
  covered only by `edges()`.
