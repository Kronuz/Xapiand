# schema-load-once: journal

State: waiting on jobs 261112-232215-0291bf (A1 mutant, replicas), 261112-232218-983be8 (A1 mutant, probes), 261112-232220-71809c (A2 ASan x20), 261112-232222-f3a86e (A3 build-test)
Quest: .campaign/xapiand/quests/schema-load-once.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on the jobs in the State line; when they finish, paste the Evidence table, write the Result, set State to ready for review.

## Next

1. When the jobs finish: paste `campaign brief`'s Evidence table, write Result, State "ready for review".

## Blocked on

Nothing

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |

## Result

pending

## Rulings I made

- The interleaving (A1): SingleFlight.run checks nothing between a caller's cache miss and its claim of the flight, and a
  flight's value reaches the cache only in each caller's continuation, after the flight is erased. A waiter B that missed
  the cache (of the foreign schema `.xapiand/indexes/<index>`, the default link of an index without metadata) but is
  descheduled before `foreign_loads.run` finds no flight once leader A's load ended: B becomes a second leader and reads the
  foreign schema again (3 loads, not 2). The same gap exists for the metadata flight (cache miss, flight ends, new flight).
  In test_node:535 the held turn makes all 6 calls join the metadata flight, so only the foreign flight showed it, under
  ASan's slower scheduling.
- Fix: a load puts its schema into the cache before its flight ends (load_metadata, load_foreign), and a leader asks a probe
  (cached_metadata, cached_foreign) whether the cache holds the schema now before it loads (SingleFlight::run's third
  argument; not counted as a load). A hit on the metadata flight re-enters the existing "found in cache" path.
- Settings flight (lead's msg, file set widened to src/index_resolver_lru.*): same mechanism. load_and_publish reads, runs the
  resolver's own initialize_loaded/place_shards (now shared functions), and store_settings puts the entry and its shards in the
  cache before the flight ends; cached_settings is the probe. Because a new index's shards are now placed (with default
  replicas) before a caller's settings are applied, a replica-count change on a not-loaded index clears the shards so they are
  placed again (settling only adds replicas). Seam: IndexResolverLRU::before_settings_claim; test
  test_a_call_paused_before_claiming_the_settings_load_loads_nothing (1 read). Passed at the new head (job 261112-224510-9fae0e).
- Replicas test (lead's msg): test_a_new_index_gets_the_replicas_asked_below_and_above_the_default adds two nodes (so it runs
  last), sets the default to master+1 and creates new indexes asking 0, 1, 2 replicas and the default; each shard must have
  1, 2, 3, 2 nodes. Passed (job 261112-225104-1e0203). A second mutant removes the shards.clear() on a replica change.
- Mutant 230457 survived because the test asked number_of_shards 2, not the default: the other clear (shards count differs)
  re-placed the shards anyway. The test now asks the default shard count, so only the replica count asks for other placement;
  the clear is needed (the flight places with the default replicas, and settling only adds).
- harness/async_allowlist.tsv (lead's msg): the 6 new schemas_lru.cc and 1 new index_resolver_lru.cc mutex hits are O(1) cache
  sections with no I/O, none across an await; counts 19->25 and 3->4, notes extended; async_check.py --strict passes.
- msg: build-test 261112-223937-d8cb1b failed only on test_loop_yield.cc:393 (master flake, ratio 0.706 vs 0.7 at load 11); the
  old three jobs are superseded. If it flakes at the new head, the lead reruns the check.
- Seam: SchemasLRU::before_metadata_claim / before_foreign_claim, awaited between a first use's cache miss and its claim;
  empty in production (one std::function test).
- Tests: test_a_call_paused_before_claiming_{the_foreign,the_metadata}_load_loads_nothing hold the first call at the seam
  on a task::event, let the other 5 finish (2 loads), release it, and count 2 loads still.
