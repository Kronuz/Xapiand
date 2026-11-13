# loop-scheduling: journal

State: waiting on jobs 261112-170146-163a2c 261112-170149-32915c 261112-170152-275ba3 (and builds 261112-170140-f2c47f, 261112-170143-aabac2)
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

Re-scope (D79, 11-12): in-loop priority. Second design contributor took over.

E2 (261112-154306-4d90d5, exclusive, ABBA r16 r8 r8 r16, census off): r8/r16 per pair: loaded light p99 0.596,
0.814 (55.0 -> 37.5 ms median); heavy searches 0.787, 0.782; lag p99 median 4.10/3.58 -> 2.56/2.05 ms (8 of 8
and 16 of 16 loops over 1,024 us in every arm); RESTORE loadtest docs/s 1.018, 0.979; loadtest qps 1.04, 0.969.
Per light class (r16 arm 1, loaded p50/p99 ms): get 1.5/37.4, search (`contact.state`, limit 10) 11.6/84.0,
pair 7.2/300; unloaded search p50 2.0 ms. The light search's p50 rises 6x under load while get's barely moves:
it spends slices and joins the demoted FIFO behind the heavy requests.

Prototype (scratch d58c9721d, 251a8e912): a patched Kronuz/task copy (`harness/experiments/task-lane`, built
through `FETCHCONTENT_SOURCE_DIR_TASK`): `budget` counts slices spent; light for `LS_LANE_K` of them (an
exhausted light slice posts ordinary), then demoted; `scheduler_ref` carries a heavy bit, set by `drain()`'s
scope, so every resumption captured there (chores, events, await_asio) posts to the demoted lane; `handle()`
starts each request light and records kind, slices and server time (`LS_REQ_OUT`, `lane_report.py`).
Checked locally: a 3-task asio_scheduler toy shows light resumes alternating with one heavy turn once the heavy
tasks pass K.
- 261112-163007-a6f351 light: macOS build of 251a8e912.
- 261112-163009-f675a9 exclusive (after it): X1, arms off, K 1, 4, 16, K 4 at 8 reactors (`x1_lane.sh`).
- 261112-163032-c04fc9 light: the same build on the Linux VM.

- 261112-163007-a6f351 done (binary `~/.scratch/loop-scheduling/251a8e912/xapiand`).
- Ready after X1: `x2_ab.sh BIN K` (macOS ABBA off/lane + T12 loadtest) and `vm_lane.sh 251a8e912 K` (VM ABBA,
  copy scratch b16339e4d's `pairs.py` to the VM first), scratch b16339e4d; K chosen from X1's slice histograms.
- Design shape (to write once X1/X2 are in): class by attained service (budget slices spent; light below K,
  heavy and sticky after, via a lane bit in `scheduler_ref` captured by every awaiter); heavy lane = today's
  demoted queue, one turn per io_context pass; light lane = the FIFO; starvation bound = one heavy turn per pass,
  and a light request holds the light lane for at most K slices; library default K = 0 keeps today's policy
  (Detent, prospect, completion and consensus use task); inline resumes (`await_asio`, http `resume_waiter`) only
  for a light ref; no per-loop heavy admission cap. Matches strata `docs/ARCHITECTURE.md:178-180` (LAS).

- X1 261112-163009-f675a9: exit 124 at the 10-minute deadline (lead's msg); invalid as a whole. Only the off arm
  finished. Off arm, loaded phase, per kind (budget slices spent): light_search n 1,803, mean 4.4, p50 4, p99 7,
  99.6% under 8, all under 12; heavy_search n 13,401, mean 23.4, p50 27, none under 8; agg n 8,944, mean 42.5,
  none under 12; get and put 0. So K = 8 slices (1.6 ms of attained CPU) separates them. Server-side light_search
  p50 under 16 ms (log2 bucket), so T5's light latency is server queueing, not the generator.
  The k1 and k4 arms hung in the accounts load: the prototype's heavy resume from a chore called
  `post_demoted` from a WorkPool thread, but the demoted queue is loop-thread-only (`task_asio.hh:47-50`). Fixed on
  scratch 15ef779e6 (off the loop it posts a handler that queues); the design states it as a contract.
- Split per D59: 261112-170140-f2c47f light (macOS build at 3c064618a), 261112-170143-aabac2 light (VM build);
  X2a 261112-170146-163a2c exclusive (T4/T5 ABBA off/lane K 8), X2b 261112-170149-32915c exclusive (T12 loadtest
  ABBA), X3 261112-170152-275ba3 exclusive (VM T4/T5 ABBA K 8). E2-shaped jobs fit in 10 minutes; X1's overrun
  was the hang.

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
