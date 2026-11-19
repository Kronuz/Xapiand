# http-halfclose review: correctness

Commit: http:cd6ed31015aed61c506261a8dfa8a62b7267f4ad (the code at http:33c3f7f; cd6ed31 adds AGENTS.md and CHANGELOG.md only)
Verdict: APPROVE
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261118-180446-6142f8 (suite at 33c3f7f, 13/13) and 261118-181016-e8d252 (suite at cd6ed31, exit 0).
- 261118-181018-e0741e (TSan suite at cd6ed31, 13/13, `halt_on_error=1`): A4.
- 261118-180447-06d623 (`--fails-without`, coroutine_test at mutant http:8781b59 = the tests with
  the base http_asio.h and http_message.h, checked by diff): exit 8 on 11 checks, all of them
  half-close or failed-write claims ([C] write not cancelled, [J] plain/poll/deferred/failed-write,
  [S] half-close after a complete body). It fails for the right reason: a test failure, not a build
  failure or a timeout.
- 261118-180449-60722d (`--fails-without`, readahead_test at the same mutant): exit 8 on 10 checks,
  all half-close ones ([U], [L] "starts no read", [D] pipelined not cancelled, [Q] poll and cb).

## What I checked myself

Scripts are in `.scratch/xapiand/review-http-halfclose-correctness/`. All of them use one TSan Debug
build (asserts live), at `build.sh`, in slot `review-halfclose-correctness`.

- **Stress against a sequential model, job 261118-193023-c44ea8 (`stress.sh`, `probe_halfclose.cc`): PASS.**
  The setup was 8 client threads on 2 reactors, 3 seeds, 120 iterations each, so 2,880
  connections. The scenarios were pipelined requests then a half-close (453), keep-alive then an
  idle EOF (468), a body cut short by a half-close, buffered or streamed (482), a reset at a random
  moment (491), a half-close then a reset (501), and a head cut short (485).
  - Every request read before the EOF was answered in order with its expected body, and none was
    cancelled.
  - A cut buffered body never reached its handler. A cut streamed body failed its reader with an
    error and reason `http::disconnected`.
  - A reset cancelled with `http::disconnected` or not at all, and every handler finished.
  - An idle EOF closed in at most 2.0 ms.
  - At the end every admission was released, no task frame was left, and TSan reported nothing.
  - The first run (261118-181943-71439b) failed only on my own model: a streamed handler that
    answers after its body fails gets that answer written. That is correct behavior, and v2 expects it.
- **Targeted mutants, job 261118-181954-0cfc1d (`mutants.sh`).** Each mutant was rebuilt and run
  against coroutine_test and readahead_test.
  - M1, no cancel after a failed buffered write: killed by [J] "the failed write cancels" (both cases).
  - M2, no cancel after a failed streamed write (http_asio.h:1079): **survives** (F1).
  - M3, `completed()` lets EOF fire (the old rule): killed by [C], [J], [U], [L] and [D].
  - M4, `watch()` set after EOF fires: killed by [U] and [L].
  - M5, resets never seen: killed by [C], [J], [S], [U], [L], [D] and [Q]. This proves A2's reset tests.
  - M6, `monitor()` closes on an EOF already read: killed by [J] deferred.
  - M7, `watch()` set after EOF arms a new read: killed by [U] "starts no read".
  - The repo's `test/watch_mutants.sh` on the same build killed all 7 of its mutants, and
    readahead_test passes unmutated. The rewritten [U] and [L] checks still kill the
    `eof-as-data` and `engage-lazy` mutants.

**The five questions:**

1. **Lifetime.**
   - The handler is an awaited child task, so the request, the writer and the socket outlive it.
     The input is a `shared_ptr` that its pending operation holds, and `serve_connection` closes
     the socket and then drains.
   - After EOF the input holds the EOF (`ready_`) and arms nothing. M7 is killed, and [U] and [L]
     show that no read is pending. The next `read_ahead` takes the EOF and returns, so the
     connection lives exactly until the last response is written.
   - A handler that finishes after `force()` closed the socket fails `send_now`. Its cancel loses
     to the shutdown reason (the first call wins), and the frame returns ([F] in the suite).
   - A handler that never completes holds its connection and admission until its own bound or
     until shutdown (abort, then force, and [T] (d) shows no frame outlives `stop()`). That is
     D126's trade, and it is documented (F3).
2. **A partly read body.** EOF still fails the request and nothing hangs. A buffered body returns
   at http_asio.h:1101 without reaching the handler ([B], and the stress probe's cut buffered
   bodies). A streamed body goes through `fail_body(connection_reset)` and is cancelled ([H], and
   the stress probe).
3. **Races.** There is no double cancel (`Request::cancel` is first-call-wins), no double close
   (one `close` in `serve_connection`, and `force()`'s close is idempotent) and no skipped
   pipelined response. The evidence is [D] d4/d5, the stress probe's 453 pipelined half-closes and
   501 half-close-then-reset cases, and TSan reporting nothing.
4. **Idle keep-alive.** An EOF with no request in flight closes promptly (2.0 ms at most in 468 cases).
5. **Test quality.** The tests check values: reasons, bodies, order, timing bounds, admission
   release and "no read pending". Both `--fails-without` jobs fail on the intended checks, as
   listed above.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | The new `request.cancel(disconnected)` after a failed write on the streamed-body path is untested. Removing it passes both suites. The buffered path's copy of the same line is tested. | mutant M2, job 261118-181954-0cfc1d; http_asio.h:1079 | Add a [J] case: a `wants_body_stream` handler that keeps its token and answers 32 MiB, then a reset; assert the token reads `http::disconnected`. |
| F2 | no | nit | `respond()` also returns false for a deferred reply that timed out or was abandoned, and for a write cut by the request deadline. The request is then cancelled with `http::disconnected` while the client may still be connected. The `disconnected` comment (http_message.h:53-55) names only a reset or a failed write. | http_asio.h:741-770, 1079, 1125 | Options: keep it (the connection ends either way, and the journal rules so) and say so in the comment, or cancel those cases with a distinct reason. Owner's call. |
| F3 | no | nit | D126's trade, for the record: a client that times out and calls `close()` sends a FIN, not a RST. Its in-flight request now runs to its own bound, holding its admission slot and connection, where before it was cancelled at once. This is accepted and documented (CHANGELOG "For callers"); I did not measure the cost. | design; docs/mechanisms/coroutine-handlers.md "Cancel reads, not writes" | None (owner ruled). Keep in mind for any handler whose only bound is the token. |
| F4 | no | gap | A TLS half-close (close_notify) with a request in flight, and a write after it, are not covered by any test. tls_asio_test covers only idle close_notify. The quest's criteria are plain TCP. | test/tls_asio_test.cc:355-363 | Optional: one tls_asio_test case, close_notify after a request, expecting its response. |

## Not run

- TLS half-close with a request in flight (F4), and any check on Linux or FreeBSD: those are CI's,
  and CI was still running at cd6ed31 when I wrote this.
- The Xapiand side (repin plus `wide_search_halfclose.py` for :search and :count) is a separate step under D126.
- Owned-stream replies after a half-close: their monitor path is covered by M6 only through the
  deferred reply.
