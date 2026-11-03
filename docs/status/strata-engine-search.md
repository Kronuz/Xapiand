# strata-engine-search (S2, phase 7a)

> Status: CURRENT (2026-10-28) workstream `strata-engine-search`; branch `phase7a/strata-engine-search`, rebased by merge onto master (S1, Xapiand:5d7f3263d). Local only, not pushed, no CI run.

Design: `docs/design/xapiand-on-strata.md` (strata:9dc5fd3), row S2, G5, G7, D13, T6, T7, T11.

## State

Built, proven, reviewed (CR1-CR4, CR6 closed), and merged with S1's master; CR2 closed. Ready for the lead's re-check. Local only: not pushed, no CI run (the lead watches CI).

## Done criteria (design row S2)

`engine::search` on the strata engine for term, boolean, phrase and near nodes; estimates; `refresh` before
search; docid tie-break; the default node's five shards searched as one index.

1. `engine::search` dispatches on the shards' engine; a search over indexes of two engines is refused (`invalid`).
2. Terms, `match_all`, AND, OR, AND_NOT, AND_MAYBE, FILTER, SCALE_WEIGHT, NEAR and PHRASE over terms, append chains.
3. One `prospect::multi_snapshot` over every segment of every shard: Xapian's docid interleave `(did - 1) * n + shard + 1`,
   global statistics, prospect's `bm25_xapian` weights; `prospect::search` collects (pruned, or exhaustive for a spy or
   descending ids).
4. The estimate is the exact count of live matches (G12); the write buffer is flushed before a search.
5. Ties by docid in the requested order (ascending, descending).
6. Refused with a client error that names it (`errc::syntax`, so the handler does not retry): sorter, collapse,
   similar documents, cutoffs, XOR, MAX, SYNONYM, ELITE_SET, wildcard, edit distance, posting sources, query text,
   a PHRASE over anything but terms. A PHRASE keeps its terms' query positions as offsets (gaps where stop words stood) and matches the terms at exactly those relative positions.
7. A spy is shown every live match once (pulled forward from S3, see below).
8. Tests shared with Xapian; docs suite; ASan and UBSan; mutants; smoke. `docs/mechanisms/strata-search.md`.

## Done

