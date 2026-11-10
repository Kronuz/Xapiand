# perf-watch-peek review: correctness

Commit: http:51d153aa1bc33a433b0c1b4dad56671d01f89d27
Verdict: APPROVE
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- 261109-145852-514f5a: build-test (Release, 13 ctest targets including readahead, tls_asio,
  handler_count, timeouts) at http:13933f0. Its library code is the reviewed head's (51d153a
  touches only CHANGELOG.md and docs/perf/watch-peek.md).
- 261109-150416-197873: watch_mutants.sh at 13933f0, Debug. All five mutants killed (no-resume,
  no-cancel, engage-lazy, engage-data, eof-as-data), and the unmutated header passes readahead_test
  with asserts live.
- 261109-145905-5a00d5: handler_count against base's http_asio.h fails as it must (/asks 3, against 1).
- 261109-150350-f39012: the exclusive A/B. I re-read its raw rounds myself (see risk 4).

## What I checked myself

Scripts are under `.scratch/review-perf-watch-peek-correctness/`.

- `sanitize.sh asan|tsan` runs the whole ctest suite plus `probe_walk.cc`. Each flavor is a Debug
  build with asserts live, in its own build tree and FetchContent directory.
  - 261110-023742-b509ea (ASan+UBSan): 13/13 pass, and probe_walk passes.
  - 261110-023743-2b2a9e (TSan): 13/13 pass, and probe_walk passes.
  - Neither log has a sanitizer report.
- `probe_walk.cc` is a seeded random walk: 150 walks of 80 steps over ConnectionInput.
  - Steps: sends of 1 to 5000 bytes of a non-periodic stream, lazy watch with `engage()`, non-lazy
    watch, unwatch, and plain reads.
  - It also does reads whose token is requested before start, right after suspend, or after one
    loop turn: about 1710 such reads, about 660 of them aborted on the pending watch read.
  - Each walk ends in one of three teardowns: EOF, close then `drain()`, or the loop destroyed with
    a reader and the watch read both pending.
  - Invariants: bytes read equal bytes sent, byte for byte; an aborted read carries no bytes; the
    disconnect callback never fires without a disconnect; no read hangs.
  - Result: 0 bad, 0 hangs, under both sanitizers.
- `ab_ratios.py <job out>/watch-ab/runs` recomputes per-round after/before and A/A ratios from
  f39012's raw rounds.
- Read for re-arming of an already-readable socket: Asio 1.36 `kqueue_reactor::start_op` and
  `epoll_reactor::start_op`.
  - The non-speculative branch, which is the one `async_wait` takes, re-adds `EV_ADD|EV_CLEAR` or
    issues `EPOLL_CTL_MOD`, so an edge that was consumed earlier is reported again.
  - So `monitor()` and the non-lazy `watch()` can arm `async_wait` without a `recv` first, and still
    not miss data already in the kernel.
- Shutdown's `abort()` runs on the reactor's loop (reactor ab69cf2 `on_loop_sync`). So the reader's
  `Cancel` (`socket.cancel()` from the requesting thread) is loop-local. This is the same as the old
  `read_held`.

Risk answers:

1. **Lifetimes.**
   - Each armed handler holds `owner`, a `shared_ptr` to the input, so the input outlives any
     pending read.
   - `Read` and `Drain` call `forget()` on destruction, so a frame destroyed while it waits leaves
     no waiter behind.
   - `serve_connection` closes the socket and then drains, so `stream_` outlives `readable()`.
   - At teardown the scheduler destroys frames after the loop stops, and `io_` destroys handlers
     without running them.
   - ASan, TSan and probe_walk's destroy teardown are all clean.
2. **Byte loss or duplication.**
   - `engage()` now reads up to kReadAhead bytes; data ends the watch. The bytes go back through
     `take()` first and in order. The session reads only when `buffered` is empty, so order holds.
   - Pipelined and simultaneous arrival are covered by [Q], [P] and [F] in build-test, and by
     probe_walk.
   - TLS is untouched by the peek change: `kLazyWatch` is false, `receive_now` is compiled out, and
     TLS still arms `async_read_some`. Only the waiter mechanism is shared, and `tls_asio` passes
     under both sanitizers.
