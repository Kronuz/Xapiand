# loop-scheduling: journal

State: waiting on jobs 261112-141757-8568f0 261112-141759-e35a09 261112-141803-72c158 261112-141805-74aaf9
Quest: .campaign/xapiand/quests/loop-scheduling.md (the lead's; never edit it)

## Doing

A1, the design (assignment design): measure the cause, then write `docs/design/loop-scheduling.md`.

Scratch branch `scratch/loop-scheduling` (worktree `.scratch/merge`, never pushed): master's quest base
merged with `quest/xapiand/x7-loop-fix-c` (eaeec5303; `tests/test_loop_yield.cc` takes fix-c's side, tests
not built), plus a census header force-included into every C++ TU (2c41294a4,
`harness/experiments/ls_tracking.hpp`): a sampler thread reads each thread's run state and CPU (Mach
`thread_info` on macOS, `/proc/self/task` and `schedstat` on Linux) and the generator's (parent pid), Asio
custom handler tracking marks a loop "in a handler", and env knobs per arm: `LS_LOOP_POLICY`,
`LS_WORKER_POLICY` (`qos:*` on macOS, `nice:N` on Linux), `LS_WORKERS` (WorkPool size).
`ls_report.py` summarises the loaded phase; `vm_census.sh` (65d7e9ca4) runs the arms in the VM.

Jobs (at 560b48426). Builds at 2c41294a4 and bf0733fd5 failed in prometheus-cpp's CheckAtomic probe:
the header was force-included through CMAKE_CXX_FLAGS, which every dependency's configure probe sees
(the C++17 guard did not help). Now `-DLS_CENSUS_HEADER=` adds `-include` to Xapiand's own targets only
(XAPIAND_OBJ, XAPIAND_PACKAGE_OBJ, xapiand); no separately compiled dependency includes Asio. Configure
checked locally before resubmitting (`cmake -S . -B .scratch/cfg`: rc 0; the flag shows only in those
three targets' flags.make; thread.cc and pool.cc compile).
- 261112-141757-8568f0 light: macOS census build (binary at `~/.scratch/loop-scheduling/560b48426/`).
- 261112-141759-e35a09 strict (= exclusive on macOS): E1, census arms base, nocensus, loops `qos:ui`,
  workers `qos:ut`, 8 workers, 8 reactors (heavy clients pinned at 64).
- 261112-141803-72c158 light: the same build on strata's Linux VM (port 18772, `~/ls-loop-scheduling`).
- 261112-141805-74aaf9 exclusive: E3, the VM's census arms base, nocensus, workers nice 5, loops nice -5
  (RLIMIT_NICE raised with `sudo prlimit`), 4 workers, 4 reactors (heavy 32). Whether nice -5 took
  effect is read back: the census prints each class's priorities (`/proc` field 18, 20 + nice) and the
  policy's applied/failed/errno counts; a failed arm is reported as not applied, not as a null result.

## Next

1. Read E1 and E3; pick the candidate the census supports; E2: an exclusive ABBA of it against base
   (T4/T5, and T12 RESTORE if it touches workers).
2. Write the design note, commit, "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <the design note, once committed> |

## Result

<pending>

## Rulings I made

- The census uses an in-process sampler on an experiment build (scratch branch only), not `sample(1)`:
  run state per thread separates "runnable but off CPU" from "blocked", which wall-clock profiles cannot.
- Linux runs on strata's local QEMU VM (PLAN constraint: `tools/vm/linux.sh`, no AWS), called directly
  under the job's own host hold (`aws-linux.sh run` would take a nested light hold).
