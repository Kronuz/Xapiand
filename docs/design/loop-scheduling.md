# Loop scheduling: light work ahead of heavy turns on an HTTP loop

> Status: DESIGN, for one concurrency design review (quest loop-scheduling, A1; owner decision D79). Measured on
> the merge of x7-loop-fix-c into the quest base, with a prototype on the unpushed scratch branch
> `scratch/loop-scheduling`. Nothing here is built on the quest branch.

Question. With fix (c) in place, x7-gate's T4 (every HTTP loop's p99 lag within 1,024 us) and T5 (loaded light
p99 within 2x unloaded p99 + 1 ms) still miss: lag p99 3.07-6.14 ms, light p99 48.3/52.8 ms
(`docs/perf/x7-loop-stalls.md` on x7-loop-fix-c, "At fix (c)"). The thread census ("Thread placement",
below) shows that no thread count or priority passes them, because a light request or a lag tick waits behind
heavy turns on its own loop. Which policy inside the loop puts light work first? How does a request become heavy
without a knob? What bounds a heavy request's wait? What replaces `docs/design/phase7c.md` D2's "ordinary posts in
FIFO order ... No attained-service counter exists or is added"?

Answer, in short. A request is classed by attained service: the slices its budget has spent. It is light for its
first 8 slices (1.6 ms of CPU). Under the T4/T5 mix every light search spends fewer than 12 and 99.6% fewer than
8, while every heavy request spends at least 8 and the median heavy search 27 (X1). From its 8th slice the request
is heavy for the rest of its life. Every resumption of a heavy request goes to the loop's heavy lane (today's
demoted queue), whatever woke it: a slice, a chore, an event, a timer or a body read. The heavy lane keeps today's
rule, one turn per pass of the loop's FIFO. So a light resumption, an I/O completion or a timer waits for at most
one heavy turn per pass, and a heavy request still gets a turn every pass. The mechanism is generic and belongs to
Kronuz/task (D5).

Measured on the prototype, the lane fixes the queueing it targets. A light search's p50 drops 0.45x on macOS
(11.6 to 5.2 ms) and its server-side p50 about 4x, heavy search throughput rises 1.14x, and RESTORE stays flat.
Loaded light p99 is 0.74x on Linux, but T4 and T5 still miss:

- macOS: light p99 is not improved (lane/off 0.97 and 1.37) and stays 7-11x over its bound. Lag p99 stays at
  4.1 ms, 4x the edge.
- Linux: light p99 is 1.5-1.7x over its bound.

What is left on macOS is the tail of requests that spend no slice at all: GET p99 35-92 ms, with a server-side p99
up to 32 ms. It comes from the OS descheduling runnable loops, which no in-loop order can fix.

X4 adds fewer loops to the lane. On macOS, the lane at 8 loops against today's code at 16 gives:

- light p99 0.78x (48.2 to 36.9 ms);
- worst-loop lag p99 down from 5.12 to 2.56-3.07 ms;
- heavy searches 0.957x. The lane offsets most of the 0.785x that 8 loops cost alone (E2).

T4 and T5 still miss on macOS, by 2.5-3x and 4x. On the Linux VM, 4 loops with the lane pass T5 (light p99 9.0-9.8
ms against a bound of about 11.2 ms), but heavy searches drop to 0.636x. D6 takes one loop per physical core with
the lane: 0.957x heavy on SMT hosts and no change on others. Q3 asks the owner about the rest.

## How a loop orders work today

An HTTP loop is one `asio::io_context` thread with one `task::asio_scheduler` (Kronuz/task at the pinned bdafa5f,
`task_asio.hh:58-166`). Two queues feed it:

- The io_context's own FIFO. It holds `post()` (`task_asio.hh:69-76`), every Asio completion (socket readiness,
  the `LagProbe`'s 1 ms timer, writes) and the drainer below. Asio polls for readiness and expired timers once
  per pass of this FIFO.
- The demoted queue (`post_demoted`, `:78-86`). A self-reposting drainer (`drain()`, `:141-158`) runs one demoted
  handle per pass, so everything posted before the drainer's next turn runs first. Only the loop's thread may call
  `post_demoted` (`:47-50`).

