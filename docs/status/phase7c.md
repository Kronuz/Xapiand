> Status: CURRENT (2026-10-29) the Phase 7c design ledger.

# phase7c (design)

Branch `design/phase7c` from Xapiand master `3b0dc433f`, worktree `Xapiand-wt/phase7c-design` (local
only, never pushed). Design: `docs/design/phase7c.md`. Review: `docs/reviews/phase7c-design.md`.
Scope: strata `docs/PLAN.md` Phase 7, 7c (`:651-674`, owner, 2026-10-29).

## State

designing (revision 2, ready for the architect's re-check). Revision 2 closes R1 to R7 with
decisions, folds C1 to C4 into S1's done criteria and C5 to C10 into the decisions and rows they
name, splits the Xapian deletion into X1 with its own done criteria, and records the owner's
answers to Q1 to Q6. No build, probe or measurement was run for revision 2.

## Done

- Revision 1 of the design and this ledger: `Xapiand:0a8d235b8`, `Xapiand:fdcc91862`.
- Architect's preliminary review, APPROVE WITH CONDITIONS for S1 only: `Xapiand:e30a844ff`.
- Revision 2 (see the log for how each finding closed): `Xapiand:2d8c2daab`.

## Next

- Lead: the architect's re-check of R1 to R7 against revision 2 (the review's probes are kept in
  `.scratch/review-7c/`).
- Startable now, needing no open question: S1 (with C1 to C4), X1 (the Xapian deletion), and L1 to
  L6 on their own branches.
- X2 onward wait for the re-check and for L1 to L6.

## Blocked on

