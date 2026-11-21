# tls-watch-cancel review: correctness

Commit: http:e4ad5b94a41fe851f6132d7c93506fa7d6f7439d (quest/xapiand/tls-watch-cancel; diff aed0314..e4ad5b9)
Verdict: APPROVE
Date: 2026-11-21

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261121-102716-4e6b27: `tls_watch` fails at http:445f1b2 (the test without the fix) on its
  wake-window check. That is the A1 fails-without run.
- 261121-102751-26bc30: `tls_watch`, `tls_asio`, `limits` and `readahead` pass at e4ad5b9 (Debug).
  `limits` covers close_idle() and the phased stop over TLS. Those paths reach `cancel_read()`
  through `IdleWait` and now end the TLS read through the slot alone.
- 261121-103400-803d9e: the build-test check at e4ad5b9 (A3 met in `campaign status`).
- A2 (TSan) is CI's linux-tsan job. I did not read its result.

## What I checked myself

Scripts are in `.scratch/xapiand/review-tls-watch-cancel-correctness/` (home checkout).

- Job 261121-104541-7b84d9 (`probe.sh`, a fresh checkout of e4ad5b9, Debug, exit 0):
  - `tls_watch` x40: 40 pass, 0 fail.
  - `tls_watch_probe_test.cc` P1-P5 pass. P1: a cancel while the lowest-layer read waits ends the
    read with `operation_aborted`. P2: after that terminal cancel, the next read on the same stream
    returns the next record. P3: two `cancel_read()`s with no read pending (the slot still holds
    the completed read's state) do not reach the read armed after them. P4: a token requested
    before the reader suspends ends the pending TLS read, and the stream reads on after it. P5: a
    cancel after the watch's read completed with data loses no byte.
  - Mutant M1 binds the slot but never emits it (`cancel_read()` keeps the old socket cancel).
    It kills `tls_watch` at "a cancel between the wake and the stream's continuation ends the read
    with operation_aborted". The probes pass under M1, as they should: they do not target the
    window.
  - Mutant M2 emits `partial`, which the ssl io_op's default `enable_terminal_cancellation` filter
    drops. It kills `tls_watch` at the same check, and P1 too.
- `c2_ranges.py`, rerun locally from job 261109-150350-f39012's per-round files (`runs/*/r0N.txt`).
  Every cell of the watch-peek.md timing table matches the recomputation: the medians, a/base with
  its range, a/aa, and CPU. That includes the three corrected ends, 1.97, 1.12 and 0.90. The other
  ranges are unchanged and correct. The table's medians are `statistics.median` over 6 rounds (the
  mean of the middle two). They differ from the job's `medians.txt`, which takes a single round's
  value; the doc says "medians", so the table is the right reading.
- I read the code against asio 1.36 `ssl/detail/io.hpp` (lines 260-276, the `cancelled()` check
  after `put_input`), `cancellation_state.hpp`/`base_from_cancellation_state.hpp` (the default
  filter is terminal only), `cancellation_signal.hpp` (emplace replaces in place; nothing clears
  the slot on completion), and `reactive_socket_service_base.hpp:241`.

### The five questions

1. Slot lifetime. `watch_cancel_` is bound only by `arm()`'s TLS read, and `pending_` allows one
   such read at a time. The direct path (`task::await_asio`) has its own signal per awaiter, so no
   two reads share the slot. After a read completes, its cancellation state stays in the slot
   until the next `arm()` replaces it. An emit in between reaches a completed op, and its child
   `reactor_op_cancellation` has a key no live op carries, so it does nothing: P3 and P5 show it.
   The next read's state is fresh (`cancelled_ = none`), so a stale emit cannot end a later
   re-arm. The ssl io_op's handler holds `owner`, so the signal outlives every op that installed
   state in it. The callers of `cancel_read()` are `Read::Cancel` and `IdleWait`. Both are scoped
   inside `serve_connection_body`, and `serve_connection` closes the socket and drains before the
   stream dies, so nothing emits after the stream is destroyed. When the completion handler
   re-arms inline from inside the old io_op's `call_handler`, the new state is emplaced over the
   old one. The old io_op does not touch its state after `call_handler`, so that is safe.
