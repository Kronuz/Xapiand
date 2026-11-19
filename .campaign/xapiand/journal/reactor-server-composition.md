# reactor-server-composition: journal

State: ready for review
Quest: .campaign/xapiand/quests/reactor-server-composition.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: every criterion has evidence at reactor:9349464 (code at reactor:3626f70; later commit is docs only).

## Next

1. Correctness review (lead launches it); CI needs the branch pushed (the lead held pushes).

## Blocked on

Nothing.

## Evidence

Head reactor:3626f70 (work reactor:409c4ce, dates fix reactor:3626f70). Mutants are side
branches `mutant/rsc-*` in the reactor repository, never landed:

| Job | What |
|---|---|
| 261119-135828-bf6eab | suite at head (A1-A5) |
| 261119-135829-521317 | TSan at head (A3: [R] races, [H], [W], [K]) |
| 261119-135837-88bc47 | A1 mutant reactor:f93d814 (no key mode, dates or strict; expiry 0) must fail reactor_tls_test |
| 261119-135840-1dad3b | A2 mutant reactor:1ced66d (never raises) must fail reactor_test |
| 261119-135842-6ffa4b | A3 mutant reactor:8890521 (stop_accepting a no-op) must fail reactor_session_test [W] |
| 261119-135845-fa3dd3 | A3 mutant reactor:b4b5e70 (last leave never wakes) must fail [W] |
| 261119-135847-85e4ab | A3 mutant reactor:778fd6a (wake without the mutex: lost wakeup) must fail [H] |
| 261119-135850-7391bb | A4 mutant reactor:5ebf21e (legacy stop inserts wait_requests) must fail [L] |
| 261119-135903-7920d2 | A5 strict A/B, test/gate_ab.sh, a=bf874dc b=head, a a b b x3 |

| Criterion | Evidence |
|---|---|
| A1 | 261119-135828-bf6eab, 261119-135829-521317; fails without: 261119-135837-88bc47 (every server_context check fails) |
| A2 | 261119-135828-bf6eab, 261119-135829-521317; fails without: 261119-135840-1dad3b ([R] x3) |
| A3 | 261119-135828-bf6eab, 261119-135829-521317 (TSan clean); fails without: 261119-135842-6ffa4b ([W] refusal), 261119-135845-fa3dd3 ([W] hangs), 261119-135847-85e4ab ([H] 2.3 s x5, [R] hangs) |
| A4 | 261119-135828-bf6eab, 261119-135829-521317; fails without: 261119-135850-7391bb ([L]: stop 2005 ms, not cancelled) |
| A5 | 261119-135828-bf6eab, 261119-135829-521317; strict A/B 261119-135903-7920d2: +1.7% req/s, -0.1 ns/pair at the median, inside the 11% / 2.9 ns A/A spread (docs/perf/gate-leave.md) |

## Result

reactor:409c4ce, reactor:3626f70 and reactor:9349464 on quest/xapiand/reactor-server-composition:

- `TcpServer::stop_accepting()`, `wait_requests(token) -> bool`, `cancel_requests()`,
  `force_connections()`; legacy `stop()` composes them with no wait. `wait_requests` sleeps on a
  drain the gate's last leave wakes once armed (seq_cst flag against the gate's fetch_sub, wake
  under the waiter's mutex): no poll, no lost wakeup (TSan clean; mutants caught).
- `reactor::raise_open_files(wanted)`; `reactor::tls::server_context`, `seconds_to_expiry`,
  `expiry_warning`.
- Measured: the leave change is within A/A noise (docs/perf/gate-leave.md); one server
  descriptor per plain connection (test.cc [P]), against Xapiand's FDS_PER_CLIENT 2.
- Docs: docs/API.md, docs/mechanisms/stop-and-accept.md, docs/perf/gate-leave.md, CHANGELOG.md.
- Open: README.md, AGENTS.md, ARCHITECTURE.md (outside the file set) still describe stop() as abort
  first; arm64 cost of the seq_cst leave unmeasured; the note (cpp-libraries) folds D2 when this lands.

## Rulings I made

- Legacy `stop()` runs D2's order: `stop_accepting` before `cancel_requests` (it was abort first,
  then the accept stop). Requests in flight are still cancelled at once ([L]).
- `wait_requests` arms the drain for the server's remaining life (no disarm); after it, each
  last leave takes the drain mutex. Zero wins over a requested token.
- The A3 hook sits inside the locked check, after reading the gates and before the sleep
  (`REACTOR_TEST_HOOK(wait_requests_checked)`): the last leave lands after the flag's store and
  before the wait, where a wake without the mutex is lost (mutant a3-unlocked).
- A refused `try_enter` returns its slot through `leave()`: it can be the one that brings a gate to
  zero after concurrent leaves, and must wake the drain then.
- `raise_open_files` raises only to `wanted` (capped by the hard limit and, on macOS,
  `kern.maxfilesperproc`), not to the hard limit; a refused `setrlimit` returns the limit in force.
- `server_context` wraps `configure`'s errors (asio throws `system_error` for a mismatched pair at
  load) in `std::invalid_argument` naming both files; messages keep Xapiand's harness phrases
  ("has expired", "is not valid yet", "make it mode 0600"). The 14-day warning is the caller's
  log line over `seconds_to_expiry` and `expiry_warning`: reactor has no logger.
- Dates use `ASN1_TIME_diff`; `X509_cmp_current_time` is deprecated in OpenSSL 4.
- Left out 7 measured: a served plain connection holds one server descriptor (test.cc [P]: 64
  connections add 128 descriptors with their clients), against Xapiand's `FDS_PER_CLIENT` 2.
- New tests live in existing test files (`CMakeLists.txt` is outside the file set); the A/B bench
  is `test/gate_bench.cc` and `test/gate_ab.sh`, built by the script, not CMake.
- README.md, AGENTS.md and ARCHITECTURE.md are outside the file set: they still describe
  `stop()` as abort first; docs/API.md and docs/mechanisms/stop-and-accept.md carry the phases.
