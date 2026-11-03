> Status: CURRENT (2026-11-03) the developer's ledger for the g++-13 hang of `test_pool` and `test_blocking_guard`; local branch `fix/gcc-hang`, not pushed.

# gcc-hang (build): a nested blocking wait deadlocks on g++

Branch `fix/gcc-hang` in `Xapiand-wt/gcc-hang`, on master `Xapiand:ebfb8fdfe`. Linux runs on the shared AWS lease (g++-13.3, Ubuntu 24.04, kernel 7.0).

## State

Root cause found and fixed in Xapiand; the same defect is in task's trampoline contract (repro below, for the Detent lead). `test_pool`, `test_blocking_guard` and `test_blocking_scheduler` pass on g++-13 Release, TSan and ASan.

## Root cause

task without symmetric transfer (every non-clang compiler) resumes a transfer through a per-thread trampoline: a transfer made while its loop is running is only queued, and the running loop resumes it. `engine::sync_wait` called from a task that another `sync_wait` is driving (the pool's `run_detached` falls back to `sync_wait` when no control loop runs; `nested_wait` in `test_blocking_guard`) starts the inner task through that same thread-local trampoline. The inner transfer is queued behind the outer loop, which is blocked inside the inner wait: nothing ever resumes the inner task.

Stack at the hang (gdb `thread 1`, `test_pool`, all workers parked):

```
BlockingScheduler::drive           host.h:326   (inner, done=false, ready_ empty, cv wait)
sync_wait<void>                    sync.h:91
DatabasePool::run_detached         pool.cc:541
housekeeping_pass                  pool.cc:764
task::detail::transfer             task.hh:381/389   (outer trampoline loop is resuming it)
run_then / BlockingScheduler::drive (outer)  host.h:307
sync_wait / run_detached / housekeeping    pool.cc:786 / test_pool.cc:1208
```

`test_blocking_guard`'s forked child shows the same shape (`nested_wait` -> inner `drive` under `transfer` under outer `drive`).

## Fix

`BlockingScheduler::drive` holds a `NestedTrampoline` (host.h): when a trampoline loop is already running on the thread it swaps in a fresh one for the duration of the wait and puts the outer one back; with symmetric transfer (clang) it is empty. It uses `task::detail::local_trampoline()` and `task::detail::trampoline`, present at the pinned task:b406766.

## Proof

| Check | Result |
|---|---|
| `test_blocking_scheduler` `test_a_blocking_wait_nests` (nests the wait one and two deep, awaits a child after the nested wait, watchdog aborts after 30 s), fix reverted, g++-13 Release | "a nested blocking wait did not return", abort (rc 134) |
| same test, fix in | pass; `test_pool` 197 checks and `test_blocking_guard` 14 checks pass (both hung before) |
| TSan, g++-13 (`vm.mmap_rnd_bits=28`) | `test_blocking_scheduler`, `test_blocking_guard` clean; `test_pool` only the known `test_pool.cc:183` exception_ptr report (Blocked on in the linux-gate-fixes ledger), no other report, `test_released_views_are_reclaimed_by_the_housekeeping` ok |
| ASan, g++-13 | all three clean |
| Mutant: the destructor does not put the outer trampoline back (the test nests twice and compares the running loop's `active`, `head` and queue size before and after each nested wait) | killed: `CHECK(kept)` fails, 1 of 5 checks; with the restore, 5 of 5 pass |

## task repro

`.scratch/gcc-hang-repro/nested_drive.cc` (main checkout): a 30-line thread-owned loop driven from inside a task another such loop runs. g++-13 with task:b406766 and with task HEAD (C3, `6558c2e`): "stuck", rc 1, at -O0 and -O2; clang 18: ok. C2 and C3 do not fix it: `fresh_trampoline` (task.hh) is applied only to await_asio's inline resume, not to a driver outside the library. A public hook (or making `transfer` detect a nested driver) belongs to task.

## Dropping the workaround

The fix is a fallback while task has none. Once task makes a nested driver safe (a public fresh-trampoline scope, or `transfer` detecting one) and the pin is bumped: 1. delete `NestedTrampoline` and its use in `BlockingScheduler::drive` (host.h), or replace it with the task scope; 2. keep `test_a_blocking_wait_nests` as the regression test but drop its `keeps_the_outer_loop` part if it reads `task::detail`; 3. rebuild with g++-13 and rerun `test_blocking_scheduler`, `test_blocking_guard` and `test_pool`; the watchdog turns a regression into an abort, not a hang. Revert check: with the pin bumped and `NestedTrampoline` removed, the nesting test must still pass.

## Next

1. The lead decides whether task offers a public fresh-trampoline scope; until then host.h reaches into `task::detail`.
2. CI on g++-13 reruns `test_pool` and `test_blocking_guard`.

## Blocked on

1. **`test_node` segfaults on g++-13 Release, with and without this fix** (full Release ctest with the fix: 33 of 34, `test_node` SEGFAULT; same on a plain `-DCMAKE_BUILD_TYPE=Release` build, and on master `Xapiand:ebfb8fdfe`'s `host.h`). It was hidden behind the two hangs. The `xapiand` child the test starts dies in the traceback library's SIGUSR2 collector when the shutdown dumps callstacks: gdb shows `SIGSEGV` in `traceback::collect_callstack_sig_handler` at `traceback.cc:712` (`frame->return_address`, `frame` read from the interrupted thread's context) on an idle `Xapiand:FS00` worker parked in `futex_wait`. Not examined further; it belongs to the traceback library (pinned `7f3d7ba`) or to how Xapiand arms it, and CI will report it as soon as the hangs are gone.
