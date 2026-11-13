# loop-scheduling: journal

State: waiting on jobs 261112-154306-4d90d5
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

- 261112-141757-8568f0 failed again at CheckAtomic, and not because of the flag: the reused slot's
  `build-ls/CMakeCache.txt` held the first run's failed `HAVE_CXX_ATOMICS_WITHOUT_LIB` (empty, and no
  "Performing Test" line in the log); my local configure with the same `-fno-omit-frame-pointer` passed.
  Resubmitted with `rm -rf build-ls` first: 261112-144638-49d13f (build), 261112-144641-58278e (E1).

- 261112-144638-49d13f got past configure and failed compiling: macOS's `sys/param.h` (through the Mach and
  libproc headers in the force-included header) defines `MAX`, which broke `wal.h`'s `ENUM_CLASS`. The header
  now declares only the loop mark and hooks; the sampler and platform headers moved to
  `harness/experiments/ls_census.cc` (c4f11fd14). Checked locally: configure rc 0 (CheckAtomic passes with
  `-fno-omit-frame-pointer`), and `ls_census.cc`, `wal.cc`, `pool.cc`, `thread.cc` compile. Resubmitted:
  261112-145752-4c47b7 (build), 261112-145754-4dcc08 (E1).

E3 (261112-141805-74aaf9, Linux VM, 8 vCPUs, 32 heavy clients), loaded phase, base arm: loops are
saturated, not descheduled from idle CPUs: 8 loops use 6.68 cores, runnable 99% of the window, in a handler
98%; ready (runnable, off CPU) 12% of their runnable time; schedstat run-queue wait 0.161 s per on-CPU s
(437 us per slice); workers wait 1.54 s per on-CPU s. Runnable threads mean 10.4 on 8 CPUs (more than 8 for
92% of the window); host 97% busy. The VM's idle loops already show lag p99 2.05 ms (timer wake-up under
HVF), so T4's 1,024 us edge cannot be judged there. Arms (loop lag p99 median, light p99):
base 5.63 ms, 19.8 ms; nocensus 8.19 worst, 20.0 ms; workers nice 5 (applied: pri 25) 4.61, 20.9; loops
nice -5 (applied: pri 15, RLIMIT_NICE raised) 5.12, 23.1 (the generator's ready share 34% to 57%); 4 workers
4.10, 16.7; 4 reactors (heavy 32) 2.56 (loops ready 0.5%, run-queue wait 0.003 s/s), 24.7.

E1 (261112-145754-4dcc08, macOS, 16 logical CPUs, 64 heavy clients), loaded phase, base arm: 16 loops use
10.07 cores, runnable 92% of the window, ready 32% of their runnable time; runnable threads mean 17.3 on 16 CPUs
(loops 14.7, workers 1.95, generator 0.44). Arms (loop lag p99 median, light p99, heavy searches): base 6.14 ms,
43.8 ms, 22,999; nocensus 6.14 worst, 67.4 ms; loops qos:ui (call returned 0 on 16 loops, priority stayed 31:
not applied) 5.12, 71.1; workers qos:ut (applied: priority 20) 5.12, 43.5, loops' ready unchanged; 8 workers 6.14,
63.0; 8 reactors 2.56 ms (ready 12%), 38.9 ms, 18,623 (0.81x).
Draft note written (`docs/design/loop-scheduling.md`); E2 261112-154306-4d90d5 (exclusive, scratch 58d1b9493,
`harness/experiments/e2_ab.sh`): ABBA 16 vs 8 reactors, T4/T5 then T12 loadtest, on the census build with the
census off.

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