A request is a task with a `task::budget` of 200 us (`REQUEST_SLICE`, `src/server/search_views.cc:506`, emplaced
at `:3167`). Its `checkpoint()` posts the task demoted when the current resume has run past the slice
(`task.hh:1153-1181`; each resume starts a fresh slice, `running_on`, `:282-298`). Every other resumption goes to
the FIFO through the `scheduler_ref` captured at suspension:

- chores (`src/database/pool.cc:96`, `:129`);
- events and `SleepFor` (`src/engine/host.h:175-176`);
- `await_asio`, inline when it completes in a top-level handler (`task_asio.hh:201-213`);
- http's body reads (`http_asio.h:565-579`).

That is phase7c.md D2's contract (`docs/design/phase7c.md:146-147`). "Demoted" therefore lasts one resumption: a
heavy search that awaits a refresh or a chore comes back to the FIFO with a fresh slice, beside the light
requests. And a light search that spends one slice is demoted into the same round robin as every heavy request
on its loop.

## What light requests wait for (E2, X1)

E2 (job 261112-154306-4d90d5) splits T5's light p99 by class. In the 16-reactor arm, the loaded p50/p99 in ms
are: GET by id 1.5/37.4, single-term search (`contact.state`, limit 10) 11.6/84.0, write-then-GET 7.2/300.
Unloaded, the search's p50 is 2.0 ms. The GET barely moves while the search's p50 rises 6x: the search spends
slices and waits in the demoted round robin.

X1's off arm (job 261112-163009-f675a9) records per request its budget's spent slices and its server-side time,
in the loaded phase on today's policy. The job ran past its 10-minute deadline, so the job as a whole is invalid;
this arm finished before the cut. The prototype's lane arms hung, which D2 (e) explains and fixes.

| Kind (stall_load) | Requests | Slices spent: mean, p50, p99 | Under 8 slices | Under 12 | Server time p50, p99 |
|---|---:|---|---:|---:|---|
| GET by id | 2,198 | 0, 0, 0 | 100% | 100% | < 0.26, < 16.4 ms |
| PUT (write-then-GET) | 400 | 0, 0, 0 | 100% | 100% | < 2.0, < 32.8 ms |
| Light search, `contact.state`, 10 hits | 1,803 | 4.4, 4, 7 | 99.6% | 100% | < 16.4, < 65.5 ms |
| Wildcard, phrase and text, 100 hits | 13,401 | 23.4, 27, 35 | 0% | 13.7% | < 65.5, < 131 ms |
| Aggregations (`_values`, `_terms`) | 8,944 | 42.5, 40, 64+ | 0% | 0% | < 131, < 262 ms |

Server times are log2 bucket edges. A light search spends 4 slices at the median, so it competes in the demoted
round robin 4 times, each time behind about 4 heavy requests (64 heavy clients on 16 loops). Its server-side p50 of
up to 16 ms covers most of the 11.6 ms that the client sees, so T5's miss is queueing in the server, not in the
generator.

## Decisions

**D1. A request is classed by attained service: the slices its budget spent.** A slice is spent when
`checkpoint()` finds the current resume past it. A request is light while it has spent fewer than K slices and
heavy from the K-th on, for the rest of its life. K is 8 (`REQUEST_LIGHT_SLICES` beside `REQUEST_SLICE`): 1.6 ms
of CPU. X1 shows the gap it sits in: light searches 4 at the median and 7 at p99; heavy searches at least 8 and
27 at the median; aggregations 40. A request whose turns all end on their own (a GET and its chore) spends none.
This is strata's own rule, now made true (`strata/docs/ARCHITECTURE.md:178-180`: "Least-attained service: a
task's first slice runs at high priority, and a task that exhausts its budget is demoted"). Today's demotion is
forgotten at the next resume.

Alternatives, and why they lose:

- A user knob (a `_priority` field or a header). The client rarely knows the cost up front, and the measured
  classes separate without one.
- A class per route (aggregations heavy). A narrow aggregation is cheap, and a 100-hit text search costs 27
  slices.
