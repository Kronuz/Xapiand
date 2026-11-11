# x7-cancel-rows: journal

State: waiting on jobs 261110-164633-1246a9 261110-164634-b7eb0b 261110-164637-c92fef
Quest: .campaign/xapiand/quests/x7-cancel-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead chose option 1 (A5 added, Xapiand:7a43a1a13, merged at 445c7bd45; file set now has
src/database/schemas_lru.{h,cc}). Xapiand:ac7e91d22: `SchemasLRU::get`/`set` pass the
`DatabaseHandler`'s budget through `_update` to `read_metadata` and `load_shared`; the A1 test is
a helper run twice: cached schema (A1) and cold, first schema load (A5,
`test_a_search_whose_client_leaves_in_its_first_schema_load_is_cancelled`).

Suite 261110-151402-093b4e failed on the async check (`src/metrics.cc:358 [mutex] unclassified`, the
lock in `Metrics::raise_peak`); its mutants 261110-151422-751ade and 261110-151425-061294 were
skipped. Xapiand:f0c9c32f7 makes the peaks lock-free: `std::atomic<std::size_t>` members of
`Metrics` raised by a compare-exchange loop, published to their gauges by `Metrics::serialise` at
each scrape. Local `harness/async_check.sh --strict --owed` (quiet light, strata from the slot's
.deps): 250 hits, unclassified 0, owed 0.

Suite 261110-153616-0b6979 (at f0c9c32f7) failed only test_the_admission_peaks_rise_under_load: the
20,000-line RESTORE was not answered within the test client's 10 s receive timeout, and the demoted
peak stayed 0; A1 (2078 us) and A5 (3082 us) passed; async check clean; mutants skipped.
Probes (quiet light, slot build, load 15-21):
- test binary, NODE_LOG: the RESTORE took past 10 s ("Request took too long ~10075 ms"): the
  client's timeout, not an error answer. Its loop scraped /:metrics with no pause, competing with
  the restore on the one reactor.
- standalone node, same 20,000 lines, scrapes every 50 ms: 200 in 2.88 s (about 6,900 docs/s), and
  the demoted peak still 0. A restore takes a turn per object, and each resume starts a fresh
  slice, so its objects never spend one: a restore does not demote. The test's premise was wrong.
  Not the hunt.md row (1,000 docs at 26 docs/s): not reproduced here.
- standalone node, a PUT whose Lua script loops 20M times, scrapes every 5 ms: 204 in 0.76 s,
  demoted peak 1 (a script yields at its hook and its checkpoint demotes it).
Fix (test only): the demoted half runs a script PUT (10M iterations; up to 5 PUTs until the peak
is 1) while a scraper thread polls every 2 ms; no RESTORE, no 20,000-line body. Its id is 100 (the
index's ids are integers; a first probe with id "script" got a 400). Local probe, 3 runs at load
about 10: ok, 2.1 s, 1.5 s, 1.5 s.

Jobs (at Xapiand:a8a441930; 0b6979's mutants fa27e0 and aee819 were skipped):
- 261110-164633-1246a9: `campaign verify` build-test, the suite for A1-A5.
- 261110-164634-b7eb0b: mutant A1/A4 at 2ce02f1f6151 (refs/mutants/x7-cancel-rows: increment and
  both peak raises removed); succeeds only when frees_its_reactor and admission_peaks both fail.
- 261110-164637-c92fef: mutant A5 at f4ad9bd6da40 (refs/mutants/x7-cancel-rows-a5: `get` passes
  nullptr for the budget); succeeds only when the cached variant passes and the cold one fails.

Earlier: 261110-143945-c98b2e failed (cold schema, the cause of A5; probe: the cached schema
variant freed the reactor 2842 us after the close). Cancelled or superseded: 261110-142943-7ee855,
261110-143158-21ff95, 261110-143812-1994f1, 261110-143947-503447, 261110-151113-1d6719,
261110-151116-28e4a2.

## Next

1. Read the three jobs; fill the Evidence table (A3: quote docs/mechanisms/cancellation.md); State
   "ready for review".

## Blocked on

Nothing. (The cold-schema gap is closed by A5; a request that joins another's schema load waits
for that load without its own token, named in cancellation.md Limits: SingleFlight is in
src/single_flight.h, outside the file set.)

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending |
| A5 | pending |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- msg (lead, after ruling 1): the three series are defined in src/metrics.{h,cc} with the other
  `xapiand_*` series (file set widened); src/server keeps only the increment and the peak updates,
  and metrics_view has no extra output path. Peaks go through `Metrics::raise_peak` (load fast
  path, lock only on a new peak, so racing admissions never lower a gauge). Names:
  `xapiand_http_requests_cancelled` (counter), `xapiand_admission_inflight_peak`,
  `xapiand_scheduler_demoted_peak` (gauges, the names x7-gate's stall_load.py reads).
- Both peaks are sampled in `SearchService::create_extension`, which http calls right after the
  reactor's gate admits the request: the gate's count there includes it, so the in-flight peak is
  exact; the demoted peak is what the demoted cap saw (sampled at admissions only). The gauge is
  the max over reactors.
- The counter counts every `task::cancelled` that reaches `handle` (search, restore, flush), not
  only searches.
- A1's "within one slice": the test holds the index's turn, so the search is suspended in its
  turn wait; the check is that the client's close lets the request's admission go while the turn
  is still held (the cancel took effect at its checkpoint, not at the release), with the measured
  latency printed. A wall-clock 200 us bound across a socket close is not deterministic.
