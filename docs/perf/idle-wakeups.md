# Idle-node wakeups

**Date:** 2026-11-13  
**Status:** Process baseline and privileged per-thread wait/dispatch trace complete; worker-waker correlation remains inferred.

An idle node has four recurring timer sources: eight WAL writer threads each wait at most 100 ms, seventeen reactor lag probes rearm every 100 ms, the catalog sweep rearms every second, and the fsync scheduler caps an empty wait at 30 seconds. These are timer-expiration opportunities, not measured package wakeups. macOS coalesces timers, so their rates must not be added and labeled as hardware wakeups.

The executable is built from Xapiand:c3f270ecc, RelWithDebInfo with LTO off, on x86_64 macOS 15.8.1. Dependencies include reactor:ab69cf2, logger:f61d8df, and scheduler:d57e8fc. The node uses default worker counts, port 18899 bound to loopback, and one index with one document (`{"value":1,"_id":"doc","_version":1}`). No product source changes are included.

## Attribution

| Thread | Source | Period | Timer opportunities/s | Proposed fix |
|---|---|---|---:|---|
| Xapiand:WL00 through WL07 | `src/database/wal.cc:776` timed queue dequeue | 100 ms | 10 each, 80 total, inferred from source | Wait indefinitely while empty and notify every writer on shutdown; verify shutdown/drain invariants in the owning quest. |
| Main control reactor | `src/manager.cc:555`, reactor `reactor_lag.h:105` and `:116` | 100 ms idle | 10, inferred from source | Arm lag diagnostics only while activity exists; define how the next activity reactivates measurement. |
| 16 unnamed HTTP reactor threads | `src/manager.cc:956`, reactor `reactor.h:665`, `reactor_lag.h:105` | 100 ms idle | 160 total, inferred from source | Apply the same lag-probe policy to every loop; preserve stall detection semantics explicitly. |
| Main control reactor housekeeping | `src/manager.h:139`, `src/manager.cc:826`, `src/control_plane.h:143` | 60 s | 1/60, inferred from source | Keep scheduled housekeeping when required; do not replace a deadline with a shorter polling interval. |
| Xapiand:W14/W15 | threadpool `work_pool.hh:260,1068`; Strata `include/strata/core/catalog.hh:375,2457` | Catalog timer: 1 s | 300 / 302 park returns in 300 s | The park is indefinite. The one-second catalog sweep posts work; avoid that sweep while no maintenance deadline or pressure needs it. The timer-to-worker correlation is inferred from source and matching rate, not a captured wake-address link. Other workers record zero park returns. |
| Xapiand:FS-- | scheduler `scheduler.h:513,598` | At most 30 s even when empty | 10 wait returns / 300 s, measured | Use an indefinite wait when there is no queued deadline; notify on arrival and shutdown. FS00 through FS07 record no wait returns in the trace. |
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

## Privileged trace

Native macOS administrator authentication succeeds. A second exclusive run at the same executable records 300 seconds of DTrace events. DTrace exits 0 with no diagnostics. Each wait-return stack identifies its thread family; a separate sample maps TIDs to names. The trace also records scheduler `on-cpu` events separately.

**These are wait-call returns, not counts of physical CPU wakeups.** `kevent` includes nonblocking polls and may return without a context switch. The roughly 20 calls/s per HTTP reactor must not be labeled 20 timer wakeups/s. The lag probe has a 100 ms idle timer; scheduler dispatch counts are about 11/s per reactor. Likewise, a dispatch can follow preemption rather than a sleeping-thread wake. Package-idle and interrupt counts in the earlier process-level table retain their own meaning.