- Wall time since the request started. It counts the waiting that this design removes, so a light request that
  waited would turn heavy.
- Many levels, pure least-attained service (a priority queue by slices spent). A long request starves while
  shorter ones keep arriving unless aging is added, and the two measured populations already sit on either side
  of one threshold.
- Accumulated CPU instead of whole slices. This is more exact for a request that does many short turns, but each
  such turn already ends within a slice, so it holds the loop no longer than a light turn. Q2.

**D2. Two lanes: light work runs ahead of heavy turns.** The light lane is the io_context FIFO: light resumptions,
every Asio completion, the lag probe's timer and the drainer. The heavy lane is the demoted queue.

- (a) A light request whose slice is spent requeues in the light lane, at the FIFO's tail; it is no longer
  demoted at once. At its K-th spent slice it goes to the heavy lane and its scope is marked heavy.
- (b) The class travels with the `scheduler_ref` that an awaiter captures (`task::current()`). A reference taken
  in a heavy scope posts to the heavy lane, so a heavy request's chores, events, timers, `await_asio` and body
  reads all resume there, with no change to any awaiter (pool.cc, host.h, http's reads). The drainer runs each
  heavy handle in a heavy scope.
- (c) The drainer is unchanged: one heavy turn per FIFO pass.
- (d) An inline resume (`await_asio`'s `resume`, `task_asio.hh:201-213`; http's `resume_waiter`,
  `http_asio.h:572-579`) happens only for a light reference. A heavy one posts to its lane, or its first turn
  would run outside the lane.
- (e) `post_demoted` is callable from any thread. Off the loop's thread it posts a handler that queues the handle
  on the loop. A chore's completion runs on a `WorkPool` thread. X1's lane arms hung in the accounts load because
  the prototype pushed onto the loop-only queue from there. Scratch Xapiand:15ef779e6 fixes it, and X2 runs with
  the fix.
- (f) A request starts light, and the connection carries on light after it. `SearchService::handle` marks its
  scope light on entry and on exit. Otherwise the connection's next read, captured in a heavy scope, would start
  the next request in the heavy lane.

Alternatives, and why they lose:

- Admission of at most N heavy requests per loop. The lane already gives light work the loop after every heavy
  turn, however many heavy requests are queued. A cap would either queue heavy requests in front of the lane (the
  same order, more state) or answer 503, which users see. The demoted cap of 256 (`search_views.cc:3095`)
  already sheds overload.
- Strict priority (drain the light lane before any heavy turn). It starves heavy requests under a light flood
  (T10's 16 clients).
- Several heavy turns per pass. Nothing measured needs them (D3, X2).

**D3. Bounds.** The drainer holds one place in the FIFO, so every pass with a non-empty heavy lane runs exactly
one heavy turn.

- A light resumption or a timer waits for at most one heavy turn per pass: one slice plus one unit of work, about
  0.25-0.35 ms (one hit is about 50 us and 256 ordinals about 0.15 ms, `x7-loop-stalls.md`), plus the light
  handles queued ahead of it. A timer is seen at the pass's poll and runs in the next pass, so the lag is at most
  two heavy turns plus the light work queued ahead.
- A heavy request at position p in the heavy lane runs within p passes. Heavy requests take turns in FIFO order,
  so none is skipped.
- A pass holds at most one turn of each light handle queued when it starts. A light request holds the light lane
  for at most K slices in its whole life before it turns heavy, and the admission gate caps a loop at 1,000
  in-flight requests (phase7c.md D2 R3). So a heavy turn waits at most 1,000 x (slice + unit), about 0.35 s, in
  the worst case of a full gate of light requests, and in practice the light work queued at a pass's start
  (X2 reports heavy throughput).
- Not bounded here: the OS descheduling a loop that is runnable (the census, below). The lane orders work inside
  a loop; it cannot give a loop a CPU.

**D4. What replaces phase7c.md D2's policy bullet** (`docs/design/phase7c.md:146-147`), word for word:

> *The policy is least-attained service in two lanes.* A request's budget counts the slices it has spent; the
> request is light below `REQUEST_LIGHT_SLICES` (8) and heavy from then on. Light resumptions, Asio completions
> and timers run in the loop's FIFO order; every resumption of a heavy request (a slice, a chore, an event, a
> timer, a body read) waits in the heavy lane, which runs one turn per FIFO pass. A request starts light. See
> `docs/design/loop-scheduling.md`.

D2's admission bullet keeps its two caps. `asio_scheduler::demoted()` now counts the heavy handles queued, every
resumption of a heavy request rather than only spent slices, and its cap stays at 256.

**D5. Where it lives: Kronuz/task, with a one-line change in Kronuz/http, then Xapiand.** The mechanism knows
nothing about HTTP, so it goes in the shared library (library quest `task-lanes`). Its default keeps today's
behaviour for the other users of `task::budget` and `asio_scheduler`: Detent, prospect, completion and consensus.

- `task.hh`:
  - `budget(slice, token = {}, light_slices = 0)`; `light_slices` 0 is today's policy (every spent slice
    demoted, nothing sticky).
  - `budget::spent()`.
  - `checkpoint()`: below `light_slices`, `post`; from there on, enter the heavy lane and `post_demoted`.
  - `scheduler_ref` carries the lane (`heavy()`), and `post()` routes a heavy reference to `post_demoted`.
  - `task::enter_light()` marks the current scope light.
  - `inline_scheduler` keeps its order for tests that use it.
