> Status: CURRENT (2026-10-29) the pre-7c baseline from step S1, taken by the developer on the host below. It is preliminary: the lead re-measures T4, T5, T11 and T12 on `main` under `quiet exclusive` before X2 lands (K7).

# Phase 7c pre-7c baseline

Workstream `phase7c` (S1), maintainer: the S1 developer. Design: `docs/design/phase7c.md` (rows T4, T5, T8, T11, T12 and T1's census).
The numbers are what the commit below did on this host; they are not a timing claim about 7c.

## Conditions

- Commit: Xapiand `phase7c/s1-tooling` at `142b2ca77` (master `05c8dc8ff` plus the lag probe, the `:metrics` histogram and the tools).
  Libraries: reactor `ef26c30` (LagProbe), http `e5b9cd9` (`enable_lag_probe`), and for `profile_light` only http `trace/hop` `10c5074`
  (`-DHTTP_TRACE_HOP`). Engine: strata (`XAPIAND_ENGINE=strata`), one `--solo` node.
- Host: Intel Core i9-9980HK (8 cores, 16 threads), 32 GiB, macOS 15.8.1 x86_64, kqueue. Taken under `QUIET_PAUSE=1 quiet strict`, which on a
  Mac holds the host and pauses the other agents' light holds; Detent's VMs were not stopped. The harness preflight is the dry run in the ledger.
- Builds: `RelWithDebInfo` with `-fno-omit-frame-pointer` (`-O2 -g`). Binary A is the plain build; binary B adds `-DHTTP_TRACE_HOP`.
- Load, as the design fixes it: `accounts` = 1,000,000 documents (the 1,000 accounts of `docs/public/assets/accounts.ndjson` with explicit
  `_id`s), 10 reactors (the default on this host), heavy = 4 x reactors = 40 closed-loop clients (39 searches and aggregations, 1 RESTORE
  stream of 20,000-document requests), light = open loop at a fixed rate, 10% write-then-GET pairs, 45% GET by id, 45% single-term
  searches (`contact.state`, `_limit` 10). Phases are 30 s. Latency runs from each request's scheduled send time.

## Commands

```sh
# build A and B (the second takes the http branch from its worktree)
cmake -S . -B build-a -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer && cmake --build build-a --target xapiand
cmake -S . -B build-b -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -DHTTP_TRACE_HOP" \
      -DFETCHCONTENT_SOURCE_DIR_HTTP_LIB=$HOME/code/Kronuz/http-wt/trace-hop && cmake --build build-b --target xapiand
# all rows below, one hold (about 12 minutes plus a 5 minute load of accounts)
QUIET_PAUSE=1 quiet strict NAME 90 "pre-7c baseline" -- harness/baseline_7c.sh build-a/bin/xapiand build-b/bin/xapiand OUTDIR
python3 harness/baseline_report.py OUTDIR            # renders the tables below
```

`baseline_7c.sh` runs, and removes the nodes of: `stall_load.py --suite --census` (saturate, T5 unloaded and loaded, `--restore-into accounts`,
T8 both overloads), `profile_light.sh BIN_TRACE OUT --secs 20` (T11), `profile_bulk.sh BIN OUT --mult 20` and `loadtest.py` with 16 clients
and 3 trials (T12). After 7c, T11 compares two such files: `harness/profile_light.sh --compare PRE.json POST.json`.

## Results

stall_load: commit 142b2ca77, 16 CPUs, 10 reactors, accounts 1,000,000 documents (loaded in 303.53 s), 30 s phases, light rate 210/s is half the saturating rate, rounded down to 5, total 665.7 s

saturating light rate: 425/s (first bad 450/s; unloaded p99 37.2 ms (the lower of the first two steps, floor 1 ms), limit 372.0 ms)

| rate /s | query p99 ms | good |
|---:|---:|:--|
| 25 | 1,631.2 | yes |
| 50 | 37.2 | yes |
| 100 | 38.5 | yes |
| 200 | 58.1 | yes |
| 400 | 313.1 | yes |
| 800 | 9,653.1 | no |
| 600 | 4,183.4 | no |
| 500 | 1,671.6 | no |
| 450 | 537.5 | no |
| 425 | 315.8 | yes |

| phase | light /s (target, issued) | query p50 ms | p99 ms | p99.9 ms | unfinished | 503 | other err | worst loop p99 / p99.9 lag (us) | accounts segments (backlog) | RSS MB |
|---|---|---:|---:|---:|---:|---:|---:|---|---|---:|
| unloaded | 210, 210 | 4.6 | 70.6 | 111.9 | 0 | 0 | 0 | 2560 / 2560 (control) | 59 (0) | 2,576 |
| loaded | 210, 209.99 | 23,984.4 | 30,757.5 | 31,310.9 | 2943 | 0 | 0 | 2560 / 2560 (control) | 55 (26) | 2,819 |
| loaded-same-index | 210, 209.99 | 23,982.8 | 30,134.1 | 30,496.1 | 2912 | 0 | 0 | 2560 / 2560 (control) | 34 (0) | 2,884 |
| overload-light-4x | 1700, 1699.98 | 19,503.2 | 30,470.3 | 30,680.3 | 35122 | 0 | 0 | 2560 / 2560 (control) | 29 (0) | 3,054 |
| overload-heavy-4x | 210, 209.99 | 24,379.4 | 37,969.8 | 38,207.9 | 5929 | 0 | 0 | 2560 / 2560 (control) | 30 (0) | 3,076 |

- loaded: heavy 269 searches, 100,000 documents restored into restore-target, 0 503
- loaded-same-index: heavy 278 searches, 20,000 documents restored into accounts, 0 503
- overload-heavy-4x: heavy 331 searches, 0 documents restored into restore-target, 0 503

per-loop lag in the loaded phase (bucket upper edges, us):

| loop | wakes | p50 | p99 | p99.9 | max so far |
|---|---:|---:|---:|---:|---:|
| http0 | 23858 | 256 | 640 | 2048 | 5000 |
| http1 | 18066 | 256 | 896 | 2048 | 5001 |
| http2 | 25310 | 320 | 640 | 1792 | 5007 |
| http3 | 24581 | 256 | 512 | 1792 | 4989 |
| http4 | 22707 | 256 | 768 | 2048 | 4986 |
| http5 | 21858 | 256 | 768 | 2048 | 5061 |
| http6 | 22707 | 256 | 512 | 2048 | 5001 |
| http7 | 23822 | 256 | 768 | 2048 | 5018 |
| http8 | 24199 | 256 | 512 | 2048 | 5002 |
| http9 | 24530 | 256 | 640 | 2048 | 5027 |
| control | 399 | 1536 | 2560 | 2560 | 31874 |

thread census mid-loaded-phase: unnamed 31, Xapiand:DI 30, Xapiand:AC 10, Xapiand:DP 10, Xapiand:FS 8, Xapiand:TR 8, Xapiand:WL 8, Xapiand:DU 4, Xapiand:PU 4, Xapiand:SE 4, Xapiand:SU 4, main 1, Xapiand:AC-- 1, Xapiand:DBCL 1, Xapiand:DU-- 1, Xapiand:FS-- 1, Xapiand:IU-- 1, Xapiand:LOG 1, Xapiand:PU-- 1, Xapiand:SU-- 1, Xapiand:TR-- 1; total 131

profile_light (T11, concurrency 1, GET by id, 22,371 requests, mean 760 us): CPU ms/request hop 0.0165, scheduler 0.0022, engine 0.1387, http 0.4677, other 0.0358; hop CPU share 0.0249; hop wall median 25.2 us (post to start 10.5, end to resume 14.3), handler run median 470.8 us, hop share of offloaded wall 0.0665

loadtest (T12): RESTORE 20,000 documents 6,076 docs/s (median of 7964, 6076, 6020); queries at 16 clients 1,277 QPS (median of 1424, 1064, 1277), p50 11.771 ms, p99 31.025 ms

profile_bulk smoke (T12): 20,000 documents in 4.38 s = 4,564 docs/s, 1.131 ms CPU/doc


## Reading the numbers

- **T5 is far from met before 7c.** Unloaded, the light queries at half the saturating rate have p99 70.6 ms (p50 4.6 ms). Under the heavy
  load 2,943 of about 6,300 light units were still waiting when the 10 s drain ended and the rest took tens of seconds: the light
  percentiles in the loaded rows are censored at the age of the unfinished requests (about 30 s), so they say "starved", not a latency. The
  same-index run is no worse than the separate-index run here because the heavy searches already starve the light requests.
- **T4: the HTTP loops are within the 1,024 us edge before 7c** (loaded p99 512 to 896 us, p99.9 at most 2,048 us, max 5.1 ms) because the
  handlers run on the offload pool, not the loops. After 7c the same row measures the loops doing the work.
- **The control loop reads 1,536 us p50 and 2,560 us p99 even idle**, because its probe wakes every 100 ms and kqueue's timer coalescing
  makes a long sleep late by about 1.5 ms. T4's 1 ms bound cannot hold for the control loop at that interval on this host. A decision for the
  lead: probe the control loop at 1 ms always (one more loop waking 1,000 times a second), or have T4 read it only while busy.
- **The probe's cost.** An idle node woke 11 loops 10 times a second each; its CPU over 30 s was 0.49 and 0.50 s with the probes and 0.42 and
  0.79 s without, within the noise of the node's own background threads. Busy loops woke 600 to 800 times a second (the 1 ms interval). On
  kqueue each re-arm writes the reactor's wake pipe and causes one extra loop iteration, which is why `profile_light` runs with
  `XAPIAND_LAG_PROBE=0`.
- **T8** gives 0 503s in both overloads before 7c: the offload gate's window (1,000 a reactor) is never reached; the requests queue instead,
  which is the unfinished count.
- **T11 (profile_light).** Concurrency 1, GET by id through `ab -k`, `sample` of the node. `sample` takes about one sample per 1.7 ms per
  thread, so a group's CPU is its busy samples times that interval over the requests; the samples are in the JSON
  (`cpu_samples`: hop 214, scheduler 29, engine 1,802, http 6,077, other 465). The offload hop is 16.5 us of CPU per request (cond-signal
  wake, post, dequeue) and 25.2 us of wall (post to start 10.5 us, end to resume 14.3 us, median), against a handler run of 470.8 us, with
  the sampler attached. The dispatch (hop plus scheduler) is 18.7 us of CPU and 25.2 us of wall per request: T11 asks 7c's dispatch to be at
  most half of each. The `http` group is mostly the reactor thread's socket work and is not part of the comparison.
- **T12** RESTORE of 20,000 accounts: 6,076 docs/s in `loadtest.py` (three trials 7,964, 6,076, 6,020) and 4,564 docs/s in the
  `profile_bulk.sh` smoke (a different request shape). Saturated queries: 1,277 QPS (1,424, 1,064, 1,277). The host has 16 logical CPUs, so
  T12's "more than 10 CPUs" holds.
- **Segments and merge backlog.** The node reports neither, so `stall_load.py` counts `seg-*.sx` files in the data directory at the end of each
  phase and watches the count settle for up to 15 s; the backlog is what that wait removed (the loaded phase left 26 segments of 55 to merge).
- **Thread census (T1's today).** 131 threads: 31 unnamed (the reactors, the offload pools and the control thread), DI 30, AC 10, DP 10,
  FS 8, TR 8, WL 8, DU 4, PU 4, SE 4, SU 4, and one each of main, DBCL, LOG and the `--` suffixed helpers. `ps -M` counts the same 131.

## Known limits of this baseline

- One host and one run of each row, with other agents' work paused but Detent's VMs running; the three-trial rows show the spread.
- The first dry runs found two harness faults that this file does not use: a reference p99 taken while merges ran after the load, and an
  unbounded drain after an overload.
