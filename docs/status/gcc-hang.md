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
| Mutant: the destructor does not put the outer trampoline back | survives (test passes); the revert above is the mutant the test kills |

## task repro

`.scratch/gcc-hang-repro/nested_drive.cc` (main checkout): a 30-line thread-owned loop driven from inside a task another such loop runs. g++-13 with task:b406766 and with task HEAD (C3, `6558c2e`): "stuck", rc 1, at -O0 and -O2; clang 18: ok. C2 and C3 do not fix it: `fresh_trampoline` (task.hh) is applied only to await_asio's inline resume, not to a driver outside the library. A public hook (or making `transfer` detect a nested driver) belongs to task.

## Next

1. The lead decides whether task offers a public fresh-trampoline scope; until then host.h reaches into `task::detail`.
2. CI on g++-13 reruns `test_pool` and `test_blocking_guard`.

## Blocked on

Nothing.
