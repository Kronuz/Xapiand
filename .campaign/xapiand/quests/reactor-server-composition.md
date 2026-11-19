# reactor-server-composition: reactor: server composition, stop phases, tls server_context

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status reactor-server-composition`.

## Outcome

The note's (cpp-libraries:04a75c0 docs/server-composition.md) steps 4-5 in reactor, plus D2; high because `wait_requests` carries D71's first signal. Plan: docs/design/lifecycle-adoption.md quest 2 (about 950 lines). The stop is exposed as phases: `stop_accepting()`, `wait_requests(cancel_token) -> bool` (woken by the gate's last leave once stopping, no poll), `cancel_requests()`, `force_connections()`.

## Acceptance criteria

- A1 `tls::server_context` takes Xapiand's checks (`tls.cc:51-76`: key mode, dates, pair) and sets `strict`; `tls::seconds_to_expiry` feeds the 14-day warning: a test each
- A2 `raise_open_files(wanted)` raises the soft limit toward the hard one, returning the limit in force: test
- A3 After `stop_accepting()` a new connection is refused while an in-flight request completes; `wait_requests` returns true at zero, false at its token, and true when a hook puts the last leave between the flag's store and the wait: tests, TSan
- A4 Legacy `stop()` cancels an in-flight request at once: test, mutant inserting `wait_requests`
- A5 The gate's leave stays within A/A noise on the request bench: strict A/B. Suite and CI

## Contracts

Provides: `stop_accepting()`, `wait_requests(cancel_token) -> bool`, `cancel_requests()`, `force_connections()`; legacy `stop()` unchanged in behavior (`stop_accepting`, `cancel_requests`, grace, `force_connections`, grace, join, no wait) for Xapiand before 9, Prism and Detent; `tls::server_context`, `tls::seconds_to_expiry`, `raise_open_files(wanted)`. Consumed by http-limits and server-limits.

## Decisions

- D2 (phases; legacy `stop()` keeps cancelling at once), D1 (library quests filed here, Detent D81), D71 (first signal lets in-flight requests finish).
- Left out 7: `FDS_PER_CLIENT` (2, `main.cc:86`) versus the note's one per connection is measured in this quest.
- No open owner question touches this quest.

## May land early

None.
