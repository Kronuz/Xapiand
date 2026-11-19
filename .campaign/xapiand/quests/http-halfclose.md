# http-halfclose: A half-closed client gets its response

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status http-halfclose`.

## Outcome

A client that half-closes its connection after sending a request (shutdown(SHUT_WR)) still gets that request's response. Read EOF on a connection with a request in flight stops reading but no longer cancels the request; only a connection reset or a failed write cancels it (D126). Today `ConnectionInput::completed` (http_asio.h:643-651) fires the in-flight request's CancelWatch on any read error including EOF, so Xapiand answers a half-closed :search or :count with 503 (probe 261118-170529-87c0b4).

## Acceptance criteria

- A1 A half-closed client gets the full response for a request whose handler checks its cancel token (a deferred handler that waits on the token, and a plain one), and the connection then closes cleanly: a test that fails at the parent commit (`--fails-without`) and passes at the fix
- A2 A reset (SO_LINGER 0 close) and a failed write still cancel the in-flight request: a test with values (the handler sees `http::disconnected`)
- A3 Pipelined requests after a half-close: requests already read are all answered in order, and no new read is started: a test
- A4 TSan build of the suite passes: a job
- A5 The suite passes and CI is green on every platform
- A6 [artifact] The comment at http_asio.h:426-431 and the docs describe the new rule: `docs/`

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Owner 2026-11-18 (D126): half-close keeps the response; cancel on reset or failed write only. High risk: connection lifetime across async handlers. Xapiand repins http afterwards (a separate step) and its probe harness/wide_search_halfclose.py must then pass for :search and :count. Http is shared with Detent: tell its lead when this lands.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
