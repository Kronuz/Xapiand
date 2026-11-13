# http-lane: journal

State: ready for review
Quest: .campaign/xapiand/quests/http-lane.md (the lead's; never edit it)

## Doing

Done; branch pushed at http:09b644d (quest/xapiand/http-lane).

## Next

1. Lead: review, and CI on the pushed branch (build-test is green; the CI check had no run when last read).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-222327-cf718c; fails without: 261112-222330-e221cd |
| A2 | 261112-222327-cf718c |

## Result

http pins task at 18eb516 (CMakeLists.txt). `resume_waiter` already hands its waiter to
`task::detail::asio_park::resume`, which at the new pin posts a heavy reference through its scheduler
(heavy lane) and resumes only a light one inline; so the pin bump is the behavior change and
`resume_waiter` needed no code, only its comment (http_asio.h). A caller that never sets a lane is
light and unchanged.

Test: readahead_test `[H]` (lane_tests): a reader spawned heavy, waiting on the watch's pending read,
resumes with inline_resume_depth 0 and a heavy scope; a light one with depth 1, light. Mutant
`heavy-inline` in test/watch_mutants.sh (waiter resumed with `with_heavy(false)`) fails
"[H] a heavy reader's read is resumed through its scheduler, in the heavy lane": job e221cd exits 0.

## Rulings I made

- No code change in `resume_waiter`: delegating to asio_park::resume is the one mechanism; a second
  heavy test in http would duplicate task's rule. The mutant pins the delegation.
