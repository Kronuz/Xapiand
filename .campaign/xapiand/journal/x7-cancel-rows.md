# x7-cancel-rows: journal

State: waiting on jobs 261110-151402-093b4e 261110-151422-751ade 261110-151425-061294
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

Jobs:
- 261110-151402-093b4e: `campaign verify` build-test at ac7e91d22, the suite for A1-A5.
- 261110-151422-751ade: mutant A1/A4 at 719349afbd65 (refs/mutants/x7-cancel-rows: increment and
  both peak raises removed); succeeds only when frees_its_reactor and admission_peaks both fail.
- 261110-151425-061294: mutant A5 at cb43adbb9985 (refs/mutants/x7-cancel-rows-a5: `get` passes
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
