> Status: CURRENT (2026-11-10) request cancellation and the request path's series in `/:metrics`, as built on `quest/xapiand/x7-cancel-rows` (T9, D56). Not reviewed.

# Cancellation and its metrics

## What

A request's cancel is http's token (`http::Request::cancel_token()`): a client that leaves (its EOF) or a node that stops
requests it. Reads stop at their next checkpoint; a write that was admitted completes. Three series of `Metrics`
(`src/metrics.{h,cc}`, with the other `xapiand_*` series) show it in `GET /:metrics`:

| Series | Type | What |
|---|---|---|
| `xapiand_http_requests_cancelled` | counter | Requests a cancel stopped at a checkpoint (`task::cancelled` reached `SearchService::handle`) |
| `xapiand_admission_inflight_peak` | gauge | Most requests one reactor's admission gate held at once, since the node started |
| `xapiand_scheduler_demoted_peak` | gauge | Most demoted handles a request found on its reactor when it was admitted, since the node started |

## How

1. **The cancel.** `handle` gives the token to the request's budget (`Request::budget`, a 200 us slice). A search's turn
   wait (`take_turn`, `lock_shard`) and its slices check it, so a search waiting for its index's turn ends at once and a
   running one at its next slice. A schema's load reads the metadata (and a foreign schema) with the caller's budget
   (`SchemasLRU::get` passes `DatabaseHandler::budget` to `read_metadata` and `load_shared`), so a search still in its
   index's first schema load ends the same way. `handle` catches `task::cancelled`, counts it, answers 503 to a client still there, and
   closes the connection; the request's admission lease ends with its response.
2. **Writes complete.** A write's budget has no token: a PUT whose client half-closes after its body (the server reads
   the EOF while the write waits for its turn) is applied, committed and answered (204), and survives a restart.
3. **The peaks.** `SearchService::create_extension` runs right after the reactor's `AdmissionGate` admits a request, so
   the gate's count then includes it: the most of those counts is the in-flight peak, which the gate's cap (1,000 per
   reactor) bounds. The same call reads the loop's demoted count, which the demoted cap (256) compares, and keeps the most
   of it. `Metrics::raise_peak` sets a gauge only above what it holds: a lower value costs one load, a new peak takes a
   lock, so concurrent admissions never lower it. The gauge is the largest over all reactors.

## Limits

- The demoted peak is sampled at admissions only: a loop whose demoted queue grows while no request arrives shows it at
  the next admission, or not at all. It is what the demoted cap saw, not the queue's own maximum.
- The peaks never reset; a scrape reads the most since the node started.
- A request that joins a schema load another request leads (`SingleFlight`) waits for that load without its own token;
  a leader that is cancelled ends the joiners' wait and each loads again.
- A cancel that ends a request before `handle` (a 503 at admission, a client that leaves mid-headers) is not counted.

## Code

`src/metrics.{h,cc}` (the three series, `Metrics::raise_peak`), `src/database/schemas_lru.cc` (`read_metadata`,
`load_shared`, `SchemasLRU::_update`), `src/server/search_views.cc`
(`SearchService::create_extension`, `SearchService::handle`).
Tests: `tests/test_node.cc` (`test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted`,
`test_a_search_whose_client_leaves_in_its_first_schema_load_is_cancelled`,
`test_a_put_whose_client_half_closes_completes_durably`, `test_the_admission_peaks_rise_under_load`).