3. **Error and cancellation paths.**
   - Every completion reaches `completed()` exactly once. `readable()` either re-arms (pending
     stays set) or completes, and an arm that throws completes with `no_memory`.
   - `resume_waiter()` `exchange`s the waiter, so each waiter resumes at most once.
   - The token paths (requested before, during and after suspend) are covered by [U] and the probe.
   - One narrow gap remains: N1.
4. **The A/B claim.**
   - Like with like: the same `build_dirs`, Release, with the same toolchain. Each binary is stamped
     with its commit and checked with `file`. Base is http:8262172, which is main plus only the
     `httpc` bench kind.
   - Rounds are interleaved: a, aa, base, e1 in each round.
   - A/A is a copy of a's binary.
   - Recomputed: `httpc` 1 conn is 1.39x faster after (1.37-1.97) against a/aa 0.96-1.00, and 8
     conn is 1.10x faster (1.06-1.18) against a/aa 0.93-1.05. Both hold.
   - The 64-connection row is stated honestly as noise: 0.82-1.12, against a/aa 0.82-1.10. See N2
     and N4 for how the A/A column is printed.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | defect | A cancel can be lost in a narrow window. The reader's `Cancel` ends the wait with `socket.cancel()`. If the `async_wait` has already completed (its handler is queued) when the token is requested, that cancel finds nothing to cancel. If `readable()` then gets `EAGAIN` (a spurious wake), it re-arms, and the reader waits past its deadline or shutdown until data or EOF arrives. Asio's own `async_read_some` has no such window, because its retry stays inside the reactor op that cancel reaches. This needs a spurious readiness on TCP, so it is rare. | http_asio.h:612-617 (`readable`), :527 (`cancel_.emplace`) | Have `Cancel` set a flag on the input (or check the waiter's token) that `readable()` checks before re-arming; complete with `operation_aborted` instead. |
| N2 | no | nit | The A/A column in docs/perf/watch-peek.md's Timing table is printed in the opposite orientation to "after vs before" on several rows. Example, `http` 1 conn: printed 0.81-1.00, but a/aa is 1.00-1.24. With the printed numbers, the doc's own rule ("range on one side of 1 and ratio outside the A/A range") would count `http` 1 conn (1.01-1.22, ratio 1.04) as a win. In the same orientation it is noise, which is what the doc says. Every verdict is right; the column is not. | `ab_ratios.py` over f39012 runs/; docs/perf/watch-peek.md Timing table | Print the A/A range as a/aa, matching the ratio column. |
| N3 | no | nit | "An asking request costs the same two `recv` calls as one that never asks" is true for `recv` only. Each `async_wait` arm on an empty op queue issues a `kevent(EV_ADD)` or `epoll_ctl(MOD)`, so asking still costs one syscall more than not asking. That may be part of the unattributed 0.7-1.1 us residual. | Asio 1.36 kqueue_reactor.ipp / epoll_reactor.ipp `start_op` (non-speculative branch); docs/mechanisms/coroutine-handlers.md lazy-watch paragraph | Say it in the doc, or count syscalls in the counting build. |
| N4 | no | nit | The series order inside a round is fixed (a, aa, base, e1), interleaved but never rotated. The A/A spread is wide and one-sided at 1 conn (`http` a/aa 1.00-1.24), which suggests a position effect. The `httpc` 1 conn effect (1.37-1.97) is far outside it, so the conclusion stands. | f39012 run.log order; `ab_ratios.py` | Rotate the series order per round in bench/ab.sh for future A/Bs. |
| N5 | no | nit | `resume_waiter()` resumes inline without the `inline_resume_depth` bound that `task::await_asio` applies. Today's structure makes one resume per Asio handler, so depth is bounded anyway, but this is a second copy of the rule that can drift. | http_asio.h:573-579; task bdafa5f task_asio.hh `asio_park::resume` | Call task's resume helper if it is exposed, or note the dependency. |

## Not run

- Linux (epoll), FreeBSD and arm64. These sanitizer results are macOS x86-64 Apple clang only.
  The epoll re-arm conclusion comes from reading the code, not from a run.
- A probe that forces the spurious-`EAGAIN` interleaving of N1. It needs a hook between the
  readiness and `readable()`; there is none.
- A mutant against probe_walk itself. The existing five mutants (197873) cover the claims it checks.
- No timing rerun, and no TLS-specific A/B.
