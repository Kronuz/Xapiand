# Loop scheduling: what keeps the HTTP loops off their CPUs under the T4/T5 mix

> Status: DESIGN, for one concurrency design review (quest loop-scheduling, A1). Measured on the merge of
> x7-loop-fix-c into the quest base; nothing here is built. Prototypes stay on the unpushed scratch branch
> `scratch/loop-scheduling`.

Question. With fix (c) in place, x7-gate's T4 (every HTTP loop's p99 lag within 1,024 us) and T5 (loaded light
p99 within 2x unloaded p99 + 1 ms) still miss: lag p99 3.07-6.14 ms, light p99 48.3/52.8 ms
(`docs/perf/x7-loop-stalls.md`, "At fix (c)"). E4 there found the loops' long handlers at 0.45 CPU per wall
second, so they are off their CPUs mid-handler. Is that more runnable threads than CPUs, loops not favoured over
workers, or long turns? And which one mechanism, on Linux and macOS alike, makes T4 and T5 pass?

Answer, in short. The loops are descheduled, by each other: under this mix every loop is runnable about 92% of the
time, so 16 loops alone fill the 16 logical CPUs. Favouring loops over workers does not help, because workers are
not what the loops wait for. Fewer loops (one per physical core) cut the loops' time off CPU from 32% to 12% of
their runnable time and the lag p99 2.4x, but T4 and T5 still miss: once the loops keep their CPUs, a lag tick or
a light request waits behind the heavy requests' turns on its own loop. No thread-scheduling mechanism alone passes
T4 and T5 under this mix; Q1 asks the lead how to re-scope.

## Method: a thread census

An experiment build (scratch commit Xapiand:c4f11fd14 on macOS, Xapiand:560b48426 on Linux; RelWithDebInfo,
frame pointers, LTO off) adds a sampler thread (`harness/experiments/ls_census.cc` on the scratch branch). Every
2-3 ms on macOS (Mach `thread_info`, `THREAD_EXTENDED_INFO`) and every 5-6 ms on Linux (`/proc/self/task/*/stat`)
it reads each thread's run state, CPU time and scheduled priority, plus the load generator's (the parent process).
Asio's custom handler tracking marks a loop as "in a handler". Linux adds `schedstat`'s exact run-queue wait per
thread. Per thread, "ready" (runnable but off CPU) is the wall time sampled in the runnable state minus the CPU
time: the descheduled share, which a wall-clock profile (`sample(1)`) cannot separate from running. Knobs per
run: `LS_LOOP_POLICY`, `LS_WORKER_POLICY` (QoS class on macOS, nice on Linux, applied by each thread to itself;
the census reads the priority back), `LS_WORKERS` (WorkPool size), and `--processors` for the reactor count.

Load: x7-gate's `harness/stall_load.py`, 20,000 `accounts`, light open loop at 200/s, heavy closed loop with the
heavy clients fixed across arms (64 on macOS, 32 on Linux: 4x today's reactors), plus the RESTORE stream; an
unloaded then a loaded phase of 20 s. The census covers the loaded phase. The sampler costs 0.15-0.20 cores on
macOS and 0.05-0.07 on Linux.

Hosts: macOS 15.8.1, Intel Core i9-9980HK, 8 cores, 16 logical CPUs (E1, job 261112-145754-4dcc08, strict).
Linux: strata's local QEMU VM (HVF), Ubuntu 24.04, kernel 6.8, 8 vCPUs, one thread per core (E3, job
261112-141805-74aaf9, exclusive on the host). One run per arm, so a single arm's light p99 is noisy: the base and
census-off arms on macOS, the same code, gave 43.8 and 67.4 ms. Lag percentiles are coarser (bucket edges) and
more stable.

## What the census shows

### macOS, 16 logical CPUs, 16 loops (E1)

| Class | Threads | Cores used | Runnable share | Ready (off CPU) share | Blocked | Uninterruptible |
|---|---:|---:|---:|---:|---:|---:|
| HTTP loop | 16 | 10.07 | 0.916 | 0.290 | 0.056 | 0.027 |
| WorkPool worker | 16 | 1.69 | 0.114 | 0.013 | 0.886 | 0.002 |
| WAL writer | 8 | 0.09 | 0.031 | 0.013 | 0.968 | 0 |
| Generator (Python) | 1 | 0.34 | 0.435 | 0.094 | 0.565 | 0 |

