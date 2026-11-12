# http-cancel-race: journal

State: waiting on jobs 261112-035947-754791 261112-035948-060f09 261112-035951-01fc5f
Quest: .campaign/xapiand/quests/http-cancel-race.md (the lead's; never edit it)

## Doing

Waiting on the three jobs below and on GitHub CI at http:473bbc3.

## Next

1. When the jobs and CI are green, set State to "ready for review" with the brief's table.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261112-035947-754791 (suite, incl. readahead_test's new window case); 261112-035948-060f09 `--fails-without`: `sh test/watch_mutants.sh build-mutants cancel-race`, exits 0 only when readahead_test fails on "[U] a cancel between the wake and its handler ends the read with operation_aborted"; 261112-035951-01fc5f every watch mutant killed |
| A2 | build-test 261112-035947-754791; GitHub CI at http:473bbc3 |
| A3 | [artifact] `docs/perf/watch-peek.md` (Timing: A/A printed as after/control; rotation stated) |

Local runs before submitting (not evidence): readahead_test PASS (0 failures); the cancel-race
mutant killed on its own check.

## Result

http:473bbc3 (http_asio.h, test/readahead_test.cc, test/watch_mutants.sh, docs/perf/watch-peek.md):

1. N1: the held read's `Cancel` sets `cancelled_` on the input before cancelling the socket;
   `readable()` that finds `EAGAIN` with a waiter whose wait was cancelled completes with
   `operation_aborted` instead of re-arming. `wait_for_read()` clears the mark, and it counts only
   while a waiter is set.
2. N5: `resume_waiter()` calls `task::detail::asio_park::resume(waiter_scheduler_, h)`, task's one
   rule with its inline-resume bound (it handles a null scheduler by resuming in place).
3. Test: readahead_test queues a test `async_wait` on the same socket before the watch's wait; both
   complete in one reactor pass (FIFO per descriptor on kqueue and epoll), and the test's handler,
   which runs first, takes the byte and requests the token. The watch's handler then reads `EAGAIN`.
4. N2: the A/A column is recomputed from job 261109-150350-f39012's runs as a/aa per round. The six
   verdicts stand; the 8-connection `httpc` paragraph now reads 1.06 above the A/A top of 1.05.

## Rulings I made

- N4's premise does not hold: `bench/ab.sh run` already rotates the starting series by round
  (http:d6f8468, an ancestor of the A/B commit), and f39012's run.log shows the order a aa base e1,
  aa base e1 a, ... So bench/watch_ab.sh is unchanged; the note states the rotation (A3).
- The mark is a plain bool, set by `Cancel` on the requesting thread. The tokens reaching this read
  are requested on the loop (deadline timers; reactor's `SessionControl::abort()` runs on the loop),
  as the existing `socket.cancel()` in the same callback already assumes.
- The flag only matters when the wake finds nothing; a wake that finds data completes with the data,
  as an `async_read_some` that completed before the cancel would.
- `task::detail::asio_park::resume` is a detail name of task; the quest's contract names it
  ("task_asio.hh resume helper"), so http calls it rather than keeping a copy.
