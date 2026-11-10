# x7-cancel-rows: journal

State: waiting on jobs 261110-143945-c98b2e 261110-143947-503447
Quest: .campaign/xapiand/quests/x7-cancel-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Series defined in src/metrics.{h,cc} (Xapiand:cfd72a900, per the lead's message). Job
261110-143945-c98b2e builds test_node at head and runs half_closes, client_leaves,
admission_peaks. Mutant 261110-143947-503447 (after it) runs at refs/mutants/x7-cancel-rows
(c604c7077f9a: the increment and both peak raises removed); it succeeds only when both client_leaves
and admission_peaks fail. Cancelled: 261110-142943-7ee855 (wrong target name),
261110-143158-21ff95 and 261110-143812-1994f1 (superseded by the move to Metrics).

## Next

1. Read both jobs; fix what they show (a test change means a new mutant commit), then
   `campaign verify x7-cancel-rows` and fill the Evidence table.

## Blocked on

Nothing.

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
