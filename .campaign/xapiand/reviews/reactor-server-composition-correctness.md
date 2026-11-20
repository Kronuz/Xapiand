# reactor-server-composition review: correctness

Commit: reactor:9349464efec8203a86377c3d2590eb918c131e07 (code at reactor:3626f70; reactor:9349464 adds docs)
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-135828-bf6eab (suite at reactor:3626f70) and 261119-135829-521317 (TSan: [R], [H], [W],
  [K]): the code is unchanged from reactor:3626f70 to reactor:9349464.
- Mutants caught: 261119-135837-88bc47 (A1), 261119-135840-1dad3b (A2), 261119-135842-6ffa4b,
  261119-135845-fa3dd3, 261119-135847-85e4ab (A3: no-op stop_accepting, no wake, wake without the
  mutex), 261119-135850-7391bb (A4: legacy stop inserting wait_requests).
- 261119-135903-7920d2 (A5 strict A/B, within A/A noise).

Together they show each criterion's test and that it fails without its fix. I did not rerun them.

## What I checked myself

All by reading; no question needed a probe job, so there is no scratch directory.

1. **Lost wakeup in `wait_requests`** (`reactor.h:893-904`, `:208-212`, `:925-930`). The waiter's
   `armed` store precedes its gate reads in the seq_cst total order; a leave's `fetch_sub` and
   `armed` load are seq_cst too. A last leave whose `fetch_sub` comes after the waiter's read of
   that gate therefore comes after the store and sees `armed`, then takes the drain mutex the
   waiter holds from the read through `wait`, so the notify reaches the sleeper. This holds across
   several gates (each gate's read precedes its own late leave). The refused `try_enter` path
   (`leave()` from limit+1) wakes when it is the one reaching zero. The `cancel_callback` is
   declared before the `unique_lock`, so the lock is released before the callback's destructor
   waits for a concurrent `wake()`: no deadlock. task's `cancel_callback` runs `f` at once for a
   requested token and its destructor waits for a running `f` (task.hh `cancel_callback`).
   `drain_` is declared before `reactors_`, so a lease released in phase 3 or destruction finds it
   alive; `TcpServer` is non-copyable and so non-movable, so the gates' pointer cannot dangle.
   Answer: no missed leave.
2. **A connection accepted just before `stop_accepting`.** The count is gate leases only; nothing
   counts a session between accept and its first `gate().enter()`. In the shared accept path the
   session is even posted to another loop (`reactor.h:473-478`), and http enters the gate at
   headers-complete (http `http_asio.h:870`). Such a request is not waited for and is cancelled
   by `cancel_requests()` (F1). Legacy behavior cancelled it too, so nothing regresses.
3. **Keep-alive after the stop.** At the reactor level a request on an open connection is admitted,
   counted and served (documented; the token bounds the wait). Refusing it is http-limits A3; the
   reactor exposes no session-visible "accepting stopped" state (F2).
4. **Legacy `stop()` order.** Every current user reaches `TcpServer::stop()` through
   `http::HttpAsioService::stop()` (http `http_asio.h:1246`): Xapiand `src/manager.cc:1081`
   (grep with `--exclude-dir=xapian`), Detent `src/detent_relay/host.cc:432` and
   `src/detent_authority/main.cc:216`, Prism `src/main.cc:348`. None reads `Reactor::stopping()`
   or relies on cancellation before the accept loops end; http's only gate use is the lease at
   `http_asio.h:870`. `stop_accepting()` cannot block on a session: the accept loops never await
   admission (`reactor.h:666-667`, `:708-709`) and `stop_on_owner` cancels the retry sleep. A
   session accepted during the stop is tracked before the walk or aborted by `track()` after it,
   so the outcome matches the old order, which the earlier http review L3-N3 already called safe
   and D16 listed acceptors first. Answer: no user depends on abort-first.
5. **`raise_open_files`** (`reactor.h:723-746`). Linux: the soft limit can rise to the hard one
   unprivileged and the hard one is bounded by `fs.nr_open`, so `setrlimit` succeeds. macOS: the
   default hard limit is `RLIM_INFINITY`, and libc's POSIX-mode `setrlimit` returns `EINVAL` past
   `kern.maxfilesperproc` (OPEN_MAX is only the compat suggestion), so capping at the sysctl first
   is what makes the call succeed; for root the kernel's bound is the larger `kern.maxfiles`, so
   the cap is conservative there. FreeBSD: the kernel clamps to `maxfilesperproc` silently, and the
   function re-reads the limit, so the return is the truth. Never lowers; a refused `setrlimit`
   returns the limit in force. Findings F3, F4, F5.
6. **`server_context` and `seconds_to_expiry`** (`reactor_tls.h:91-141`). `ASN1_TIME_diff` with
   `from = nullptr` compares against `time(NULL)` through `OPENSSL_gmtime`: UTC, independent of
   `TZ`; the tests build dates with `X509_gmtime_adj`. `days * 86400 + rest` is right because
   OpenSSL returns both with the same sign. Unreadable dates now refuse the certificate, where
   Xapiand's `X509_cmp_current_time` returned 0 and accepted it: stricter, correct. Key mode,
   pair and `strict` match `src/tls.cc:51-76`. Edges in F6.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | A request is counted only from the session's `gate().enter()`. A connection accepted just before `stop_accepting()` (possibly still posted to its loop), or a request whose headers are still arriving, holds no lease: `wait_requests` can return true and `cancel_requests()` then drops it. D71's "in-flight requests finish" therefore excludes requests not yet admitted. No regression: the legacy stop cancelled them too. | `reactor.h:473-478`, `:925-930`; http `http_asio.h:865-870` | Owner or lead's call, for http-limits and xapiand-lifecycle: (a) keep, and state the window in docs/API.md and stop-and-accept.md; (b) http enters the gate at a request's first byte, which narrows it to accepted-but-unread connections; (c) reactor counts a launched session as pending until its first enter or idle read. |
| F2 | no | gap | Sessions cannot see that accepting stopped: `accepting_stopped_` is a private plain `bool`, and `Reactor::stopping()` flips only at `cancel_requests()`. http-limits A3 (refuse keep-alive requests after `stop_accepting()`) needs its own atomic flag in `HttpAsioService`'s wrapper, which works only if callers stop through the wrapper rather than the `TcpServer`. | `reactor.h:939`, `:285`, `:307` | Put it in http-limits' brief; or add an atomic `accepting()` on `TcpServer` if http would rather read it there. |
| F3 | no | defect | FreeBSD's `rlim_t` is signed (`__int64_t`). A `wanted` above `INT64_MAX`, such as `SIZE_MAX` for "as many as allowed", casts negative, passes `wanted <= rlim_cur`, and returns without raising. Linux and macOS raise to the hard limit for the same call. Xapiand's consumer passes a finite count, so nothing hits it today. | `reactor.h:729-730` | Clamp first: `const rlim_t want = wanted > std::numeric_limits<rlim_t>::max() ? std::numeric_limits<rlim_t>::max() : rlim_t(wanted);`; test that `raise_open_files(SIZE_MAX)` returns the hard limit (Linux CI). |
| F4 | no | gap | Xapiand's loop (`src/main.cc:398-436`) sets `rlim_max` as well, so as root it can rise past the hard limit, and `--max-files` can lower the limit. `raise_open_files` caps at the current hard limit for root too and never lowers. A2 asks for exactly this, but server-limits narrows root behavior and the lowering option if it calls only this function. | `reactor.h:731`; Xapiand `src/main.cc:405-430` | Owner's call for server-limits: keep (A2 as written), keep Xapiand's own path for root and `--max-files`, or add a privileged raise of `rlim_max`. |
| F5 | no | gap | Test [R] reaches neither the macOS `kern.maxfilesperproc` cap nor the refused-`setrlimit` return; its asks stay under the hard limit. | `test/test.cc` [R] | On macOS with an infinite hard limit, `raise_open_files(maxfilesperproc + 1)` returns `maxfilesperproc`. |
| F6 | no | nit | `notAfter == now` is accepted (`left < 0` refuses), where Xapiand refused at that second. RFC 5280 makes `notAfter` inclusive, so the new behavior is right. `seconds_to_expiry` returns 0 both for an unreadable date and for "expires now", so a caller's 14-day warning fires for both, as in Xapiand. | `reactor_tls.h:129`, `:136-141` | Keep; say "0 when unreadable" in the comment if callers must tell the cases apart. |
| F7 | no | nit | "Each is idempotent and `stop()` skips the parts already run": `stop()` still re-runs `force_connections()` (`force_all` has no once-guard) when its grace runs out after a caller already forced. A second `shutdown(2)` is harmless. | `reactor.h:289`, `:862-865` | Reword, or guard `force_all` like `abort_all`. |
| F8 | no | nit | At this commit README.md, AGENTS.md and ARCHITECTURE.md still describe `stop()` as abort first. reactor:78f28ca, a docs-only commit after the reviewed one, updates them; I did not review it. | journal "Open"; `git -C reactor diff 9349464 78f28ca --stat` | Land reactor:78f28ca with the quest after a docs read. |

## Not run

- No probe jobs. Everything above was settled by reading against the passing suite, TSan and
  mutant jobs.
- I did not read the platform kernels' code. The macOS and FreeBSD `setrlimit` behavior in check 5
  comes from their documented semantics; FreeBSD and the macOS cap path have no CI coverage
  (F3, F5).
- The arm64 cost of the seq_cst leave (the journal leaves it open) is outside this lens.
- CI at reactor:9349464 was still running when I wrote this; landing waits on it.
- reactor:78f28ca (docs only) is unreviewed.
