# hunt-traceback-freebsd: journal

State: waiting on jobs 261121-211440-65eb0b 261121-211442-6909c6 261121-211444-6d3f40; CI on traceback:ee5debb
Quest: .campaign/xapiand/quests/hunt-traceback-freebsd.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 pushed (traceback:7ef1ac2 test, traceback:ee5debb fix). Waiting on the macOS jobs and
CI's FreeBSD job.

## Next

1. Read the jobs and CI's freebsd-x86_64 result at traceback:ee5debb; fill Evidence.

## Mechanism

`dump_callstacks()` (traceback.cc:848 at a6690d9) calls a thread idle when
`callstack[1 + skip_call] == snapshot[1 + skip_snap]`, with skip 5/4 for slot 0 (calibrated to
Xapiand's ev-loop call chain). Both arrays are locals filled only up to the recorded frame counts
(traceback.cc:829-840). On FreeBSD 14.3 the main thread's recorded stack is about four frames:
the handler trims at the interrupted frame's return address (pthread_kill), then the caller,
main (test_dumper inlined at -O2), __libc_start1; libexecinfo's backtrace() drops the frame below
it (_start). Index 5 and index 4 are both past the end, so the comparison reads two
uninitialized slots; they matched on FreeBSD, the main thread counted idle, and only its name
was missing (total:2 still counts it). Linux has deeper start-up frames (__libc_start_call_main,
__libc_start_main, _start), so index 4/5 were real, different addresses. macOS (3 frames) read
garbage that happened to differ. Every other path to the failure (a deferred self-signal, a
genuine match) also needs the out-of-range read: aligned stacks from the same caller never have
frame 5 of one equal frame 4 of the other.

Hypotheses read and set aside: pthread_t identity (pthread_t is a pointer on FreeBSD, `==` is
sound, and total:2 shows both slots live); register_thread keying (main registers with
pthread_self(), same as deregister/handler lookup).

## Fix

A stack too short to hold the compared frame is not comparable and prints as active
(traceback.cc `comparable`); the active line's label no longer re-reads those slots (it was
always "(active)" in that branch). Output format unchanged. Test: hooks_test poisons the stack
between callstacks_snapshot() and dump_callstacks() so the unfixed out-of-range reads match on
every platform, and prints the dump when the name check fails.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-211440-65eb0b (build-test at ee5debb) |
| A2 | 261121-211442-6909c6 (fails-without at 7ef1ac2, macOS), 261121-211444-6d3f40 (pass at ee5debb); CI FreeBSD before: runs 44570136588, 44958843249 |
| A3 | CI at ee5debb |

## Result

Pending the jobs.

## Rulings I made

- The fix treats an uncomparable stack as active rather than idle: the dump cannot prove a
  thread idle without the frame, and the dumping thread is the case that hits it.
- The skip 5/4 calibration for slot 0 stays (Xapiand's call chain); changing it is out of scope.
