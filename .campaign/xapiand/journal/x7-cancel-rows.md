# x7-cancel-rows: journal

State: waiting on jobs 261110-143158-21ff95 261110-143812-1994f1
Quest: .campaign/xapiand/quests/x7-cancel-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code and tests committed (src/server/search_views.cc, tests/test_node.cc); docs in
docs/mechanisms/cancellation.md. Job 261110-143158-21ff95 builds test_node and runs the three new
cases (filters half_closes, client_leaves, admission_peaks).

Mutant 261110-143812-1994f1 (after it) runs at refs/mutants/x7-cancel-rows (30755328952c, the
counter increment and both peak samples removed); it succeeds only when both
client_leaves and admission_peaks fail.

## Next

1. Read 261110-143158-21ff95 and 261110-143812-1994f1; fix what they show (a test change means a
   new mutant commit), then `campaign verify x7-cancel-rows` and fill the Evidence table.

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

- The three series live in src/server (the file set) and `metrics_view` appends their text after
  `server_metrics()`, labelled cluster/node like the reactor lag block, not in src/metrics.h
  (outside the file set). Names: `xapiand_http_requests_cancelled` (counter),
  `xapiand_admission_inflight_peak`, `xapiand_scheduler_demoted_peak` (gauges, the exact names
  x7-gate's stall_load.py reads).
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
