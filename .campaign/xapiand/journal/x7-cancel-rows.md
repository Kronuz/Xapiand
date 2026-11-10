# x7-cancel-rows: journal

State: waiting on jobs 261110-151113-1d6719 261110-151116-28e4a2
Quest: .campaign/xapiand/quests/x7-cancel-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Job 261110-151113-1d6719 builds test_node at Xapiand:dd181754c and runs half_closes,
client_leaves, admission_peaks. Mutant 261110-151116-28e4a2 (after it) runs at
refs/mutants/x7-cancel-rows (d6856a04756e: the increment and both peak raises removed); it
succeeds only when both client_leaves and admission_peaks fail.

Failed 261110-143945-c98b2e (at cfd72a900): client_leaves, the search was not let go 2.0 s after
its client left and the counter did not move; the other 6 checks passed; its mutant was skipped.
Cause: the test's search was the index's first, so it waited in the schema load, which
reads the metadata through `SchemasLRU::read_metadata` with a `DatabaseHandler` of its own and no
budget: that turn wait has no token. Probe (local, quiet light, slot build with the test caching
the schema first by one SEARCH): passes, the reactor let the search go 2842 us after the close,
counter +1. So the search's own turn wait and the counter work; the test now caches the schema
first (dd181754c), and the cold-schema gap is under Blocked on.
Earlier cancelled: 261110-142943-7ee855, 261110-143158-21ff95, 261110-143812-1994f1,
261110-143947-503447 (skipped).

## Next

1. Read 261110-151113-1d6719 and 261110-151116-28e4a2; then `campaign verify x7-cancel-rows` and
   fill the Evidence table.

## Blocked on

Lead: a search whose schema is not cached, held in the schema load's turn wait, is not cancelled
by its client leaving (src/database/schemas_lru.cc `read_metadata`, outside the file set). Options:
(a) widen the file set to src/database/schemas_lru.cc and pass the caller's token (the
`DatabaseHandler`'s budget) into `read_metadata`'s handler, with a cold-schema variant of the A1
test; (b) a follow-up quest. Recommendation: (a), small and the same mechanism the search's own
turn wait uses. Named in docs/mechanisms/cancellation.md Limits. The rest of the quest does not
wait on it.

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
