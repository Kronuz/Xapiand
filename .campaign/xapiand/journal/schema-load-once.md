# schema-load-once: journal

State: waiting on jobs 261112-224936-c667cc, 261112-224941-6dba3b, 261112-224945-2e1a69
Quest: .campaign/xapiand/quests/schema-load-once.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

State: waiting on jobs 261112-224936-c667cc (A1 mutant), 261112-224941-6dba3b (A2 ASan x20), 261112-224945-2e1a69 (A3 build-test). Lead's change: settings flight in scope; fixed with the same mechanism, jobs rerun at the new head.

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
- msg: build-test 261112-223937-d8cb1b failed only on test_loop_yield.cc:393 (master flake, ratio 0.706 vs 0.7 at load 11); the
  old three jobs are superseded. If it flakes at the new head, the lead reruns the check.
- Seam: SchemasLRU::before_metadata_claim / before_foreign_claim, awaited between a first use's cache miss and its claim;
  empty in production (one std::function test).
- Tests: test_a_call_paused_before_claiming_{the_foreign,the_metadata}_load_loads_nothing hold the first call at the seam
  on a task::event, let the other 5 finish (2 loads), release it, and count 2 loads still.
