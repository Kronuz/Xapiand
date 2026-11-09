# http-phase7c-l3: journal

State: landed
Quest: .campaign/xapiand/quests/http-phase7c-l3.md (the lead's; never edit it)

## Doing

Nothing: landed at http:4d56ab1 on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-l3.md`: "[x] Offload removed: worker pool, `should_offload`, body hand-off" |
| A2 | `docs/status/phase7c-l3.md`: "[x] `HttpHandler::handle` is a coroutine; `Router` routes are coroutines" |
| A3 | `docs/status/phase7c-l3.md`: "[x] `BodyStream` read with `co_await` on the loop (asio_test [K] one thread; coroutine_test [B])" |
| A4 | `docs/status/phase7c-l3.md`: "[x] Per-request disconnect callback (`Request::on_cancel`); both C17 rules stated" |
| A5 | `docs/status/phase7c-l3.md`: "[x] `reactor::AdmissionGate`, beside the application's own cap" |
| A6 | `docs/status/phase7c-l3.md`: "[x] Buffered-body cap with 413 (coroutine_test [E]); parser fuzz target" |
| A7 | `docs/status/phase7c-l3.md`: "[x] Shutdown order (D16) and its bound (C14: the grace, reactor test [F], coroutine_test [F])" |
| A8 | `docs/status/phase7c-l3.md`: "[x] prism (and http-log, which it wraps) migrated; prism builds and its demo serves" |
| A9 | `docs/status/phase7c-l3.md`: "[x] ASan+UBSan and TSan clean at the final heads (reactor 9/9, http 10/10 each, 0 reports)" |
| A10 | `docs/status/phase7c-l3.md`: "[x] preflight (`cpp-libraries/tools/preflight.sh`): reactor mac-hardened, mac-asan-ubsan, linux, freebsd ok" |
| A11 | `docs/status/phase7c-l3.md`: "[x] Xapiand HTTP-layer check against these pins: configures; the compile errors are listed below for X3" |

## Result

Landed at http:4d56ab1 on 2026-10-30. http:bda9b5a coroutine handlers, then the review fixes http:fce24c6 (L3-B1, L3-B2) and reactor:76c32b4 (L3-N1); the ledger's last commit http:4d56ab1 is on main.

- Mutants: 16 http and 7 reactor mutants die; the drain-time cancel mutant survived and its code was removed as redundant.
- libFuzzer 181 s, 1.9M runs, no crash; TSan reactor_test 518 runs, no hang.
- Review: `docs/reviews/phase7c-l3-code.md` -- APPROVE WITH CONDITIONS (L3-B1, L3-B2, L3-N1 fixed; probes rerun clean).
- CI green at the review heads (reactor:7dcf73c, http:0553e53, http-log:86661b3, prism:51e21bf); at the fix heads pushed, not watched.
- Docs: `docs/mechanisms/coroutine-handlers.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
