# http-halfclose: journal

State: waiting on jobs 261118-201311-bc538f 261118-201313-8f56ef 261118-201316-1f8ff0, CI run 44456767194
Quest: .campaign/xapiand/quests/http-halfclose.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head http:ec273dd. Review APPROVED at cd6ed31 (accepted by the lead); CI at cd6ed31 failed every
Linux job on one check, readahead [L] "engage() after a reset fires the disconnect" (macOS passed).

Cause: the test, not the transport. It waited for the reset with `recv(MSG_PEEK | MSG_DONTWAIT)`.
On Linux a recv that returns a pending socket error takes it (sock_error() clears sk_err, MSG_PEEK
or not), and the socket is then closed for reading, so engage()'s recv read 0. Under the old rule
EOF also fired, which hid it; under D126 EOF is a half-close. Nothing in the transport reads the
socket but the watch itself, and the [U] and [L] cases where the watch's own read meets the reset
passed on Linux in that same run. Fix: the test waits with poll() (takes nothing); a Linux-only
check pins the kernel behavior (a peek returns ECONNRESET, the next recv reads EOF). CI run
44456767194 is the Linux proof.

Review F1 (gap): coroutine [J] `/stream-big-token`, a wants_body_stream handler that keeps its
token and answers 32 MiB, then a reset: the token reads http::disconnected. `--fails-without` at
mutant M2 http:a243051 (local branch `mutant/xapiand/http-halfclose-m2`: ec273dd without the
streamed path's cancel), job 261118-201313-8f56ef. F2 (nit): the `disconnected` comment in
http_message.h now names every undelivered reply (failed or timed-out write, deferred reply ended
unwritten); behavior unchanged. F3: none (owner ruled). F4 (optional TLS case): not done, out of
the quest's plain-TCP criteria.

Jobs at ec273dd: suite 261118-201311-bc538f, TSan 261118-201316-1f8ff0, F1 mutant 261118-201313-8f56ef.
Local macOS run of ec273dd's tree: 13/13 pass.

## Next

1. Record the three jobs and CI 44456767194 (Linux); on green, ready for re-acceptance.

## Blocked on

Nothing. (The lead widened the file set to AGENTS.md and CHANGELOG.md; done in http:cd6ed31.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | suite 261118-201311-bc538f at http:ec273dd (passed at cd6ed31: 261118-181016-e8d252); `--fails-without` 261118-180447-06d623 passed (coroutine at the mutant http:8781b59) |
| A2 | suite 261118-201311-bc538f; F1 `--fails-without` 261118-201313-8f56ef at mutant M2 (coroutine [J] reset and failed-write checks, [C] reset) |
| A3 | suite 261118-201311-bc538f; `--fails-without` 261118-180449-60722d passed (readahead at the mutant) |
| A4 | TSan 261118-201316-1f8ff0 at http:ec273dd (passed at cd6ed31: 261118-181018-e0741e) |
| A5 | suite 261118-201311-bc538f; CI run 44456767194 at ec273dd (cd6ed31's failed on Linux: the [L] probe, fixed) |
| A6 | [artifact] `docs/mechanisms/coroutine-handlers.md`: "One rule reads the socket watch: a read error other than EOF (a reset) is a disconnect, and EOF" (also `http_asio.h`: "One rule reads it: a read error other than EOF (a reset) is a"; `AGENTS.md`: "One rule reads it: a read error other than EOF (a reset, not our own cancellation)"), all at http:ec273dd |

Lead's message (Detent's long-poll shape): done. Lead's message (widen to AGENTS.md, CHANGELOG.md): done in http:cd6ed31. Lead's message (Linux CI, F1): done in http:ec273dd, see Doing.

Detent's long poll: coroutine_test [J] `/poll` is that shape (a
handler held on its token up to its own bound): half-closed, it is answered "poll:bound" at its
bound (301 ms of 300); half-closed and then reset, nothing is seen until the bound, then the write
fails and cancels the request (the kept token reads http::disconnected; released 242 ms after the
reset, the poll ran 301 ms). Nothing waits past the handler's own bound.

Local run (macOS, RelWithDebInfo, the tree of http:33c3f7f): 13/13 ctest pass.

## Result

http:33c3f7f (code, tests, mechanism doc) and http:cd6ed31 (AGENTS.md, CHANGELOG.md). EOF on a connection with a request in flight ends reading and cancels nothing;
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
- The Linux CI failure is the test's probe, not the transport (see Doing); the test changed,
  the transport did not. A Linux-only check pins why a test must never recv to wait for a reset.
- F2: kept the behavior (any undelivered reply cancels with http::disconnected) and made the
  comment say so, rather than a new reason; the connection ends in every such case.