| Thread / TID | Wait-return event | Returns in 300 s | Returns/s | On-CPU probe events |
|---|---|---:|---:|---:|
| Main control / 225889133 | `kevent` | 6,279 | 20.93000 | 3,963 |
| Xapiand:W14 / 225889165 | `ulock_wait2` | 300 | 1.00000 | 322 |
| Xapiand:W15 / 225889166 | `ulock_wait2` | 302 | 1.00667 | 330 |
| Xapiand:WL00 / 225889167 | `psynch_cvwait` | 2,899 | 9.66333 | 3,199 |
| Xapiand:WL01 / 225889168 | `psynch_cvwait` | 2,898 | 9.66000 | 3,198 |
| Xapiand:WL02 / 225889169 | `psynch_cvwait` | 2,899 | 9.66333 | 3,206 |
| Xapiand:WL03 / 225889170 | `psynch_cvwait` | 2,899 | 9.66333 | 3,216 |
| Xapiand:WL04 / 225889171 | `psynch_cvwait` | 2,898 | 9.66000 | 3,184 |
| Xapiand:WL05 / 225889172 | `psynch_cvwait` | 2,896 | 9.65333 | 3,171 |
| Xapiand:WL06 / 225889173 | `psynch_cvwait` | 2,898 | 9.66000 | 3,190 |
| Xapiand:WL07 / 225889174 | `psynch_cvwait` | 2,898 | 9.66000 | 3,178 |
| Xapiand:FS-- / 225889177 | `psynch_cvwait` | 10 | 0.03333 | 12 |
| Unnamed HTTP reactor / 225889196 | `kevent` | 5,973 | 19.91000 | 3,237 |
| Unnamed HTTP reactor / 225889197 | `kevent` | 5,973 | 19.91000 | 3,340 |
| Unnamed HTTP reactor / 225889198 | `kevent` | 5,973 | 19.91000 | 3,328 |
| Unnamed HTTP reactor / 225889199 | `kevent` | 5,973 | 19.91000 | 3,316 |
| Unnamed HTTP reactor / 225889200 | `kevent` | 5,973 | 19.91000 | 3,291 |
| Unnamed HTTP reactor / 225889201 | `kevent` | 5,973 | 19.91000 | 3,307 |
| Unnamed HTTP reactor / 225889202 | `kevent` | 5,973 | 19.91000 | 3,285 |
| Unnamed HTTP reactor / 225889203 | `kevent` | 5,973 | 19.91000 | 3,295 |
| Unnamed HTTP reactor / 225889204 | `kevent` | 5,973 | 19.91000 | 3,305 |
| Unnamed HTTP reactor / 225889205 | `kevent` | 5,973 | 19.91000 | 3,282 |
| Unnamed HTTP reactor / 225889206 | `kevent` | 5,973 | 19.91000 | 3,309 |
| Unnamed HTTP reactor / 225889207 | `kevent` | 5,973 | 19.91000 | 3,222 |
| Unnamed HTTP reactor / 225889208 | `kevent` | 5,973 | 19.91000 | 3,282 |
| Unnamed HTTP reactor / 225889209 | `kevent` | 5,973 | 19.91000 | 3,301 |
| Unnamed HTTP reactor / 225889210 | `kevent` | 5,973 | 19.91000 | 3,296 |
| Unnamed HTTP reactor / 225889211 | `kevent` | 5,973 | 19.91000 | 3,228 |
| Xapiand:LOG / 225889217 | `psynch_cvwait` | 1 | 0.00333 | 1 |

The other sampled worker/fsync threads have no wait-return or on-CPU events in the aggregation. LOG has one deadline-return event, with no evidence of a periodic empty-logger poll.

The catalog's default `sweep_period_ns` is 1,000,000,000 (`catalog.hh:375`). `arm_period()` posts a pass on that timer and `run_pass()` rearms it (`:2441-2466`). Xapiand supplies the timer through `DatabasePool::make_host` (`src/database/pool.cc:554`). This accounts for a one-second control-loop task and is consistent with W14/W15's 300/302 wake-return events. A follow-up trace of unpark addresses and caller stacks would make the timer-to-worker link direct. The fsync scheduler's explicit `now + 30s` cap (`scheduler.h:513`) accounts for its ten timed returns. These two sources are missed by the initial parked-stack sample alone.

### Trace command

Authenticate before acquiring the exclusive hold, so time spent at the native administrator dialog does not consume the trace deadline. Start the unprivileged node and read its existing document, allow activity-triggered probes to return to idle, then execute as root:

```sh
/usr/sbin/dtrace -Z -q -s .scratch/idle-waits.d -p NODE_PID > .scratch/idle-traced-waits.txt
```

The exact D program is:

```d
syscall::kevent:return,
syscall::kevent64:return,
syscall::kevent_qos:return,
syscall::*wait*:return
/pid == $target/
{
    @waits[tid, probefunc] = count();
    @stacks[tid, ustack(12)] = count();
}
sched:::on-cpu
/pid == $target/
{
    @dispatch[tid] = count();
}
tick-300s
{
    exit(0);
}
END
{
    printf("WAIT RETURNS BY THREAD AND SYSCALL\n");
    printa("%d %s %@d\n", @waits);
    printf("CPU DISPATCHES BY THREAD\n");
    printa("%d %@d\n", @dispatch);
    printf("WAIT RETURN STACKS\n");
    printa(@stacks);
}
```

The local raw artifacts are `.scratch/codex/idle-traced-waits.txt`, `idle-traced-stacks.txt`, and `idle-traced-dtrace.log`. The table preserves every nonzero per-thread aggregation from that run. No product timer, wheel, housekeeping, database, or engine file changes are included.

## Remaining attribution

The periodic families and exact per-thread call/dispatch counts are available. A direct wake-address link from the catalog callback to W14/W15 remains to be captured. Separating blocking and nonblocking `kevent` calls would further explain the reactor's syscall count. No claim equates call returns, timer firings, scheduler dispatches, interrupt wakeups, or package-idle wakeups. No power saving is measured.
