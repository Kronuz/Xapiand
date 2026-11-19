# http-halfclose: journal

State: waiting on jobs 261118-180446-6142f8 261118-180447-06d623 261118-180449-60722d 261118-180450-67e80e
Quest: .campaign/xapiand/quests/http-halfclose.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs queued at http:33c3f7f (suite, TSan) and at the mutant http:8781b59 (local branch
`mutant/xapiand/http-halfclose-a1`: 33c3f7f with http_asio.h and http_message.h from the base;
never pushed, never landed). Branch pushed; CI runs on quest/xapiand/http-halfclose.

## Next

1. Record the four jobs and CI; on green, set State to ready for review.
2. Once the lead widens the file set (Blocked on), update AGENTS.md:155-158 and CHANGELOG.md.

## Blocked on

The lead: AGENTS.md (lines 155-158, "EOF or a read error ... calls Request::cancel(http::disconnected).
That is the per-request rule and it differs from the deferred-reply monitor's") now states the old
rule, and CHANGELOG.md has no entry for a behavior change Detent and Xapiand see. Both are outside
the file set (http_asio.h, http_message.h, test/**, docs/**, CMakeLists.txt). Options: (a) widen
the set to AGENTS.md and CHANGELOG.md and I update them here (recommended: one commit, the docs
match the code that lands); (b) a follow-up quest. Nothing else waits on it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | suite 261118-180446-6142f8 at http:33c3f7f; `--fails-without` 261118-180447-06d623 (coroutine at the mutant) |
| A2 | suite 261118-180446-6142f8 (coroutine [J] reset and failed-write checks, [C] reset) |
| A3 | suite 261118-180446-6142f8; `--fails-without` 261118-180449-60722d (readahead at the mutant) |
| A4 | TSan 261118-180450-67e80e |
| A5 | suite 261118-180446-6142f8; CI on quest/xapiand/http-halfclose (pending) |
| A6 | [artifact] http_asio.h ConnectionInput comment (was 426-431), http_message.h `disconnected` and `Request::cancel` comments, docs/mechanisms/coroutine-handlers.md "Cancel reads, not writes" (one rule, failed write, what ends a waiting handler) |

Lead's message (Detent's long-poll shape): done. coroutine_test [J] `/poll` is that shape (a
handler held on its token up to its own bound): half-closed, it is answered "poll:bound" at its
bound (301 ms of 300); half-closed and then reset, nothing is seen until the bound, then the write
fails and cancels the request (the kept token reads http::disconnected; released 242 ms after the
reset, the poll ran 301 ms). Nothing waits past the handler's own bound.

Local run (macOS, RelWithDebInfo, the tree of http:33c3f7f): 13/13 ctest pass.

## Result

http:33c3f7f. EOF on a connection with a request in flight ends reading and cancels nothing;
a reset cancels (watch) or closes the reply exchange (monitor); a failed reply write now also
calls `Request::cancel(http::disconnected)`. Watch and monitor read one rule (a read error other
than EOF is a disconnect). The connection already outlived the handler and its reply (the
handler is a child task, the reply is written before the next read), so the only lifetime
change is that nothing is read after EOF: the next read takes the held EOF and the connection
closes after the last response.

What ends a handler that never completes after a half-close: its own bound (a search budget, a
long poll's maximum, a deferred reply's deadline) or shutdown (abort, then force after the grace).
The transport cannot see a full close behind the FIN until it writes, and its request timeout
bounds reads and writes, not the handler. That is D126's accepted trade.

How it works: docs/mechanisms/coroutine-handlers.md, "Cancel reads, not writes".

## Rulings I made

- One rule for watch and monitor: EOF is a half-close in both; the monitor keeps treating our
  own `operation_aborted` (a forced close at shutdown) as a disconnect, the watch does not (its
  reads are cancelled by the session's deadlines).
- TLS: only close_notify maps to EOF; a TCP FIN without close_notify stays `stream_truncated`,
  a disconnect, as asio::ssl reports it (http_asio.h cannot name asio::ssl errors without OpenSSL).
- A failed reply write cancels the request with `http::disconnected` (first call wins, so a
  shutdown that already cancelled keeps its reason). This covers a deferred reply that timed out
  or was abandoned too: the connection ends there either way.
- A body cut short (EOF before Content-Length or the last chunk) still fails the body and
  cancels: that is a truncated request, not a half-close after sending one.
- Tests that used a half-close or a FIN as "a hang-up" now use a reset (SO_LINGER 0); [T] (e)
  reads its 503 after a shutdown cancel instead of after a half-close.
- A1's fails-without runs at a mutant commit (the tests at 33c3f7f with the base transport), not
  a tests-first commit, so the branch history never holds a red commit.
