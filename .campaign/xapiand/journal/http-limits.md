# http-limits: journal

State: working
Quest: .campaign/xapiand/quests/http-limits.md (the lead's; never edit it)

## Doing

Code and tests done on quest/xapiand/http-limits (http:96f9fe0); local suite 14/14 (macOS).
Next: mutant branches, then verify, TSan and mutant jobs.

## Next

1. Mutant branches `mutant/xapiand/http-limits-*` and their `--fails-without` jobs.
2. `campaign verify http-limits`, a TSan job; docs (TRANSPORT.md, coroutine-handlers.md), CHANGELOG.
3. Merge http main once detent:http-drop-user-data lands (expected conflicts: http_asio.h, tests).

## Blocked on

Lead (msg-147 to the lead): the file set lacks CMakeLists.txt (the reactor pin, limits_test's
registration), bench/server_compare.cc and bench/idle-cost/server.cc (callers of the removed API;
CI builds server_compare), README.md, AGENTS.md, ARCHITECTURE.md (name the removed API). Made the
mechanical edits there in their own commits (http:c10b9ff, http:30e48f8, http:96f9fe0's
CMakeLists hunk); recommend widening the set to them.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <pending> |
| A2 | <pending> |
| A3 | <pending> |
| A4 | <pending> |

## Result

<pending>

## Rulings I made

- F1 (reactor review): option (b), the gate at a request's first byte. `first_byte()` enters the
  gate, a seq_cst fence, then reads `closing`: a request that read it clear is counted by
  `wait_requests()` (fence against stop_accepting()'s store and wait_requests()' seq_cst read). A
  request whose first byte comes after stop_accepting() is refused 503 (no Retry-After) at
  headers-complete and holds no lease. The window left: a connection accepted but not yet read.
  Cost: one fence per request; slowloris holds a gate slot from its first byte, bounded by
  max_clients (1024) already below max_in_flight per loop (1024).
- F2: `ServiceState::closing`, an atomic in http set by HttpAsioService::stop_accepting() and
  stop(); callers stop through the service, which now forwards all four phases.
- After stop_accepting() idle keep-alive connections stay open and a request on one is refused
  503 + Connection: close (A3), not closed at once: a client's in-transit request gets an
  explicit refusal instead of a reset it cannot retry safely. cancel_requests() ends them.
- close_idle(): an epoch (`idle_closes`) read at connection start plus a per-loop idle list
  (reactor::AbortList of IdleWait nodes) walked by a post to each loop. Idle connections are
  cancelled at once (ConnectionInput::cancel_read, as a requested token does); a connection busy
  at the call replies Connection: close and closes, or closes at its next idle entry.
- Tls lives in http_asio.h with a forward-declared asio::ssl::context; the TLS constructor is
  declared there and defined in http_asio_tls.h, so plain users keep no OpenSSL dependency.
- TlsFailure::connections removed: max_clients bounds TLS at the accept, so it never fires.
- handler_count_test and the two bench servers keep the timeouts off (their measurements are of
  the bare path); the defaults now turn them on.