- `task_asio.hh`:
  - `drain()` runs a handle that was queued heavy in a heavy scope. Each queue entry records its lane, so a
    budget with `light_slices` 0 demotes exactly as today.
  - `post_demoted` works from any thread (D2 e).
  - `await_asio` resumes inline only for a light reference.
  - Tests in `test/asio_test.cc` (A4).
- Kronuz/http `http_asio.h:578`: `resume_waiter` resumes inline only for a light reference.
- Xapiand:
  - `src/server/search_views.cc`: `REQUEST_LIGHT_SLICES` and the request's budget (`:3167`); the three
    script budgets (`:943`, `:1040`, `:1458`); `enter_light()` at `handle`'s entry and exit.
  - `src/engine/host.h:82` and `src/engine/host_asio.h:52`, `:119-154`: the Host forwards a heavy post, and the
    debug-only `Counted` pump keeps each entry's lane.
  - The Kronuz/task and Kronuz/http pins in `CMakeLists.txt`.
  - `docs/design/phase7c.md` (D2's bullet, D4 above; C5's "one reactor per CPU" becomes "per physical core",
    D6).
  - `src/opts.cc` (D6's core count), with a unit test under `tests/` of the topology parse: two threads per core,
    one per core, an affinity mask that splits a core, and unreadable topology. Its mutant returns the logical
    count.
  - `tests/test_loop_yield.cc` (A4).

**D6. With the lane, one loop per physical core, not per logical CPU** (un-parked by X4, if the owner agrees: Q3).

- Alone, 8 loops on the Mac's 8 cores (16 logical CPUs) cost 0.785x heavy searches (E2).
- With the lane they cost 0.957x (X4: 0.952 and 0.961 per pair). They buy loaded light p99 0.78x and a worst-loop
  lag p99 of 2.56-3.07 ms against 5.12.
- On a host without SMT the count is unchanged.

The reactor count defaults to the physical core count, and `--processors` still caps it. Detection is one function
in `src/opts.cc` that sets `num_http_servers` (`:380`):

- macOS: `sysctlbyname("hw.physicalcpu")`.
- Linux: the distinct `(physical_package_id, core_id)` pairs under `/sys/devices/system/cpu/cpu*/topology/`, over
  the CPUs in `sched_getaffinity`.
- FreeBSD: `kern.smp.cores`.
- On failure: today's logical count.

Readers, WAL and fsync threads keep following the logical count.

Alternative, and why it loses: half the logical CPUs everywhere. On the VM, 4 loops with the lane pass T5, but at
0.636x heavy searches (X4). That cost buys one gate row, so it is not a default.

## Evidence for the lane (X2-X4)

All three runs are exclusive jobs of at most 10 minutes. Each is an ABBA of today's policy (off) against the lane
at K = 8 (lane), on the fixed prototype Xapiand:3c064618a: the macOS build is job 261112-170140-f2c47f, the VM
build 261112-170143-aabac2. Ratios are lane/off per adjacent pair. A cell lists the two arms of a kind.

X2a, macOS, 16 loops, 64 heavy clients (job 261112-170146-163a2c):

| Loaded phase | Off (arms 1, 4) | Lane (arms 2, 3) | Lane/off per pair |
|---|---|---|---|
| Light query p99 (T5 bound 8.1-8.8 ms) | 63.5, 71.1 ms | 61.9, 97.1 ms | 0.974, 1.366: MISS |
| Light search p50, client | 11.2, 12.1 ms | 5.2, 5.2 ms | about 0.45 |
| Light search p50, server (log2 edge) | < 16.4 ms | < 4.1 ms | about 4x lower |
| Light search p99, client | 69.9, 84.3 ms | 68.0, 114.1 ms | not improved |
| GET by id p99, client (0 slices) | 51.7, 35.5 ms | 56.8, 92.2 ms | worse or noise |
| Loop lag p99 median (max), T4 edge 1,024 us | 4.10 (7.17), 5.12 (7.17) ms | 4.10 (4.10), 4.10 (5.12) ms | MISS, 16 of 16 loops over |
| Heavy searches | 22,102, 20,571 | 24,263, 24,162 | 1.098, 1.175 |

X2b, macOS, T12 loadtest (job 261112-170149-32915c). It ran all four arms in full, each restoring 20,000 documents
and running 26,856-33,932 queries:

- RESTORE docs/s: 7,699 and 7,951 against 7,751 and 7,706; lane/off 1.007 and 0.969, so flat.
- Query qps: lane/off 1.242 and 1.013. The first off arm is low, at 3,354.
- Query p99: lane/off 0.555 and 1.018.

X3, Linux VM, 8 loops, 32 heavy clients (job 261112-170152-275ba3):

| Loaded phase | Off (arms 1, 4) | Lane (arms 2, 3) | Lane/off per pair |
|---|---|---|---|
| Light query p99 (T5 bound 10.1-10.2 ms) | 25.6, 19.4 ms | 14.8, 17.4 ms | 0.579, 0.895: MISS by 1.5-1.7x |
| Light search p50 / p99, client | 7.2 / 30.3, 7.3 / 22.2 ms | 5.2 / 17.5, 5.8 / 18.6 ms | lower |
| GET by id p99, client | 10.7, 12.0 ms | 12.8, 13.0 ms | flat |
| Loop lag p99 median (max) | 5.12 (8.19), 5.12 (10.24) ms | 4.10 (6.14), 5.12 (7.17) ms | idle floor 2.05 ms, so T4 cannot be judged here |
| Heavy searches | 25,507, 24,921 | 25,452, 25,439 | 0.998, 1.021 |

The slice counts hold on Linux. Light searches spend 3 slices at the median and 5 at p99; heavy searches 16 at the
median and aggregations 22. K = 8 sits in the gap on both hosts. The lane arms ran the accounts load and
RESTORE to the end, so D2 (e)'s fix holds.

What the numbers say:

1. The lane removes the light search's queueing behind heavy turns. Its client p50 drops 0.45x on macOS and 0.75x
   on Linux. Heavy throughput rises 1.14x on macOS and is flat on Linux. RESTORE is flat (0.99x).
2. T5 still misses on both hosts. On macOS its p99 is set by tails that spend no slice. A GET's server-side p99
   reaches 32 ms in every arm, lane or not. A request of 0 slices waits only in the light lane, behind at most one
   heavy turn per pass (D3), so a 32 ms wait is a loop thread off its CPU: E1's loops sit ready 32% of their
   runnable time. The client is also one Python process on the same host, and its own delay counts in T5.
3. T4 misses on macOS by the same cause. The lag p99 median stays at 4.1 ms, and only the worst loop improves
   (7.2 to 4.1-5.1 ms). On the VM, the 2.05 ms idle floor hides T4.
4. Fewer runnable loops than CPUs, combined with the lane, is what X4 measures next.

X4, the lane at K = 8 with fewer loops, as an ABBA against today's code at today's loop count. There is one
exclusive job of at most 10 minutes per platform, on the same binaries as X2 and X3.

macOS, 16 loops off against 8 loops with the lane, 64 heavy clients (job 261112-175900-eaaa36):

| Loaded phase | Off, 16 loops (arms 1, 4) | Lane, 8 loops (arms 2, 3) | Lane/off per pair |
|---|---|---|---|
| Light query p99 (T5 bound 8.1-9.0 ms) | 55.7, 40.7 ms | 39.5, 34.4 ms | 0.709, 0.844: MISS by 4.1-4.4x |
| Light search p50 / p99, client | 11.0 / 72.1, 10.8 / 44.4 ms | 3.6 / 41.2, 3.5 / 37.5 ms | p50 0.33x |
| GET by id p99, client | 41.0, 23.2 ms | 36.9, 24.4 ms | flat |
| Loop lag p99 median (max), T4 edge 1,024 us | 3.07 (5.12), 3.07 (5.12) ms | 3.07 (3.07), 2.56 (2.56) ms | MISS by 2.5-3x; worst loop 0.5-0.6x |
| Lag p99.9 median | 6.14, 7.17 ms | 4.61, 4.10 ms | about 0.65x |
| Heavy searches | 23,255, 23,407 | 22,132, 22,502 | 0.952, 0.961 |

Linux VM, 8 loops off against 4 loops with the lane, 32 heavy clients (job 261112-175902-15a8eb):

| Loaded phase | Off, 8 loops (arms 1, 4) | Lane, 4 loops (arms 2, 3) | Lane/off per pair |
|---|---|---|---|
| Light query p99 (T5 bound 10.8-11.3 ms) | 17.0, 20.9 ms | 9.8, 9.0 ms | 0.574, 0.434: PASS |
| GET by id p99, client | 11.1, 13.4 ms | 5.3, 6.3 ms | about 0.5x |
| Loop lag p99 median (max) | 4.10 (10.24), 5.63 (10.24) ms | 4.10 (4.10), 4.10 (5.12) ms | idle floor 2.05 ms hides T4 |
| Heavy searches | 26,959, 26,985 | 17,426, 16,901 | 0.646, 0.626 |

What X4 says:

5. On macOS, the lane's heavy gain offsets most of fewer loops' cost: 0.957x heavy with the lane at 8 loops,
   against 0.785x for 8 loops alone. Light p99 improves 0.78x and the worst loop's lag about 2x, but T4 and T5
   still miss: lag p99 2.56-3.07 ms against 1,024 us, light p99 34-40 ms against about 8.5 ms. The GET tail
   (0 slices) stays at 23-41 ms, so loops are still off their CPUs, and 8 loops plus the workers plus the client
   still share 16 logical CPUs.
6. On the VM, half the CPUs as loops passes T5 with the lane, and the light tail halves, GET included. The cost is
   0.636x heavy throughput, because that host has no SMT siblings to give the loops.
7. What would close T4 and T5 on macOS is not in the loop. The candidates are a gate whose load generator does not
   share the server's 16 logical CPUs (today one Python process on the same host measures T5), or fewer loops
   than cores, at the VM's 0.64x-scale heavy cost. Neither has been measured on macOS here: Q3.

## Contracts

- No API, format or wire change. No option: K is a constant beside `REQUEST_SLICE`.
- D6 changes one default: the reactor count is the physical core count, not the logical CPU count, and
  `--processors` still caps it. `GET /:info`'s `num_http_servers` shows the count in use.
- Kronuz/task: `budget`'s third parameter defaults to today's policy. `post_demoted` becomes callable from any
  thread, a widening that keeps every existing caller correct.
- `demoted()` and Xapiand's `scheduler_demoted_peak` metric count heavy handles queued (D4).

## Checks

- A2 (T4, T5): X2a's job (`x2_ab.sh BIN 8 t45`: stall_load ABBA, today's policy against the lane, heavy clients
  64, exclusive, under 10 minutes) on the landed merge, against the merge without this quest.
