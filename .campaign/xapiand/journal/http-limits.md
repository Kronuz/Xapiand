# http-limits: journal

State: waiting on jobs 261119-160804-917397
Quest: .campaign/xapiand/quests/http-limits.md (the lead's; never edit it)

## Doing

Done at http:121aa41 on quest/xapiand/http-limits (not pushed, per the lead). Waiting on the
suite job; then ready for review.

## Next

1. Merge http main once detent:http-drop-user-data lands (expected conflicts: http_asio.h's
   writer and connection body, timeouts_test's new deadline section, which takes `Limits`), then
   rerun verify, TSan and the mutants (regenerate them with `.scratch/mutants.py` in the
   worktree).
2. Push for CI (the lead's call); A4 names CI.

## Blocked on

Lead (msg-147 to the lead): the file set lacks CMakeLists.txt (the reactor pin, limits_test's
registration), bench/server_compare.cc and bench/idle-cost/server.cc (callers of the removed API;
CI builds server_compare), README.md and AGENTS.md (name the removed API). Made the mechanical
edits there (http:c10b9ff, http:30e48f8, http:96f9fe0's CMakeLists hunk, http:658b112);
recommend widening the set to them. ARCHITECTURE.md needed no change.

## Evidence

Mutants are side branches `mutant/xapiand/http-limits-*` in the http repository at http:96f9fe0's
code, never landed; each must fail `limits_test`:

| Job | What |
|---|---|
| 261119-160804-917397 | suite at http:121aa41 (queued) |
| 261119-160805-c2cd28 | TSan suite at http:121aa41: 14/14 |
| 261119-155941-ca6165 | a1-clients (max_clients never reaches the server) |
| 261119-155943-6f93d3 | a1-body (max_body never reaches the parser): 1001-byte body not 413 |
| 261119-155944-5fd2a2 | a1-handshakes (tls_handshakes never caps) |
| 261119-155946-be3af9 | a2-walk (close_idle never walks): idle connection open 2 s |
| 261119-155948-717b8b | a2-busy (busy connection ignores close_idle): no Connection: close |
| 261119-155949-3bb6e3 | a3-flag (stop_accepting leaves the flag clear): keep-alive request served, replies keep-alive |
| 261119-155951-43f37e | a3-gate (gate at headers-complete, the old code): wait_requests misses a request mid-headers |
| 261119-155953-7e73c2 | a4-header (header timeout bounds the handler): the held poll gets no reply |

| Criterion | Evidence |
|---|---|
| A1 | 261119-160805-c2cd28 (suite 261119-160804-917397 queued); fails without: 261119-155941-ca6165, 261119-155943-6f93d3, 261119-155944-5fd2a2 |
| A2 | 261119-160805-c2cd28; fails without: 261119-155946-be3af9, 261119-155948-717b8b |
| A3 | 261119-160805-c2cd28; fails without: 261119-155949-3bb6e3, 261119-155951-43f37e |
| A4 | 261119-160805-c2cd28 (TSan clean); fails without: 261119-155953-7e73c2; CI pending a push |

Tests: `test/limits_test.cc` ([B] each field at its bound and the refusals, [I] close_idle, [S]
stop_accepting and F1, [P] held poll), every section plain and TLS. Local macOS run: 14/14, limits
10.7 s.

## Result

http:c10b9ff..http:121aa41. `HttpAsioService(handler, reactors, const http::Limits&, http::Tls)`
takes every bound; the setters, `tls_connection_factory`, `AsioResponseOptions`' limits, TLS's own
connection count and `TlsFailure::connections` are gone; defaults per the note (1024/1024, idle
75 s, header 30 s, inactivity 60 s) apply to plain and TLS. The service forwards the four stop
phases; after `stop_accepting()` a request is refused 503 and replies close; `close_idle()`
closes idle connections at once and busy ones after their reply. F1 closed by (b), the
first-byte gate. How it works: http `docs/mechanisms/coroutine-handlers.md` ("Shutdown",
"Closing idle connections"), `docs/TRANSPORT.md`; `CHANGELOG.md` 2026-11-19.

Open: the per-request seq_cst fence is unmeasured (no perf criterion); the defaults add a
deadline per read on paths that had none, one io handler per request (handler_count_test, which
now sets them off); Xapiand, Detent and Prism move with server-limits and their adoptions.

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
