# reactor-server-composition: journal

State: waiting on jobs 261119-135828-bf6eab 261119-135829-521317 261119-135837-88bc47 261119-135840-1dad3b 261119-135842-6ffa4b 261119-135845-fa3dd3 261119-135847-85e4ab 261119-135850-7391bb 261119-135903-7920d2
Quest: .campaign/xapiand/quests/reactor-server-composition.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on the suite, TSan, six mutant jobs and the strict A/B at reactor:3626f70.

## Next

1. Read the jobs; paste `campaign brief`'s table; set State to ready for review.

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
| A1 | <from `campaign brief`> |

## Result

<pending jobs>

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