- A3 (T12): X2b's job (`x2_ab.sh BIN 8 lt`: loadtest ABBA, RESTORE docs/s and query qps) against base, and T12's
  RESTORE against pre-7c by x7-gate's procedure. Heavy search throughput is reported from X2a.
- A4, Kronuz/task `test/asio_test.cc`, each run with its mutant as a `--fails-without` job:
  1. A light task overtakes heavy turns. Two tasks past K and one light task run on one `asio_scheduler`; between
     two consecutive light turns at most one heavy turn runs. Mutant: the light task's spent slice posts demoted
     (today's rule).
  2. A heavy task progresses under a light flood. A light task that reposts itself forever runs beside a heavy
     task; the heavy task runs one turn per pass and finishes within its turn count. Mutant: strict priority, the
     drainer runs only when the FIFO is empty.
  3. Heavy is sticky across a chore. A heavy task awaits a completion that another thread posts through its
     captured reference; it resumes from the heavy lane (`demoted()` counts it), and its next light-lane neighbour
     runs first. Mutant: `post()` ignores the lane.
  4. A cross-thread `post_demoted`. Eight threads each post 10,000 heavy resumes and every one runs once
     (TSan build). Mutant: no hop to the loop.
  5. The default is unchanged. With `light_slices` 0, a sequence of checkpoints, chores and resumes gives today's
     order exactly.
- A4, Xapiand `tests/test_loop_yield.cc`, a node with one reactor:
  1. A single-term search issued while four 100-hit searches run completes before them. Mutant: K = 0.
  2. A light request after a heavy one on the same connection starts light. Mutant: no `enter_light` at
     `handle`'s exit.

## Open questions

- **Q1 (lead).** K is 8, from macOS under one mix (X1) and confirmed on Linux (X3). Keep it as a constant until
  another mix moves the gap? Recommendation: yes. It is attained CPU, so the light search's cost on a host decides
  it, not the host's speed relative to the loop count.
- **Q2 (review).** Count whole spent slices (measured, simplest) or accumulated CPU per resume (exact for many
  short turns)? Recommendation: slices. A short turn already yields within a slice.
- **Q3 (owner): un-park fewer loops, and at what heavy cost?** Recommendation:
  1. Land the lane (D1-D5) and one loop per physical core (D6) together. On SMT hosts this costs 0.957x heavy
     searches against today (X4; the lane alone is 1.14x, X2). It buys light-search p50 0.33x, loaded light p99
     0.78x and the worst loop's lag about 2x lower. On hosts without SMT it is the lane alone: flat heavy, light
     p99 0.74x (X3).
  2. Do not take half the CPUs as loops. It passes T5 on the VM, but at 0.636x heavy throughput.
  3. Record T4 and T5 on macOS as misses: lag p99 2.56-3.07 ms against 1,024 us, light p99 34-40 ms against
     about 8.5 ms. Their remainder is loops off their CPUs, shared with the workers and with the gate's own load
     generator. Before any further mechanism, rerun x7-gate's T4/T5 with the generator off the server's CPUs (on
     the VM against the host, or pinned to its own cores), so the gate measures the server.

  Options the owner may prefer instead:
  - (a) The lane alone, with loops kept at one per logical CPU: heavy 1.14x, macOS light p99 unchanged.
  - (b) Change the gate's T4/T5 bounds for a host that runs its own client.

## First steps after the review

1. Library quest `task-lanes` (Kronuz/task, plus the one line in Kronuz/http): D5's library half and the
   asio_test tests, landed with the default unchanged.
2. This quest: bump both pins, then make D5's Xapiand changes and, if Q3 keeps D6, the core count in `src/opts.cc`
   with its unit test. Then the `test_loop_yield` tests, phase7c.md's D2 bullet and C5, and the A2 and A3 jobs on
   the merge with x7-loop-fix-c.

## Thread placement: the census (E1-E3)

The evidence behind D6. Method: an experiment build (scratch Xapiand:c4f11fd14 on macOS, Xapiand:560b48426
on Linux) adds a sampler thread. Every 2-6 ms it reads each thread's run state, CPU time and priority, plus the
load generator's. Asio's handler tracking marks a loop as "in a handler", and Linux adds `schedstat`. "Ready" is
the time a thread is runnable but off a CPU. The load is x7-gate's `harness/stall_load.py` (20,000 `accounts`,
light open loop at 200/s, heavy closed loop of 64 clients on macOS and 32 on Linux, plus RESTORE), with a 20 s
loaded phase.

