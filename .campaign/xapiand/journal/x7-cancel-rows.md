# x7-cancel-rows: journal

State: waiting on jobs 261110-170115-ba1167
Quest: .campaign/xapiand/quests/x7-cancel-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Suite 261110-170115-ba1167 (`campaign verify` build-test) runs at Xapiand:febdcf478, which only drops a blank
line in src/metrics.cc (the lead's nit) after a8a441930 was green: suite 261110-164633-1246a9 and
both mutants killed (261110-164634-b7eb0b, 261110-164637-c92fef). The lead read the diff; review is
waived under D57 (normal risk).

Failed and examined: 261110-143945-c98b2e (cold schema: led to A5), 261110-151402-093b4e (async
check: the lock in `raise_peak`, made lock-free in f0c9c32f7), 261110-153616-0b6979 (peaks test:
the RESTORE outran the client's 10 s timeout and never demotes; replaced by a script PUT in
a8a441930). Cancelled or skipped: 261110-142943-7ee855, 261110-143158-21ff95,
261110-143812-1994f1, 261110-143947-503447, 261110-151113-1d6719, 261110-151116-28e4a2,
261110-151422-751ade, 261110-151425-061294, 261110-153617-fa27e0, 261110-153619-aee819.
Probes (local, quiet light; not evidence): the cached-schema search freed the reactor 2842 us after
its client left; a standalone 20,000-line RESTORE took 2.88 s and never raised the demoted peak; a
Lua-script PUT raised it in 0.76 s; the fixed peaks test passed three times (2.1, 1.5, 1.5 s).

## Next

1. When 261110-170115-ba1167 passes, State "ready for review" (the lead lands; no review under D57).

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
| A1 | 261110-170115-ba1167 (build-test; `test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted`; at a8a441930 the reactor let the search go 2078 us after the close) |
| A2 | 261110-170115-ba1167 (build-test; `test_a_put_whose_client_half_closes_completes_durably`) |
| A3 | 261110-170115-ba1167 (build-test; the tests read `xapiand_http_requests_cancelled` from `GET /:metrics`) and `docs/mechanisms/cancellation.md`, quoted below |
| A4 | 261110-170115-ba1167 (build-test; `test_the_admission_peaks_rise_under_load` reads both peaks from `GET /:metrics`) |
| A5 | 261110-170115-ba1167 (build-test; `test_a_search_whose_client_leaves_in_its_first_schema_load_is_cancelled`; at a8a441930, 3082 us) |

### Fails without the fix

- A1, A4: 261110-164634-b7eb0b at Xapiand:2ce02f1f6151 (refs/mutants/x7-cancel-rows: the counter
  increment and both peak raises removed): frees_its_reactor and admission_peaks both fail; the
  job succeeds only then.
- A5: 261110-164637-c92fef at Xapiand:f4ad9bd6da40 (refs/mutants/x7-cancel-rows-a5: `SchemasLRU::get`
  passes nullptr for the budget): the cached variant passes and the cold one fails.

The mutants ran at a8a441930's children; febdcf478 changes only a blank line in src/metrics.cc.

### A3 quote (docs/mechanisms/cancellation.md)

> | `xapiand_http_requests_cancelled` | counter | Requests a cancel stopped at a checkpoint (`task::cancelled` reached `SearchService::handle`) |

## Result

A search whose client leaves ends at its turn wait, its own or its index's first schema load's
(the schema reads now take the caller's budget), and frees its reactor's admission in about 2-3 ms
(2078 us cached, 3082 us cold, at a8a441930), with `xapiand_http_requests_cancelled` counting it.
A PUT whose client half-closes after its body is applied, answered 204, and read back after a
restart. `/:metrics` exports `xapiand_admission_inflight_peak` and `xapiand_scheduler_demoted_peak`
(lock-free atomics in `Metrics`, raised at each admission, published at scrape) for x7-gate's T8.
How it works: docs/mechanisms/cancellation.md; requests-on-the-reactor.md links it.
Open: a request that joins another's schema load (`SingleFlight`) waits for it without its own
token (src/single_flight.h, outside the file set); the demoted peak is sampled at admissions only.

## Rulings I made

- msg (lead, after ruling 1): the three series are defined in src/metrics.{h,cc} with the other
  `xapiand_*` series (file set widened); src/server keeps only the increment and the peak updates,
  and metrics_view has no extra output path. Each peak is a `std::atomic` in `Metrics` raised by a
  compare-exchange loop and published to its gauge in `Metrics::serialise` (the lead's message
  after the async check; no lock on the admission path). Names:
  `xapiand_http_requests_cancelled` (counter), `xapiand_admission_inflight_peak`,
  `xapiand_scheduler_demoted_peak` (gauges, the names x7-gate's stall_load.py reads).
- Both peaks are sampled in `SearchService::create_extension`, which http calls right after the
  reactor's gate admits the request: the gate's count there includes it, so the in-flight peak is
  exact; the demoted peak is what the demoted cap saw (sampled at admissions only). The gauge is
  the max over reactors.
- The counter counts every `task::cancelled` that reaches `handle` (search, restore, flush), not
  only searches.
- The demoted half of the A4 test runs a scripted PUT, not a RESTORE: a restore never spends a
  slice (a turn per object) and its large body outran the client's timeout under load.
- A1's "within one slice": the test holds the index's turn, so the search is suspended in its
  turn wait; the check is that the client's close lets the request's admission go while the turn
  is still held (the cancel took effect at its checkpoint, not at the release), with the measured
  latency printed. A wall-clock 200 us bound across a socket close is not deterministic.
