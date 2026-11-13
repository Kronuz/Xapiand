# schema-load-once: journal

State: waiting on jobs 261112-223932-8ff4dc 261112-223935-5dbeff 261112-223937-d8cb1b
Quest: .campaign/xapiand/quests/schema-load-once.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

State: waiting on jobs 261112-223932-8ff4dc (mutant, A1), 261112-223935-5dbeff (ASan x20, A2), 261112-223937-d8cb1b
(build-test, A3). The paused-claim tests passed at the fix (job 261112-223540-75e701, before the squash: 20 checks).

## Next

1. When the jobs finish: paste `campaign brief`'s Evidence table, write Result, State "ready for review".
2. Push is done at the squashed head; CI is CI's.

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
- The index settings flight (index_resolver_lru.cc, outside this quest's file set) has the same gap; the 2-argument run is
  unchanged for it. Reported, not fixed.
- Seam: SchemasLRU::before_metadata_claim / before_foreign_claim, awaited between a first use's cache miss and its claim;
  empty in production (one std::function test).
- Tests: test_a_call_paused_before_claiming_{the_foreign,the_metadata}_load_loads_nothing hold the first call at the seam
  on a task::event, let the other 5 finish (2 loads), release it, and count 2 loads still.