Nothing. Q1 to Q6 are answered (strata `docs/status/xapiand-on-strata.md`, "Owner answers on the 7c
design"). Detent's move off http's offload path is coordinated separately; Detent keeps its http
pin until then, and nothing in 7c waits on it.

## Log

### 2026-10-29

- Revision 1 read: strata PLAN, ARCHITECTURE "Concurrency and async", `xapiand-on-strata.md`,
  `write-buffer.md`; Xapiand AGENTS, ARCHITECTURE, `offload-bridge.md`, `engine-offload-architect.md`,
  `phase7a.md`, `src/engine/`, the pool, handler, shard, schemas, resolver, Lua, manager and HTTP
  service code; the reactor, http, task, completion, journal, residency, threadpool, scheduler,
  logger and atomic-shared-ptr libraries; Detent's use of http's offload.
- Revision 2 read: the review (R1 to R7, C1 to C10, probes); the owner's answers; task `aa447e7`
  (`budget`, `running_on`, `cancel_source`, `asio_scheduler`, `as_awaitable`); reactor `0c04d0d`
  (`OffloadGate`, `track`, `abort_all`, `wait_sessions`); http `6a07f99` and `dc3be77`
  (`ConnectionInput::monitor`, the streamed and offload paths, `BodySink`, `stream`); threadpool
  `feature/scheduler` (`WorkItem`, class lanes, reserve, `merge_cap`); strata `d93bf7d` (include
  unchanged at `5b0e6ed`: `apply`, `check_out`, `check_in`, `writer_scope`, `run_chores`, `drain`,
  `reader_handle`, `pool_executor`, `hazard.hh`, `owner.hh`); storage `671eaac` (`write`, chunked
  `read`); Xapiand `processor.cc`, `shard.cc` storage paths, `search_service.cc`, `opts.cc`, and the
  sites F8 added.
- How revision 2 closes each blocking item:
  - R1: each Lua call runs `func(env)` with its own `_ENV` table over a shared base; the hook yields
    only when yieldable; a non-yieldable bound, an instruction cap, a live-byte cap and a matcher
    step-budget patch end runaways (D10, T10).
  - R2: a slice is a continuous run, stamped by `running_on` at every resume; the policy is called
    two-level; T5's light mix has write-then-GET pairs (D2).
  - R3: `OffloadGate` stays as `AdmissionGate`, counting in-flight requests per reactor beside the
    demoted cap, each with its own 503; T8 runs light and heavy overloads (D2).
  - R4: every lease operation is a turn, timers included; the registry has `closing` and a retry
    rule; one turn per task (schema writes first), and `lock_until` deadlines answer 503 (D5).
  - R5: Xapiand never calls `check_out`, so no chores run inline; strata's `defer_io` returns
    `flush_owed` or `drain_owed` instead of doing I/O; faults stay Phase 6's (D6).
  - R6: `task::await_asio` bridges Asio to tasks; the disconnect monitor requests a cancel source
    for reads; the handler await is not cancellable and frames end only by completion; shutdown
    order is D16, tested in T7 (D3, D16).
  - R7: D9 is "blob I/O in chores over `DataStorage`"; `storage_commit` is named in D7; uploads above
    the body cap stream into their own volume through a new storage streamed write (L6); X1 keeps
    `DataStorage`.
- Not opened: `src/xapian/`, `src/sortable_serialise.{cc,h}`, `src/server/remote_protocol_views.cc`.
  Their deletion in X1 is by `git rm`. `sortable_serialise` users were found by grepping other files.

---

# phase7c S1 (build): the gate tooling and the pre-7c baseline

> Status: CURRENT (2026-10-29) the S1 developer's ledger. Branch `phase7c/s1-tooling` in `Xapiand-wt/s1-tooling` (local only, from master `05c8dc8ff`),
> `phase7c/s1-lag-probe` in `reactor-wt/s1-lag-probe` and `http-wt/s1-lag-probe` (pushed), `trace/hop` in `http-wt/trace-hop` (pushed, never merged).

## S1 state

Done, ready for review. Every K has evidence below. Nothing is blocked. The baseline is the developer's and preliminary (K7): the lead
re-measures on `main`.

## S1 done criteria, ticked

- [x] **K1 reactor `LagProbe`** (`reactor:3041327`, `reactor:ef26c30`; `reactor_lag.h`, `test/lag_test.cc`). Tests: a 6 ms busy handler lands a wake in a
  bucket of at least 4,096 us (max lateness 5,593 us), zero loop-thread allocations from the first wake on, 10 wakes a second after 1 s idle (20 in
  2 s) against about 1,000 busy (255 in 300 ms), `stop()`, a probe per `TcpServer` reactor, the bucket edges (1,023 us is under the 1,024 us edge).
  - *Fails with the fix reverted* (mutants of `reactor_lag.h`, each run through `.scratch/run_variant.sh`): no re-arm fails 5 checks; lateness not
    recorded fails the 6 ms check; always the 100 ms interval fails 3; always the 1 ms interval fails 2; dropping the slot allocator fails the zero-allocation
    check (with `ASIO_DISABLE_SMALL_BLOCK_RECYCLING`, which the test sets, and under ASan).
  - *Sanitizers* (Homebrew LLVM, separate builds): ASan+UBSan clean, TSan clean (the allocation count is skipped under TSan, whose runtime owns `operator new`).
  - *CI* on `reactor:ef26c30`: all 8 jobs green (linux-asan-ubsan, linux-tsan, linux-x86_64-gcc and -clang, linux-arm64-gcc, macos-arm64 and -x86_64, freebsd-x86_64).
    The first push failed only linux-tsan, on the `operator new` link collision, fixed in `reactor:ef26c30`.
  - *Cost (C4)*: idle 10 wakes a second a loop; busy about 1,000. A started probe allocates nothing. On kqueue every re-arm writes the wake pipe and costs one extra loop
    iteration (seen in `sample`), which is why `XAPIAND_LAG_PROBE=0` exists. An idle node's CPU with and without the probes is within noise (0.49 and 0.50 s against 0.42
    and 0.79 s over 30 s). A percentile reads as its bucket's upper edge: "1 ms" is the 1,024 us edge.
