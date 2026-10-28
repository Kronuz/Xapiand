# strata-engine-search (S2, phase 7a)

> Status: CURRENT (2026-10-27) workstream `strata-engine-search`; branch `phase7a/strata-engine-search` from `phase7a/strata-engine-write` (Xapiand:b078d5a18). Local only, not pushed, no CI run.

Design: `strata/docs/design/xapiand-on-strata.md` (strata:9dc5fd3), row S2, G5, G7, D13, T6, T7, T11.

## State

Built and proven; waiting for the code review. Local only: not pushed, no CI run (the lead watches CI).

## Done criteria (design row S2)

`engine::search` on the strata engine for term, boolean, phrase and near nodes; estimates; `refresh` before
search; docid tie-break; the default node's five shards searched as one index.

1. `engine::search` dispatches on the shards' engine; a search over indexes of two engines is refused (`invalid`).
2. Terms, `match_all`, AND, OR, AND_NOT, AND_MAYBE, FILTER, SCALE_WEIGHT, NEAR and PHRASE over terms, append chains.
3. One snapshot over every segment of every shard: Xapian's docid interleave `(did - 1) * n + shard + 1`, global
   statistics, prospect's `bm25_xapian` weights.
4. The estimate is the exact count of live matches (G12); the write buffer is flushed before a search.
5. Ties by docid in the requested order (ascending, descending).
6. Refused with a client error that names it (`errc::syntax`, so the handler does not retry): sorter, collapse,
   similar documents, cutoffs, XOR, MAX, SYNONYM, ELITE_SET, wildcard, edit distance, posting sources, query text,
   a PHRASE over anything but terms or with a window other than its length.
7. A spy is shown every live match once (pulled forward from S3, see below).
8. Tests shared with Xapian; docs suite; ASan and UBSan; mutants; smoke. `docs/mechanisms/strata-search.md`.

## Done

- Xapiand:90cfd9775 the search, `Shard::pin`, the dispatcher, the tests; Xapiand:544b0ecb0 spies and the
  `harness/strata_search_check.sh` screen; Xapiand:e4e29fb16 the append fix and the key-prefix function; the last
  commit holds this ledger and the harness script.
- **Spies pulled forward.** The server gives every body search an `AggregationMatchSpy` and reports its
  `_doc_count`, so refusing spies answered 5 of 96 docs-suite searches. The spy now sees each match with its id
  in its shard, weight and stored data; values read empty (P1) and term lists are refused (P2), as a strata
  `DocView` does. Aggregations that read values return empty buckets until P1; S3 owns the rest of spies.
- **Bug found by the server, not the unit tests.** The server builds every query as appends onto an empty query
  and the first version joined that empty side as "matches nothing": no search found anything. Fixed in
  Xapiand:e4e29fb16; test `test_append`; the mutant that restores the old join is caught by 93 checks.
- **Unit test** `tests/test_engine_search_strata.cc` (227,719 checks, passes): one random corpus on a Xapian and
  a strata index (1, 2, 3 and 5 shards, several commits so several segments, boolean weightless terms, positions);
  random trees of every supported node (900+ queries) plus directed phrase/near/append/spy/delete/replace/refusal/
  deep-query/20,000-append/concurrency cases. Documents, estimate, rank and weights of the strata engine are
  checked against an independent model of Xapian's BM25 (worst relative weight difference 4.2e-16, tolerance 1e-9).
- **Revert checks / mutants** (`.scratch/mutants.py`, each caught): interleave (2,490 checks fail), FILTER weighs
  (308), descending ignored (200), deleted documents visible (5), pruning drops ties (141), no refresh before search
  (6), AND_NOT as AND (2,879), estimate skipped (9,805), local statistics (crash: a term outside the snapshot),
  OR needs both (811), PHRASE as NEAR (2,857), append joins nothing = the fix reverted (93), spy sees nothing (2),
  rank offset (101,061), first ignored (1,632).