Hosts:
- macOS 15.8.1, i9-9980HK, 8 cores, 16 logical CPUs: E1, job 261112-145754-4dcc08.
- Linux: strata's QEMU VM, kernel 6.8, 8 vCPUs: E3, job 261112-141805-74aaf9.

| Arm | Loop ready / runnable | Loop lag p99, median (max) | Light p99 | Heavy searches |
|---|---:|---:|---:|---:|
| E1 base: 16 loops, 16 workers | 31.6% | 6.14 (7.17) ms | 43.8 ms | 22,999 |
| E1 workers at QoS utility (applied) | 30.8% | 5.12 (7.17) ms | 43.5 ms | 23,028 |
| E1 loops at QoS user-interactive (not applied: priority stayed 31) | 36.9% | 5.12 (6.14) ms | 71.1 ms | 21,101 |
| E1 8 workers | 32.7% | 6.14 (7.17) ms | 63.0 ms | 22,924 |
| E1 8 loops | 12.0% | 2.56 (3.58) ms | 38.9 ms | 18,623 |
| E3 base: 8 loops, 8 workers | 12.0% | 5.63 (7.17) ms | 19.8 ms | |
| E3 workers nice 5 / loops nice -5 (both applied) | 8.9% / 7.4% | 4.61 / 5.12 ms | 20.9 / 23.1 ms | |
| E3 4 workers | 8.1% | 4.10 (8.19) ms | 16.7 ms | |
| E3 4 loops | 0.5% | 2.56 (4.10) ms | 24.7 ms | |