- [x] **K2 probes in Xapiand** (`Xapiand:56c20326e`, pins at `reactor:ef26c30`, `http:e5b9cd9`). The http loops run the probe through `HttpAsioService::enable_lag_probe()`
  (`http:e3d5236`, `http:e5b9cd9`: `lag_asio_test`, whose mutant without the per-request `note_activity()` fails; CI green on both http branches), the control loop
  from `run_loop()`, and `:metrics` has `xapiand_reactor_lag_seconds` (histogram), `_sum`, `_count` and `xapiand_reactor_lag_max_seconds` per loop (`http0`...,
  `control`). Docs E2E on strata: **649 of 649 assertions, 0 failing** (377 requests, newman 6.2.3, `harness/e2e_capture.sh` with `e2e_assert.py`). The design's 566 of
  566 is 7a's older count; the docs have grown since.
- [x] **K3 `harness/stall_load.py`** (C3). The RESTORE writes `restore-target` (`--restore-into accounts` is the same-index run); light latency is measured from each request's
  scheduled send time and the issued and target rates are printed and asserted within 5%; `--saturate` doubles from `--saturate-start` until p99 passes 10x the unloaded p99 (the lower
  of the first two steps), then bisects 4 times; `--overload light|heavy N` and `--suite`; every phase writes light p50/p99/p99.9, per-loop lag deltas, 503 and other errors, in-flight and
  demoted peaks (null until 7c adds the gauges), segments and merge backlog (counted from the data directory: the node reports neither), RSS and the phase times; `--lua-loop` and
  `--index-size` are accepted and recorded as not run. Dry runs on 20,000 documents give non-empty results (`--dry-run` fails on an empty one): plain, `--saturate`, both overloads,
  `--restore-into accounts`, `--suite`. `harness/test_gate_tools.py` (14 tests, no node): the mutant that measures from the send time fails
  `test_latency_runs_from_the_scheduled_time`; a generator that cannot keep up misses the 5%; and `pct` had a float bug (`99.9` of 1,000 values read 1,000) that the percentile test caught.
- [x] **K4 `harness/thread_census.sh`**: names and counts by prefix (Linux `/proc/PID/task/*/comm`; macOS `ps -M` for the total and a one second `sample` for the names); `--allow`
  fails on any other prefix. Its dry run lists today's pools: 131 threads (see the baseline), `ps -M` agrees.