- **Xapian engine unchanged**: `test_engine_search` 4,417, `engine_census`, `engine_strata` 8,081, `engine_read`,
  `engine_shard` pass. The docs suite on the Xapian engine is 576 of 576 assertions.
- **Docs suite on a strata-backed index** (`.scratch/e2e.sh`, newman 6, one node per engine, 339 requests, 576
  assertions): Xapian 576 pass; strata 346 pass, 230 fail. Every strata failure is a refusal that names a later
  step; the only wrong-looking answers are aggregations over values answered with empty buckets (values await P1). Pending: INFO 28 requests (term lists, P2); sorted results 24 (S3 key makers, P1);
  query text 22 (S4); posting sources 6 (S3, P1); aggregations that read values 24 searches answered 200 with empty
  buckets (P1); wildcards 2, edit distance 1, ELITE_SET 1 (S3). Searches served: 13 of 96 (5 before spies).
- **Query smoke** `harness/strata_search_check.sh`, five shards through the server: term, `_and`, `_or`,
  `_and_not`, `_and_maybe`, `_filter` and a second term field return the document sets Python computes, on both
  engines (GREEN). Integer fields only: a string value in the query DSL is a `text` node until S4.

## T7 list (differences from the Xapian engine, for the lead)

1. **Statistics count deleted and replaced documents** until a merge drops them (a prospect rule); Xapian excludes
   them. Weights drift after deletes and replacements; the documents found do not.
2. **Estimates are exact counts**; Xapian's are estimates unless `check_at_least` covers the matches. A spy sees
   every match; Xapian's sees the documents it examined (`check_at_least`).
3. **`percent`** is the weight over the best weight, rounded; Xapian scales by the weight the matched terms could
   give. Only shown under `comments`.
4. **Xapian disagrees with the sum-of-weights semantics** (which the strata engine and the model follow, and which
   equal Xapian wherever Xapian agrees) in rare shapes: in the random queries, 14 with repeated terms, 7 of distinct
   terms using AND_MAYBE, FILTER, PHRASE or NEAR, and 1 in queries with weightless boolean terms returned a
   document set the model does not (e.g. `(zzz0 OR w0) FILTER (tag3 OR (bool4 AND_NOT tag4) OR w1)` matches a
   document with `w0`, `bool4`, `tag4`, none of the right side's three branches). Xapian's source was not read; these
   are recorded as observed.
5. A search over a Xapian and a strata index together is refused.

## Dependencies and decisions

- **Key prefix (lead's note, 2026-10-27).** Nothing here routes by the key prefix: terms are looked up in the
  dictionary, and `term_key_prefix()` in `src/engine/strata/search.cc` is the one function to change when
  strata's fields-and-keys redesign (strata `design/fields-and-keys`) lands. `Shard::identity()` and `Shard::pin()`
  are the other seams; `pin()` is where the snapshot is taken.
- **Prospect pin** is `phase4/xapian-ops` (prospect:c60547f, pushed) for `scale_scorer`; re-point to main when P3
  lands. **P4** (`phase4/host-collectors`) is not started: the multi-shard collection is this file's, a heap with
  pruning one ulp under the k-th weight so a lower id can still win a tie; it moves to P4's collector.
- `rethrow_translated` is repeated in `search.cc` (a copy of `shard.cc`'s) to keep this branch clear of S1's file;
  fold into one when S1 lands.
- The refusal kind is `errc::syntax` (a client error, not retried); `not_available` would be retried five times.
- The lock: a search holds every shard's lock for its duration (pinned in address order). T10 (concurrency)
  replaces it with a snapshot that outlives the lock.
- No fuzz target: this change adds no decoder or parser; it reads the in-memory `Query`. The depth guard and
  a 20,000-append chain are tested.

## Next

Code review; rebase onto S1 (`phase7a/strata-engine-write`) when it moves; S3 (key makers, spies, expansions,
operators) and S4 (text).

## Blocked on

Nothing.