- Xapiand:90cfd9775 the search, `Shard::pin`, the dispatcher, the tests; Xapiand:544b0ecb0 spies and the
  `harness/strata_search_check.sh` screen; Xapiand:e4e29fb16 the append fix and the key-prefix function;
  Xapiand:1791c7961 flat scorers for append runs (S3's stack-overflow finding); the last commit holds this ledger.
- **Spies pulled forward.** The server gives every body search an `AggregationMatchSpy` and reports its
  `_doc_count`, so refusing spies answered 5 of 96 docs-suite searches. The spy now sees each match with its id
  in its shard, weight and stored data; values read empty (P1) and term lists are refused (P2), as a strata
  `DocView` does. Aggregations that read values return empty buckets until P1; S3 owns the rest of spies.
- **Bug found by the server, not the unit tests.** The server builds every query as appends onto an empty query
  and the first version joined that empty side as "matches nothing": no search found anything. Fixed in
  Xapiand:e4e29fb16; test `test_append`; the mutant that restores the old join is caught by 93 checks.
- **Unit test** `tests/test_engine_search_strata.cc` (227,724 checks, passes): one random corpus on a Xapian and
  a strata index (1, 2, 3 and 5 shards, several commits so several segments, boolean weightless terms, positions);
  random trees of every supported node (900+ queries) plus directed phrase/near/append/spy/delete/replace/refusal/
  deep-query/20,000-append/concurrency cases. Documents, estimate, rank and weights of the strata engine are
  checked against an independent model of Xapian's BM25 (worst relative weight difference 4.2e-16, tolerance 1e-9).
- **Revert checks / mutants** (`.scratch/mutants.py`, each caught): interleave (2,490 checks fail), FILTER weighs
  (308), descending ignored (200), deleted documents visible (5), pruning drops ties (141), no refresh before search
  (6), AND_NOT as AND (2,879), estimate skipped (9,805), local statistics (crash: a term outside the snapshot),
  OR needs both (812), PHRASE as NEAR (2,857), append joins nothing = the fix reverted (93), spy sees nothing (2),
  rank offset (101,061), first ignored (1,632). Rerun on the final code: all caught.
- **Sanitizers.** ASan+UBSan Debug build, `test_engine_search_strata` with `NO_XAPIAN_COMPARE=1`: 150,016 checks, no
  report (chains, concurrency, spies, deletes included); `engine_strata` 8,081 clean. With the old nested append
  scorers the same run dies with `stack-overflow` (S3's finding, regression test `test_long_chains`). Xapian's own
  matcher asserts (Debug) and use-after-frees (ASan) on some of the random shapes, hence `NO_XAPIAN_COMPARE`; the
  Xapian comparison runs in the Release build. **Not run:** the server under the ASan build (rapidjson's
  pointer-overflow UBSan report aborts it on the first response, both engines; the repository's ignore list
  needs a recompile) and ThreadSanitizer (T10); round 2 ran TSan on the search tests (above).
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

- **Round 2** (Xapiand:94eb1c9ab, and the refusal commit after it).
  1. **P4 in place of the local heap.** Prospect repinned to main (prospect:d8274b7: P3 operators, P4 `shards.hh`). The
     local `Candidate`/`Ranks` heap, the docid interleave and the counting pass are gone: `multi_snapshot` +
     `prospect::search` (pruned for ascending ids, exhaustive for a spy or descending ids, the spy as its visitor).
     `term_table` still serves per-segment term infos, statistics and df over `snapshot.segments()`. Unit test: 230,444
     checks pass, worst relative weight difference 3.9e-16 (tolerance 1e-9), the same 14/7/1 Xapian disagreements.
  2. **Concurrency.** `test_snapshot_consistency`: a writer adds a document to shard 0, 1, 2 in turn and commits every 7th
     generation while four searchers (in order, reversed with descending ids, with a spy) search; each result must show
     per-shard counts that existed at one moment (c0 >= c1 >= c2 >= c0 - 1, documents 1..count per shard) and the weight of
     exactly that many documents (BM25 model). Lock order: opposite-order searchers plus the writer. TSan (Homebrew LLVM
     23.1.2, own build dir and dependency tree, `NO_XAPIAN_COMPARE`): 152,733 checks, 0 reports. ASan+UBSan (same
     compiler, another tree): 152,733 checks, 0 reports. Both run the whole file.
  3. **Mutants** (each run on the committed tree, `.scratch/mutants.py`; all caught but the two below):
     - refused shapes returning a result: sorter, collapse, similar, each cutoff alone, PHRASE window, PHRASE of
       non-terms, operators (combine and append), wildcard, edit distance, posting source, query text, depth. The depth
       mutant survived until a refusal test on an index with no segment was added (1 check fails now). Added to
       `test_refusals`: posting source, edit distance, shapes under scale/append/AND_NOT.
     - flat scorer weights (a run scaled by 0.99, a run's first child weighing 0: 655 and 597 checks fail), run operator
       confusion (2); `test_long_chains` compares documents, weights and order with the model.
     - spy values: weight 0 (205), interleaved id (526), wrong ordinal (195), empty data (204), seen twice (3), seen
       nothing (3); `test_spy` pairs id, data and weight per match for three windows/orders and after a delete.
     - snapshot pin: lock released at once, no lock, pins in request order: the snapshot test alone aborts (an engine
       error from a search racing the writer) or deadlocks (timeout) under each. Also: shard slot (1,679), no refresh (449).
     - Not caught by design: `strata::search`'s "shard of another engine" check, which `engine::search` makes unreachable
       by refusing a mixed set first (neither layer alone is testable; removing both would be).
  4. `ONLY_TEST=<name>` runs one test function of the file.

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

- **Key prefix (lead's note, 2026-10-27).** No query names a key: terms are looked up in the dictionary, and
  `term_key_prefix()` returns `keys::key_prefix()` (S1's, the prefix the index opens with; `src/engine/strata/keys.h`);
  `term_key_prefix()` in `src/engine/strata/search.cc` is the one function to change when
  strata's fields-and-keys redesign (strata `design/fields-and-keys`) lands. `Shard::identity()` and `Shard::pin()`
  are the other seams; `pin()` is where the snapshot is taken.
- **Prospect pin** is prospect:0296b8b (`fix/collector-reserve`, CR1; branch pushed, not yet merged into prospect
  main, which is at eee749f). Move the pin to main once it merges. The multi-shard collection is P4's.
- `rethrow_translated` is one function now (`shard.cc`, declared in `shard.h`); `strata::search` calls it with no Core.
- The refusal kind is `errc::syntax` (a client error, not retried); `not_available` would be retried five times.
- The lock: a search holds every shard's lock for its duration (pinned in address order); a search is therefore one
  moment of the whole index (the last pin). T10 (concurrency) replaces it with a snapshot that outlives the lock.
- Sanitizer builds: Homebrew LLVM 23 rejects rapidjson v1.1.0's `GenericStringRef::operator=` (const member); the
  scratch build trees were patched to `= delete`, the source and pin are unchanged.
- No fuzz target: this change adds no decoder or parser; it reads the in-memory `Query`. The depth guard and
  a 20,000-append chain are tested.

## From S3's note

1. The append bug: already fixed here (Xapiand:e4e29fb16, same rule as Xapiand:7b5a62fdf; not cherry-picked, the
   two will conflict textually, resolve by keeping this branch's version). 2. Iterative destruction: the query
   tree already freed iteratively; the scorer tree is now flat per run (above). 3. `SpyView` is minimal (id,
   data, version; values empty): S3's `SegmentDocView` replaces it.

## Code review round (`docs/reviews/strata-engine-search-code.md`)

- CR1 closed: `options.k` is clamped to the pinned snapshot's document count (`search.cc`), and prospect's collector
  reserves for the documents searched, not for k (prospect branch `fix/collector-reserve`, pushed, not merged; the
  pin in `CMakeLists.txt` moves when it lands). Test `test_huge_window`.
- CR3 closed: `test_pin_holds` (the reviewer's deterministic probe) is in the file.
- CR4 closed: `percent_of` defines non-finite weights (100 for an infinite hit, 0 for a finite one under an infinite
  top); `test_percent_infinite`.
- CR6 comment: `engine/search.h` now says `errc::syntax`.
- CR2 closed (merge Xapiand:5f0d5e598 and the test commit after it): `Shard::pin()` calls `core.check(*handle_)` under
  the lock and translates with `rethrow_translated(handle_->core)`; `term_key_prefix()` is `keys::key_prefix()`.
  Test `test_pin_after_failed_reopen` (a cancel whose reopen fails, then a search over both shards and over the
  broken one alone raise `errc::database`, the other shard's lock is free, a reopen searches again). With the
  `check()` in `pin()` removed the test fails (2 checks). `test_engine_strata` expected `not_available` from a
  strata search (S1's stub); it now checks match-all finds every document.
- Rebase proof (ASan+UBSan and TSan, Homebrew LLVM 23.1.2, separate build dirs and dependency trees, rapidjson
  patched in `_deps` only, `NO_XAPIAN_COMPARE=1`): `test_engine_search_strata` 153,580 checks, `test_engine_strata`
  9,558, the reviewer's `probe_pin_holds` (6) and `probe_lock_order` (1): all pass, no sanitizer report. Build dirs deleted.
  Notes, not fixed: CR5 (a spy re-entering a pinned shard deadlocks: say
  so in the `MatchSpy`/`KeyMaker` contract, and `pin_of` is a linear scan, O(n^2) over thousands of shards); CR6 (the
  lock is held for the whole search; the architect says whether T10 precedes the aggregation spy).

## Next

Lead's re-check of CR2; repin prospect to main when `fix/collector-reserve` merges; S3 (key makers, spies, expansions,
operators) and S4 (text).

## Blocked on

Nothing.