What the census shows:

- Under the mix every loop is runnable 92-99% of the window. On macOS, runnable threads average 17.3 on 16 CPUs,
  of which the loops are 14.7 and the workers 1.95.
- The loops are descheduled by each other. Lowering the workers changes the loops' ready share by nothing on macOS
  and by a quarter on Linux. Raising the loops needs `RLIMIT_NICE` on Linux and takes CPU from the client.
- Fewer loops remove the ready time, but light requests still wait behind heavy turns on their own loop, so no arm
  passes.
- E4's 0.45 CPU per wall second in long handlers (`x7-loop-stalls.md`) is this descheduling. On the VM even idle
  loops show a lag p99 of 2.05 ms (timer wake-up under HVF), so T4's edge cannot be judged there.

E2 (job 261112-154306-4d90d5, exclusive, ABBA of 16 against 8 reactors, census off), 8 over 16 per pair:

- Loaded light p99: 0.596 and 0.814 (median 55.0 to 37.5 ms).
- Heavy searches: 0.787 and 0.782.
- Loop lag p99 median: 4.10 and 3.58 ms to 2.56 and 2.05 ms. Every loop is over 1,024 us in every arm.
- T12 loadtest: RESTORE docs/s 1.018 and 0.979; query qps 1.04 and 0.969.

Placement alone passes neither T4 nor T5, and it costs a fifth of heavy throughput.

## Reproduce

Scratch branch `scratch/loop-scheduling` (worktree `.scratch/merge`, never pushed):

- Lane prototype: Xapiand:3c064618a, a patched Kronuz/task in `harness/experiments/task-lane`, built with
  `-DFETCHCONTENT_SOURCE_DIR_TASK=` and `-DLS_CENSUS_HEADER=`; `LS_LANE_K` sets K, unset is today's policy.
- Per-request stats: `LS_REQ_OUT`, read by `harness/experiments/lane_report.py`.
- Experiments: `x1_lane.sh` (X1), `x2_ab.sh` (X2a, X2b, and X4 with `--processors=8`), `vm_lane.sh` (X3, and X4
  with `--processors=4`), at scratch Xapiand:1316730f5.
- Census: `ls_report.py`, `vm_census.sh`, `e2_ab.sh`.
