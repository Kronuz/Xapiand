# Idle-node wakeups

**Date:** 2026-11-13  
**Status:** Process baseline complete; privileged per-thread trace queued.

An idle node has two explicit polling sources: eight WAL writer threads each wait at most 100 ms, and seventeen reactor lag probes rearm every 100 ms. These are timer-expiration opportunities, not measured package wakeups. macOS coalesces timers, so their rates must not be added and labeled as hardware wakeups.

The executable is built from Xapiand:c3f270ecc, RelWithDebInfo with LTO off, on x86_64 macOS 15.8.1. Dependencies include reactor:ab69cf2, logger:f61d8df, and scheduler:d57e8fc. The node uses default worker counts, port 18899 bound to loopback, and one index with one document (`{"value":1,"_id":"doc","_version":1}`). No product source changes are included.

## Attribution

| Thread | Source | Period | Timer opportunities/s | Proposed fix |
|---|---|---|---:|---|
| Xapiand:WL00 through WL07 | `src/database/wal.cc:776` timed queue dequeue | 100 ms | 10 each, 80 total, inferred from source | Wait indefinitely while empty and notify every writer on shutdown; verify shutdown/drain invariants in the owning quest. |
| Main control reactor | `src/manager.cc:555`, reactor `reactor_lag.h:105` and `:116` | 100 ms idle | 10, inferred from source | Arm lag diagnostics only while activity exists; define how the next activity reactivates measurement. |
| 16 unnamed HTTP reactor threads | `src/manager.cc:956`, reactor `reactor.h:665`, `reactor_lag.h:105` | 100 ms idle | 160 total, inferred from source | Apply the same lag-probe policy to every loop; preserve stall detection semantics explicitly. |
| Main control reactor housekeeping | `src/manager.h:139`, `src/manager.cc:826`, `src/control_plane.h:143` | 60 s | 1/60, inferred from source | Keep scheduled housekeeping when required; do not replace a deadline with a shorter polling interval. |
| Xapiand:W00 through W15 | threadpool `work_pool.hh:1087` in sampled stacks | Event-driven | Unmeasured | Sampled parked atomic waits give no evidence of a periodic idle poll. |
| Xapiand:FS-- and FS00 through FS07 | scheduler `scheduler.h:598` | Next deadline | Unmeasured | Preserve deadline-driven waits; trace pending deadlines before assigning a cost. |
| Xapiand:LOG | logger `logger.cc:1337` | Next line deadline, indefinite when empty | Unmeasured | Empty logger already blocks indefinitely; do not attribute a 1 ms idle poll from its batching constant. |

The five-second stack sample shows WAL writers in `BlockingConcurrentQueue::wait_dequeue_timed`, workers parked in `WorkPool::park`, and reactor threads in `kevent`. It also catches a `reactor::LagProbe::Tick` rearming a timer. `--log thread-names` names the worker families; HTTP reactor threads remain unnamed in the sample.

## Measurement

The 300-second interval is 2026-11-14 03:16:13 through 03:21:13 UTC. The interval excludes process startup, document creation, the sample command, and shutdown. `proc_pid_rusage` reports process CPU nanoseconds and package-idle/interrupt wakeup counters; these counters do not enumerate thread scheduler wakes.

| Counter | Before | After | Delta | Rate |
|---|---:|---:|---:|---:|
| User CPU ns | 76,666,618 | 837,462,408 | 760,795,790 | 0.2536% of one core |
| System CPU ns | 125,961,699 | 1,333,035,282 | 1,207,073,583 | 0.4024% of one core |
| Package-idle wakeups | 692 | 16,587 | 15,895 | 52.983/s |
| Interrupt wakeups | 2,092 | 76,288 | 74,196 | 247.320/s |

Combined CPU is 1.9679 seconds, **0.6560% of one core**. The 247.3 interrupt wakeups/s are consistent with the 250/s source-level opportunities from 17 lag probes plus eight WAL writers, allowing timer lateness and coalescing. This agreement is an attribution hypothesis, not an exact thread trace. Package-idle wakeups are a separate counter. Converting these counts to watts requires a calibrated power measurement; none is available.

## Reproduction

Build under the shared-host hold, then run a separate exclusive observation:

```sh
quiet check
quiet light codex-idle-build 30 "Build idle-node baseline" -- sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF && cmake --build build -j4'
```

Save this macOS counter reader as `.scratch/idle-usage.c` and compile it with `cc` under `quiet light`:

```c
#include <libproc.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    struct rusage_info_v4 r;
    if (argc != 2 || proc_pid_rusage(atoi(argv[1]), RUSAGE_INFO_V4, (rusage_info_t *)&r)) return 1;
    printf("user_ns=%llu system_ns=%llu package_idle=%llu interrupt=%llu\n",
        r.ri_user_time, r.ri_system_time, r.ri_pkg_idle_wkups, r.ri_interrupt_wkups);
    return 0;
}
```

The executed observation script is below. Its absolute scratch prefix identifies the retained local artifacts; adapt it to another checkout. Run it with `quiet exclusive codex-idle-observe 7 "Five-minute idle observation" -- sh .scratch/idle-observe.sh`.

```sh
#!/bin/sh
set -eu
base=/Users/kronuz/code/Kronuz/.scratch/codex
"$base/idle-wakeups/build/bin/xapiand" -D "$base/idle-data" --port 18899 --bind-address 127.0.0.1 --log thread-names --no-color > "$base/idle-node.log" 2>&1 &
node=$!
trap 'kill -TERM "$node" 2>/dev/null || true; wait "$node" || true' EXIT
for n in $(seq 1 100); do
    if curl -fsS -XPUT 'http://127.0.0.1:18899/idle/doc?commit=true' -H 'Content-Type: application/json' -d '{"value":1}' > "$base/idle-seed.json" 2>/dev/null; then break; fi
    sleep 0.2
done
curl -fsS 'http://127.0.0.1:18899/idle/doc' > "$base/idle-document.json"
/usr/bin/sample "$node" 5 10 -file "$base/idle-stacks.txt" > "$base/idle-sample.log" 2>&1
"$base/idle-usage" "$node" > "$base/idle-before.txt"
date -u > "$base/idle-start.txt"
sleep 300
date -u > "$base/idle-end.txt"
"$base/idle-usage" "$node" > "$base/idle-after.txt"
```

## Remaining evidence

Exact per-thread rates require the queued privileged trace. Unprivileged DTrace fails, and unattended sudo requires authentication. Native macOS administrator authentication now succeeds: root lists `sched:::on-cpu`, `sched:::off-cpu`, `syscall::kevent:return`, `syscall::psynch_cvwait:return`, and `syscall::ulock_wait2:return`, and the `powermetrics -s tasks` sampler runs. This resolves the access limitation. The bounded five-minute follow-up aggregates wait returns by thread, syscall and user stack, with scheduler dispatches reported separately. A scheduler dispatch is not itself proof of a wake from sleep. Until that run completes, no exact per-thread count or power saving is claimed. This macOS release does not accept `powermetrics --show-process-wakeups`; use its supported `tasks` sampler.
