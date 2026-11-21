# tls-watch-cancel: A cancel reaches a held TLS watch read between its wake and its continuation

Risk: high (concurrency). The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status tls-watch-cancel`.

## Outcome

In the http library, `cancel_read()` ends a held TLS watch read with `operation_aborted` even when
the cancel lands after the lowest-layer read completed with part of a record. Before,
`cancel_read()` only cancelled the lowest layer; asio's ssl io_op checks only its own cancellation
state, so it re-armed another read and the cancel was lost (http-cancel-race review C1). The watch
read now carries a `cancellation_signal` bound with `bind_cancellation_slot`, as
`task::await_asio` does, and `cancel_read()` emits it as terminal on TLS. Plain TCP keeps its
socket cancel and the `cancelled_` mark. docs/perf/watch-peek.md states the TLS slot in step 2,
and its three range ends are corrected from the recorded runs (review C2).

## Acceptance criteria

- A1 A cancel between the wake and the stream's continuation ends the TLS watch read with
  `operation_aborted`, with no sleep race: `test/tls_watch_test.cc` (ctest `tls_watch`), failing at
  http:445f1b2 (explorer job 261121-102716-4e6b27) and passing at the fix with `tls_asio`, `limits`
  and `readahead` (261121-102751-26bc30), rerun at the landed code
- A2 No data race in the watch read and its cancel: the CI job linux-tsan at the landed code
- A3 The library's tests pass at the landed code: the `build-test` check
- A4 [artifact] `docs/perf/watch-peek.md` names the TLS cancellation slot in step 2

## Contracts

None: `cancel_read()` keeps its signature and its guarantee; only the TLS path changes.

## Decisions

- Lead, 2026-11-21: a terminal cancellation slot on the TLS read, not a second mark checked in the
  continuation, since asio's io_op already checks its slot after `put_input`.

## May land early

None.

## Tasks

None: the change and its test are explore/xapiand/tls-cancel (http:445f1b2, http:e4ad5b9); the
contributor merges it into the quest branch.
