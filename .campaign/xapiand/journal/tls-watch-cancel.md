# tls-watch-cancel: journal

State: ready for review
Quest: .campaign/xapiand/quests/tls-watch-cancel.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: the fix and its test are on the quest branch at http:e4ad5b9; a reviewer is on it.

## Next

1. Read CI run 44936222272 (linux-tsan) and the build-test check 261121-103400-803d9e when they finish.
2. Fix review findings by id, each with a test that fails without it.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | Job 261121-112242-c328ec at http:e4ad5b9 passes `tls_watch`. Without the fix, at http:445f1b2 (test only, the earlier job 102716-4e6b27): `tls_watch` fails, and only on its check "a cancel between the wake and the stream's continuation ends the read with operation_aborted" (the earlier checks pass, the wait queued before the watch's read woke; exit 0 means it failed on that check). Job 261121-102751-26bc30 at http:e4ad5b9: `tls_watch`, `tls_asio`, `limits`, `readahead` all pass (exit 0, 93.7 s). The window is reached without a sleep: a `wait_read` queued before the watch's read runs first in the pass that completes the read with 3 bytes of a 23-byte record. Rerun at the landed code: check build-test 261121-103400-803d9e runs all ctest. |
| A2 | `ci:linux-tsan` at http:e4ad5b9 (CI run 44936222272; every other platform green too). |
| A3 | check build-test 261121-103400-803d9e at http:e4ad5b9 (queued when written). |
| A4 | [artifact] `docs/perf/watch-peek.md` step 2 names the TLS cancellation slot (http:e4ad5b9); the same commit corrects the three range ends from aa_ratio.py over perf-watch-peek's A/B runs (1.37-1.97, 0.82-1.12, 0.90-1.03). |

## Result

`cancel_read()` on TLS emits a terminal `cancellation_signal` (`watch_cancel_`) bound to the watch's
`async_read_some` with `bind_cancellation_slot`; asio's io_op checks it after `put_input`, so a
cancel between the lowest-layer read's wake and the stream's continuation ends the read with
`operation_aborted` instead of re-arming. Plain TCP keeps its socket cancel and the `cancelled_` mark.
For TLS the socket cancel is no longer issued (the slot reaches the inner read through io_op).
Docs: `docs/perf/watch-peek.md` (step 2; C2 ranges). Leaves open: TSan is CI's, no mutant added
to `test/watch_mutants.sh`. Not a timing change, so no measurement.

## Rulings I made

- TLS `cancel_read()` emits the slot only and does not also call `lowest_layer().cancel()`: the slot
  already reaches the inner read, and a socket cancel would also cancel an unrelated pending write.
  Not proven by a test on a concurrent write; `limits` and `tls_asio` pass.
- The test is a new file (`tls_watch_test.cc`) beside `tls_asio_test`, since `readahead_test` does
  not link `reactor::tls`.
