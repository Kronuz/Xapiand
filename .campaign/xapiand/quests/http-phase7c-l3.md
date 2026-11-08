# http-phase7c-l3: http, reactor, http-log and prism without the offload path (L3)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at http:4d56ab1 (2026-10-30); its ledger `docs/status/phase7c-l3.md` in Kronuz/http is the record. The quest's repository is `../http`. Its machine state is in `state.json`: `campaign status http-phase7c-l3`.

## Outcome

http drops its offload path (owner ruling Q3): handlers and routes are coroutines, `BodyStream` reads on the loop, a request carries a disconnect callback, reactor gains `AdmissionGate`, the buffered body has a 413 cap, shutdown follows D16 with a bounded grace, and http-log and prism migrate with it (reactor:76c32b4, http-log:d2315b8, prism:bdaf3e6).

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Offload removed: worker pool, `should_offload`, body hand-off, `ChannelBodyReader`, `Reactor::pool`: `docs/status/phase7c-l3.md`
- A2 [artifact] `HttpHandler::handle` and `Router` routes are coroutines: `docs/status/phase7c-l3.md`
- A3 [artifact] `BodyStream` read with `co_await` on the loop (asio_test [K], coroutine_test [B]): `docs/status/phase7c-l3.md`
- A4 [artifact] Per-request disconnect callback `Request::on_cancel`; both C17 rules stated and tested (coroutine_test [C]): `docs/status/phase7c-l3.md`
- A5 [artifact] `reactor::AdmissionGate` beside the application cap, each with its own 503 (coroutine_test [D]): `docs/status/phase7c-l3.md`
- A6 [artifact] Buffered-body cap with 413 (coroutine_test [E]); parser fuzz target: `docs/status/phase7c-l3.md`
- A7 [artifact] Shutdown order (D16) and its bound (C14); `Reactor::track` O(1) (reactor tests [F], [L]): `docs/status/phase7c-l3.md`
- A8 [artifact] prism and http-log migrated; prism builds and its demo serves 8 routes: `docs/status/phase7c-l3.md`
- A9 [artifact] ASan+UBSan and TSan clean at the final heads (reactor 9/9, http 10/10, 0 reports): `docs/status/phase7c-l3.md`
- A10 [artifact] preflight: reactor mac-hardened, mac-asan-ubsan, linux, freebsd ok; http mac-hardened, linux, freebsd ok (http mac-asan-ubsan run by hand: compressors header search path): `docs/status/phase7c-l3.md`
- A11 [artifact] Xapiand HTTP-layer check against these pins: configures; compile errors listed for X3: `docs/status/phase7c-l3.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Design: Xapiand `docs/design/phase7c.md` D2, D3, D12, D16 and section 5 L3; owner ruling Q3 (strata `docs/status/xapiand-on-strata.md`): the offload path leaves http entirely.
- Follow-ups left open in the ledger: L3-N2 (concurrent `abort_all`), N3 (D16 wording), N4 (release-mode `track` check), N5 rest (`docs/http.d2` still draws the pool), N6, N7/N10, N8/N9 and the keep-alive extra read (the lead measures; `perf-watch-peek`).
- Risk high: request lifetime, cancellation and shutdown on every HTTP path (the auditor's mapping).

## May land early

None.
