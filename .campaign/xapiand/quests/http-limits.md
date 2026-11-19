# http-limits: http: Limits, timeouts and stop_accepting

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status http-limits`.

## Outcome

The note's (cpp-libraries:04a75c0 docs/server-composition.md) C1, Settled 1 and 6. Plan: docs/design/lifecycle-adoption.md quest 3 (about 1,200 lines). Needs reactor-server-composition.

## Acceptance criteria

- A1 `HttpAsioService(handler, reactors, const Limits&, Tls)` replaces the setters, `tls_connection_factory` and `AsioResponseOptions`' limits: a test per field at its bound, plain and TLS
- A2 Idle, header and inactivity timeouts, plain and TLS; `close_idle()` closes idle connections at once and busy ones after their response: tests
- A3 After `stop_accepting()`, a request on an open keep-alive connection is refused and an in-flight response carries `Connection: close`: test
- A4 Settled 1: a held poll with complete headers and no body read trips neither timeout: test. Suite, TSan and CI

## Contracts

Provides: `http::Limits` and the `HttpAsioService(handler, reactors, const Limits&, Tls)` constructor, `close_idle()`, keep-alive refusal after `stop_accepting()`. Consumes reactor-server-composition's stop phases and `tls::server_context`. Consumed by server-limits and xapiand-lifecycle.

## Decisions

- D1 (library quests filed here, Detent D81), D2 (stop phases consumed), D77 (the grace is `Limits`' fixed 5 s), D72 (`--max-clients` bounds plain and TLS connections, applied in server-limits).
- Settled 1 and 6 of the note as the plan cites them.
- No open owner question touches this quest.

## May land early

None.