Shares are of the window, per thread, the median thread of each class. Runnable threads (Xapiand plus generator)
average 17.3 on 16 CPUs (p50 17, p99 29), more than 16 for 59% of the window; the loops are 14.7 of them, the
workers 1.95 (bursts: 8 or more runnable for 10% of the window, the RESTORE's prepare and apply chores). The host is
90% busy. A loop is in a handler 75% of the window, runnable for 95% of that, and gets 0.63 CPU per wall second:
32% of its runnable time it waits for a CPU. Every class runs at the same scheduled priority (31).

| Arm (E1) | Loop ready / runnable | Loop lag p99, median (max) | p99.9 median | Light p99 | Heavy searches |
|---|---:|---:|---:|---:|---:|
| Base, 16 loops, 16 workers | 31.6% | 6.14 (7.17) ms | 12.29 ms | 43.8 ms | 22,999 |
| Census off | | 6.14 worst | | 67.4 ms | 21,057 |
| Loops at QoS user-interactive | 36.9% | 5.12 (6.14) ms | 13.31 ms | 71.1 ms | 21,101 |
| Workers at QoS utility | 30.8% | 5.12 (7.17) ms | 10.24 ms | 43.5 ms | 23,028 |
| 8 workers | 32.7% | 6.14 (7.17) ms | 14.34 ms | 63.0 ms | 22,924 |
| 8 loops (`--processors=8`) | 12.0% | 2.56 (3.58) ms | 4.10 ms | 38.9 ms | 18,623 |

- Workers at QoS utility took effect (their priority read back 20, against 31): the loops' ready share did not
  move (0.286 against 0.290 of the window).
- Loops at QoS user-interactive did not take effect: `pthread_set_qos_class_self_np` returned 0 on all 16 loops,
  but their scheduled priority read back 31, not 47. This arm says nothing about favouring loops on macOS. A
  process started from a shell may have its QoS clamped; not investigated (see Q3).
- Fewer workers changed nothing: the workers are 1.95 runnable threads on average.
- Fewer loops did: 8 loops use 6.71 cores, are runnable 95% of the window (saturated) and ready 12% of their
  runnable time; lag p99 2.4x lower and p99.9 3.0x lower; the light p99 within the noise of the base; heavy
  searches 0.81x (one run each; E2 below repeats it ABBA).
- No arm passes: every loop's lag p99 is over 1,024 us in every arm, and light p99 is 4-8x the T5 bound
  (2 x 3.6-3.8 ms + 1 ms, about 8.5 ms) in every arm.

### Linux VM, 8 vCPUs, 8 loops (E3)

| Arm (E3) | Loop ready / runnable | Loop run-queue wait per on-CPU s | Loop lag p99, median (max) | Light p99 |
|---|---:|---:|---:|---:|
| Base, 8 loops, 8 workers | 12.0% | 0.161 s | 5.63 (7.17) ms | 19.8 ms |
| Census off | | | 8.19 worst | 20.0 ms |
| Workers nice 5 (read back: priority 25) | 8.9% | 0.117 s | 4.61 (6.14) ms | 20.9 ms |
| Loops nice -5 (read back: priority 15) | 7.4% | 0.108 s | 5.12 (6.14) ms | 23.1 ms |
| 4 workers | 8.1% | 0.129 s | 4.10 (8.19) ms | 16.7 ms |
| 4 loops (`--processors=4`) | 0.5% | 0.003 s | 2.56 (4.10) ms | 24.7 ms |

- The base is oversubscribed the same way: 8 loops use 6.68 cores and are runnable 99% of the window; runnable
  threads average 10.4 on 8 CPUs, more than 8 for 92% of the window; the host is 97% busy. Workers wait 1.54 s on
  the run queue per second on a CPU, the loops 0.16 s.
- Loops nice -5 needs `RLIMIT_NICE` (raised with `sudo prlimit` for the arm) or `CAP_SYS_NICE`; it took effect
  (priority 15). It took CPU from the generator (ready 34% to 57% of the window), which shows in the light
  latency the generator measures.
- On this VM even idle loops show lag p99 2.05 ms (the guest's timer wake-up under HVF), so T4's 1,024 us edge
  cannot be judged here. The 4-loop arm, at 0.5% ready, sits at 2.56 ms, one bucket above that floor.
- Priorities and the worker count move the loops' ready share by a third at most and the lag p99 by one or two
  buckets; the loop count removes the ready time.

## The cause

1. Oversubscription, by the loops themselves. Phase 7c sizes both pools at one per CPU
   (`docs/design/phase7c.md` D2 C5, `:155`; `src/opts.cc:380` sets `num_http_servers` from `--processors`, default
   the hardware concurrency, `:188` and `:282`; the WorkPool takes 0, the hardware concurrency,
   `src/manager.cc:285`, `src/database/pool.cc:623-626`; threadpool `work_pool.hh:726`). The risk was named at
   `docs/design/phase7c.md:554`. Under the closed-loop heavy mix every loop stays runnable (92-99% of the window),
   so the loops alone fill every CPU and timeslice each other; the workers add bursts on top. On the Mac's
   16 logical CPUs (8 cores) the loops lose 32% of their runnable time, on the 8-vCPU VM 12%.
2. Not priority. The workers are not what the loops wait for: lowering them changed the loops' ready share by
   nothing on macOS and by a quarter on Linux, with no light-latency gain. Raising the loops over everything
   takes CPU from the client on the same host (Linux) and was not applied on macOS.
3. Long turns, by queueing on the loop. With the loops on their CPUs (Linux, 4 loops: ready 0.5%), T4 still
   misses by the VM's floor and T5 worsens: 4 saturated loops carry 32 heavy clients, and a light request or a
   lag tick waits behind the heavy requests' turns (each up to the 200 us slice plus one hit or one 256-ordinal
   window) on its own loop. On macOS at 8 loops, light p99 stays at 39 ms while lag p99 is 2.56 ms: a light
   request spends several turns' queues, not one.

E4's 0.45 CPU per wall second in long handlers is cause 1: a handler of 1 ms of CPU stretched by a loop sitting
ready. The census also shows loops uninterruptible 2.7% of the window on macOS (page-ins under the RESTORE's
writes), not examined further.

## Decisions

**D1. The loop count is the one scheduling lever; priorities and the worker count are not.** Alternatives
measured and lost: workers at a lower priority (macOS QoS utility, Linux nice 5): loops' ready share unchanged on
macOS, -26% on Linux, light p99 unchanged; loops at a higher priority: not unprivileged on Linux (`RLIMIT_NICE`
or `CAP_SYS_NICE`), starves a client on the same host, and not applied on macOS; fewer workers: no change on
macOS, inside the noise on Linux. A priority mechanism would also be three platform mechanisms (QoS, nice, and
FreeBSD's `rtprio_thread`, untested) for no measured gain. Xapiand had per-thread nice once and removed it
(Xapiand:63082725a, Xapiand:d6fc01c5c); threadpool keeps an unused `ThreadPolicyType` seam (`thread.hh:35-38`,
ignored by `setup_thread`, `thread.cc:99`), which this design leaves unused.

**D2. One loop per physical core, as the mechanism if Q1 keeps it.** The reactor count defaults to the physical
core count (SMT siblings are left to the workers, the WAL threads and the OS) instead of the logical CPU count.
`--processors` still caps it. Measured: on the Mac (16 logical, 8 cores) it cuts the loops' ready share 2.6x and
the lag p99 2.4x at 0.81x heavy searches (E1; E2 repeats it as an ABBA). On a host without SMT (the VM) it is
today's count, and the VM's base (12% ready) stays as it is. Alternatives: half the logical CPUs everywhere (the
VM's 4-loop arm: ready 0.5%, lag p99 2.56 ms, light p99 24.7 against 19.8 ms, so T5 gets worse there); CPUs
minus the workers' observed mean (not a stable quantity: it follows the write load).

Detection, one function in `src/opts.cc`: macOS `sysctlbyname("hw.physicalcpu")`; Linux the distinct
`(physical_package_id, core_id)` pairs in `/sys/devices/system/cpu/cpu*/topology/` over the CPUs in
`sched_getaffinity`; FreeBSD `kern.smp.cores`; on failure the logical count (today). Readers per index
(`src/manager.cc:272-276`) and the WAL and fsync thread counts (`src/opts.cc:374-376`) keep following
`opts.processors`, the logical count; only `num_http_servers` (`src/opts.cc:380`) changes.

**D3. T4 and T5 need a second mechanism, inside the loop, outside this quest's scope.** The census shows that
once the loops keep their CPUs, what is left is queueing on the loop behind heavy turns. Candidates for that
quest, not measured here: a light request's resumptions and the loop's timers ahead of a heavy request's
resumptions (today D2 puts every resumption after a chore or turn in the ordinary FIFO, `docs/design/phase7c.md`
`:134-147`, and "no attained-service counter exists or is added", a contract this would change); or admission of
at most N heavy requests per loop at once. Q1.

## Contracts

- Options: none added. `--processors` keeps its meaning (a cap on the CPUs used); the default reactor count
  changes from the logical to the physical core count (D2), only if Q1 and Q2 keep D2. `GET /:info`'s
  `num_http_servers` (`src/server/search_views.cc:781`) shows the count in use.
- No API, format or wire change. Thread names and the T1 census list are unchanged (fewer `http` loops on SMT
  hosts).

## Checks

- A2 (T4, T5): an exclusive job of at most 10 minutes per x7-gate's procedure on the merge of x7-loop-fix-c and
  this quest, ABBA against the merge without it: `stall_load.py --phase both --rate 200 --duration 20` with the
  heavy clients fixed at 64, and the same-index run. Under the census, D2 alone is expected to miss both (E1, E2).
- A3 (T12): `loadtest.py` and `profile_bulk.sh` RESTORE docs/s, ABBA with pre-7c, one exclusive job; E2's loadtest
  arms show D2's cost first.
- A4: a unit test of the core-count function against fixtures of `/sys/devices/system/cpu` topology (two threads
  per core, one per core, an affinity mask that splits a core, unreadable topology falls back to the logical
  count), and `--processors` capping it; mutant: the function returns the logical count.

## Open questions

- **Q1 (lead).** T4 and T5 cannot pass by thread scheduling under this mix (cause 3). Options: (a) re-scope this
  quest to D2 alone, with T4 and T5 reported, and open a quest for in-loop priority (D3), which changes
  phase7c.md D2's "no attained-service counter" contract; (b) widen this quest to D3 now; (c) hold D2 too, since
  it costs heavy throughput and passes nothing by itself. Recommendation: (a), measured first by E2.
- **Q2 (owner).** D2 changes a default: 2.4x lower loop lag p99 on SMT hosts at about 0.8x heavy search
  throughput (E1; E2's ABBA decides the magnitude). Keep today's one loop per logical CPU, or one per core?
- **Q3.** Loops at QoS user-interactive were not applied on macOS (priority stayed 31). If the owner wants the
  priority route tested properly, find why (a process-level QoS clamp under a shell or `launchd`) first; D1
  stands on the Linux numbers and the macOS worker arm meanwhile.

## First steps after the review

1. `src/opts.cc`: the physical core count and `num_http_servers` from it (D2), with the unit test (A4).
2. A2 and A3 jobs on the merge with x7-loop-fix-c.
3. The in-loop quest (D3), if Q1 chooses it.

## Files

`src/opts.cc` (the core count and the default), a unit test under `tests/` for the topology parse,
`docs/design/phase7c.md` (D2 C5: "one reactor per core"), `docs/perf/x7-loop-stalls.md` (the census result, a
pointer here).

## Reproduce

Scratch branch `scratch/loop-scheduling`: Xapiand:c4f11fd14 (census build, macOS; `-DLS_CENSUS_HEADER=` adds
the header to Xapiand's own targets only), Xapiand:560b48426 (Linux), `harness/experiments/ls_report.py`
(the tables), `harness/experiments/vm_census.sh` (E3's arms), `harness/experiments/e2_ab.sh` (E2). Build jobs
261112-145752-4c47b7 (macOS) and 261112-141803-72c158 (VM).