2. State after a terminal cancel. In the window, the bytes the lowest-layer read got are already
   in the engine (`put_input`) before the check, so nothing is lost. During a pending socket read,
   that read is aborted and consumes nothing. In both cases the stream reads on: `tls_watch`'s last
   check sends only the rest of the record and reads "x"; P2 and P4 show the same. That matches
   what TCP gives after `cancel_read()`. In the server, every aborted watch read ends the
   connection anyway: read_ahead returns, and the body reads return or set `keep_alive = false`
   on `Fed::Abandoned`/`Failed` (http_asio.h:968-970, 1032-1034, 1070, 1090-1092). So a kept-alive
   re-read after a cancel is never relied on.
3. The window is reached deterministically. A wait queued first on the socket sits ahead of the
   recv in the same reactor read queue (no speculative read while that queue is non-empty), so
   one pass completes both and the wait's handler runs first. The test also witnesses the window
   itself: the follow-up read gets "x" after only the record's remainder is sent, which proves the
   aborted read had consumed the first piece. M1 (bound, never emitted) and M2 (wrong type) both
   fail it, 40/40 at the fix.
4. Plain TCP is unchanged on POSIX. `kLazyWatch` is true for `tcp::socket`, and both
   `cancel_read()` and `arm()` keep the old code there. The only cost is one unused
   `cancellation_signal` member, one pointer per connection.
5. The C2 numbers are recomputed from the recorded rounds, not retyped: see `c2_ranges.py` above.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | `Read::await_suspend`'s comment still says a requested token ends the pending read "by cancelling it on the socket and marking the wait cancelled (see readable())". On TLS it now ends through the watch's cancellation slot. | http_asio.h:483-484 | Optional: say "through `cancel_read()`". |
| N2 | no | nit | On TLS, `cancelled_ = true` in `cancel_read()` is a dead write: only the TCP `readable()` reads it. It is harmless and keeps one entry point. | http_asio.h:529, 604 | None needed. |
| N3 | no | gap | Pre-existing, not introduced here. A terminal cancel that lands while the ssl read op is writing engine output (a TLS 1.3 KeyUpdate reply during `SSL_read`) aborts that write mid-record. After `Fed::Abandoned` the reply is still written on that stream. The old socket cancel and the direct path's `cancellation_type::all` emit share this. It is rare: it needs a client KeyUpdate exactly then. | asio io.hpp:184-200, 296-301; http_asio.h:1070-1080 | The owner's call, as a hunt entry if wanted. Options: (a) keep it; (b) close instead of replying after an aborted TLS read. |
| N4 | no | nit | `cancel_read()` is loop-only: `asio::cancellation_signal::emit` is not thread-safe, the same contract the plain `cancelled_` bool already had. Every caller runs on the loop: deadlines on the loop scheduler, `SessionControl::abort` on the loop (reactor.h:449), close_idle()'s walk on the loop. The function does not say so. | http_asio.h:527-538; reactor.h:442-452 | Optional: say "on the loop" in its comment. |
| N5 | no | nit | Without `MSG_DONTWAIT` (`kLazyWatch` false), plain TCP takes the slot branch too. That is behavior-equivalent, but no CI platform builds it. | http_asio.h:400-404 | None needed. |

## Not run

- The linux-tsan CI job (A2) and the full suite (A3's build-test check): those are CI's and the
  check's. I did not read their results.
- No Release or sanitizer build of the probes, and no Linux or FreeBSD run of `tls_watch`. The
  reactor-order argument for epoll and kqueue rests on asio source, and the CI matrix runs the
  test on those platforms.
- No probe of N3 (a KeyUpdate during a cancelled read); it rests on asio source.
- The quest's "rerun at the landed code" for A1, and `campaign status`'s request to rerun
  261109-150350-f39012 and 261121-102716-4e6b27: those are for the lead.