- [x] **K5 `harness/async_check.sh`** (C1): rules for `sync_wait`, waits and condition variables, mutexes, sleeps and yields, file syscalls and streams, pool checkouts. **0 unclassified hits
  on this tree**: 316 hits in `src/` (96 allowed, 207 owed to a decision, 13 compiled out) plus the strata `published_position` yield (inventory J: `kind_of`, `getStopWordsEntry`,
  `mime_type`, the census and `published_position` are all hits). `--strict` exits 1 on an unclassified hit (a `std::lock_guard`, a `::open` or a `cv.wait` added to a tracked file failed it,
  and a row's count caps its hits); it never opens `src/xapian`, `src/sortable_serialise.*`, `src/engine/xapian`, or the remote and replication protocol files, so it runs unchanged on
  `phase7c/x1-delete-xapian` (0 unclassified there, 7 rows reported as shrunk). `harness/async_allowlist.tsv` rows are `file, rule, anchor, class, count, note`. Owed rows name their
  decision; `--strict --owed` is the gate once X6 lands. The strata rows need `--extern strata=<strata checkout>`.
- [x] **K6 `harness/profile_light.sh`** (C2) and `http` `trace/hop` (`http:10c5074`, CI green): CPU per request from `sample` frames by group (hop, scheduler, engine, http), wall from the
  stamps at the post, the start of the run, the end and the resume. The pre-7c dry run and the baseline show a non-zero hop share in both: 214 of 8,587 busy samples (2.5%) and a 25.2 us
  median hop wall, 6.6% of the offloaded wall. `--compare PRE POST` is T11's ratio. `XAPIAND_LAG_PROBE=0` keeps the probe out of the profile.
- [x] **K7 the commands the lead runs** for T5, T11 and T12 on `main` before X2 lands, under `quiet exclusive`:

  ```sh
  cmake -S . -B build-a -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer && cmake --build build-a --target xapiand
  cmake -S . -B build-b -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -DHTTP_TRACE_HOP" \
        -DFETCHCONTENT_SOURCE_DIR_HTTP_LIB=<http checkout at trace/hop> && cmake --build build-b --target xapiand
  quiet exclusive NAME 90 "pre-7c baseline" -- harness/baseline_7c.sh build-a/bin/xapiand build-b/bin/xapiand OUTDIR
  python3 harness/baseline_report.py OUTDIR
  ```

  `baseline_7c.sh` runs `stall_load.py --suite --census` (T4, T5, T8), `profile_light.sh` (T11), `profile_bulk.sh` and `loadtest.py` (T12). Five interleaved trials with a bootstrap
  interval are the lead's: this runs each row once.

## Baseline (K7's preliminary numbers; `docs/perf/phase7c-baseline.md`)

Light query p99 at 210 requests a second (half the saturating 425): 70.6 ms unloaded; under the heavy load about half the light requests do not finish in 30 s (T5 not met before 7c).
The HTTP loops' lag p99 is at most 896 us under that load; the control loop's is 2,560 us even idle. Dispatch per light request: 18.7 us CPU, 25.2 us wall. RESTORE 6,076 docs/s,
16-client queries 1,277 QPS.

## Findings for the lead

1. **T4 and the control loop.** Its 100 ms idle probe reads about 1.5 ms p50 late on macOS (timer coalescing), so a 1 ms bound cannot hold for it at that interval. Probe it at 1 ms always,
   or have T4 read it only while busy: the lead's or the architect's call.
2. **L3 must keep `reactor->lag().note_activity()` per parsed request** (one line in `serve_connection_body`) when it rewrites that loop, or the probes drop to 100 ms under load.
3. **The probe's kqueue cost** (one wake-pipe write and one extra iteration per tick) is the price of an Asio timer on macOS and FreeBSD; epoll uses a timerfd. Not measured on Linux.
4. The light mix saturates a 1,000,000-document `accounts` at about 425 requests a second before 7c, mostly the 10% pairs' refreshes (a pair's `GET` waits for a segment write).

## S1 Done (commits)

- reactor: `reactor:3041327` (LagProbe), `reactor:ef26c30` (TSan link fix); CI green on `ef26c30`. Branch `phase7c/s1-lag-probe`, pushed, not merged.
- http: `http:e3d5236`, `http:e5b9cd9` (`phase7c/s1-lag-probe`, CI green), `http:10c5074` (`trace/hop`, CI green). Pushed, not merged.
- Xapiand `phase7c/s1-tooling` (not pushed): `Xapiand:56c20326e` (probes, `:metrics`), `Xapiand:e01965cb5` (tools), `Xapiand:feabe624c`, `Xapiand:3bdb315b6`, `Xapiand:142b2ca77`
  (baseline run, drain bound, merge settling, changelog), and the commit that adds this section (tests, pins at the CI-green tips, the baseline file).

## S1 Next

- Lead: land `reactor` and `http` `phase7c/s1-lag-probe` (CI green), then repin Xapiand to main; delete `trace/hop` after the T11 post-7c run, or keep it for that run.
- Lead: the T4/T5/T8/T11/T12 baseline on `main` under `quiet exclusive` (K7), and the control-loop decision above.
- L3: keep the per-request `note_activity()`. X6: switch `async_check.sh` to `--strict --owed` in CI.

## S1 Blocked on

Nothing.
