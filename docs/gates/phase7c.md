# Gate 7c: Xapiand that never blocks a reactor

> Status: DRAFT for review (2026-11-13). Master moved under the scores: Xapiand:b6a25cf18 pins strata 397c02d5
> (sync-at-first-commit) instead of 86fe7c47, merged here; every row reruns at Xapiand:d53d85c22 (jobs in "Reruns at
> the merged head"). Until those are read, each row names the code its number ran: most rows master
> Xapiand:6ede7a0ce's `src` (strata 86fe7c47); T11 and T13's per-open-index cost Xapiand:27d45f101's `src` (strata
> 5d742b24). Misses: T4 (worst loop
> p99 6,144 us against 1,024 us), T5's first half (client-side loaded light p99 32.3 ms against 9.14 ms), T10's flood
> (light GET p99 45.6 ms against about 20 ms) and T4 beside four looping scripts, T13's coverage (6 of 9 areas) and the
> reader registry's bytes, C5 on FreeBSD (owner D86: a stated miss). T12 now passes (RESTORE 1.121, QPS 2.642 of
> pre-7c). Not shown: T8's RSS within 10% of T4's and its 503s within one slice, T9's "within one slice", the wide
> search at 10,000 indexes on master's code and SR3's close, T10's flood timing on Linux (a light job). A
> 2,500-index wide search took p50 23.1 s on strata 86fe7c47, caused by the strata bump Xapiand:55741956f and fixed
> by Xapiand:b6a25cf18's pin (Wide searches).

Rows are `docs/design/phase7c.md` section 4. The code under test is master Xapiand:6ede7a0ce's `src` (x7-loop-fix-c,
loop-scheduling, schema-load-once, one-id) with this quest's harness: heads Xapiand:0a0321691 and Xapiand:6dcc770ff
differ from it in `harness/` and `docs/` only. Functional rows run as `light` jobs on this host (macOS 15 x86_64, Intel
Core i9-9980HK, 8 cores, 16 logical CPUs) and on strata's Linux VM (8 vCPUs, 15 GiB, Ubuntu 24.04, kernel 6.8).

Jobs, functional (light, all pass unless named):

- **rows** 261113-111808-93efd0 (Xapiand:0a0321691, macOS): Release and ASan+UBSan builds, `ctest` under ASan+UBSan (39
  of 39), every row of `gate7c_rows.sh`: 14 of 14 rows pass.
- **build-test** 261113-111813-3cf199, **e2e** 261113-111813-3013ac, **t7-tsan** 261113-111814-1a49cf, **recovery**
  261113-111814-f9dc7f, **removed** 261113-111814-e12ef2, **restore corpora** 261113-111821-2f9e75.
- **linux** 261113-130015-3cce93 (Xapiand:6dcc770ff, failed on one row): Release (clang 18) and ASan+UBSan (gcc 13, as
  CI) builds, every row of `gate7c_rows.sh`, T6 at 2x RAM and in RAM, the restore corpora. Lua came from a patched
  copy on the VM (`FETCHCONTENT_SOURCE_DIR_LUA`), not the pinned tarball and patch; the rerun uses the pin. 13 of 14 gating rows and the
  info row pass; `t10_lua` fails on its flood timing limit (T10, C5). The first run, 261113-111826-184241, is
  superseded (clang's UBSan stopped every ASan row in lz4; `memory.max` OOM-killed T6's loaded node: R18).
- **x3-5** 261111-165347-d979c7 (Xapiand:03ba6478a, built in-job: Xapiand:27d45f101's `src`, strata 5d742b24, light):
  per-open-index resident cost, 10,000 indexes.
- **CI** run 42820866169 at Xapiand:ae2ea6c9b: ASan+UBSan and TSan on Linux (smoke, load, docs E2E, TLS).

Jobs, timing (D59: one job of at most 10 minutes each, holding ABBA pairs; head against pre-7c Xapiand:142b2ca77, both
RelWithDebInfo with frame pointers; 16 reactors on head, 10 on pre-7c, each binary's default). Sizes are 20,000
accounts, not the design's 1,000,000 (R11); the light rate is 200/s open loop on both arms. Ratios are head over pre-7c,
the median of the pairs with their range. The pre-7c builds are 2026-11-11's (same commit). These jobs did not build
the binary they timed: each ran the `x7-timing` slot's `build-cur`, so the commit a job names is its harness, and the
binary is the last build of that slot. The 11-13 jobs below timed 261113-111748-e52352's build at Xapiand:0a0321691
(master 6ede7a0ce's `src`, strata 86fe7c47); the 11-11 jobs 261111-171014-19268b's at Xapiand:49ebb4481 (master
27d45f101's `src`, strata 5d742b24). The reruns build `build-cur` in the job and print its source stamp.

- **t45** 261113-125927-90f8a3 (exclusive, Xapiand:6dcc770ff): `stall_load.py --phase both --restore-prebuilt 8` pre,
  cur, cur, pre; a same-index run on cur.
- **t12q** 261113-125938-4f5407 and **t12b** 261113-125941-90ebdc (exclusive): `loadtest.py` (RESTORE of 20,000, then 16
  clients for 8 s) and `profile_bulk.sh --mult 20`, 3 pairs each.
- **t10** 261113-125943-736919 (strict): `lua_bounds.py --loops 4`, then the loaded phase beside 4 looping scripts.
- **a16** 261113-125946-4ec27a (strict): `wide_search_check.py gate`, 2,500 indexes, 10 searches, each loop's lag (SR3);
  e52352's binary.
- **a21** 261113-111904-2e33d9 (exclusive, Linux VM) and 261111-171055-51a31b (exclusive, macOS): before
  (Xapiand:b6b8f7534) and after (Xapiand:927acf570) the one-shape wave, 3 pairs; allocations and handlers
  261111-200223-9495ee.
- **t11** 261111-171043-7a746f (exclusive, 19268b's binary: Xapiand:27d45f101's `src`, strata 5d742b24; not rerun):
  `profile_light.sh` (20 s), pre-7c's trace/hop build against head, 3 pairs.

**Reruns at the merged head** (Xapiand:d53d85c22: master Xapiand:db349389f merged, strata 397c02d5; F1, F2). Timing
jobs build `build-cur` in the job (261113-164916-d5bb9d builds it first, light) and print its source stamp
(`git ls-tree` of the build's inputs): t45 261113-164926-89b9ef, t12q 261113-164929-53a2d2, t12b 261113-164931-09250a
(exclusive), t10 261113-164933-460810 (strict), a16 as in Wide searches, T13's per-open-index cost
261113-164953-250830 (light, on the 10,000 indexes). Functional: rows 261113-164955-f95555, corpora
261113-164958-204ccb, Linux 261113-165001-16b520 (pinned Lua), checks build-test 261113-165003-4bde4b, e2e
261113-165003-fb9907, t7-tsan 261113-165003-2b6d6d, recovery 261113-165004-4bafe6, removed 261113-165004-905bcf.
T3, T4, T5, T7, T10, T12, T13 and A16 are rescored from them.

**P1 generator fix.** `stall_load.py` built each 20,000-document RESTORE body on its own asyncio loop, stalling light
requests 50-159 ms (PLAN.md "P1 diagnosis"). T4/T5 run with `--restore-prebuilt 8` (bodies built before the clock
starts, written apart from their head) and report light latency client-side and server-side (`Response-Time`).

## Rows

| Row | Target | Evidence | Status |
|---|---|---|---|
| T1 no offload pool | Census: section 2's threads only; `--strict` exits 0 | rows 93efd0: `t1_strict_owed`; `t1_census` under load: 16 unnamed (the reactors), 16 `W`, 9 `FS`, 8 `WL`, `LOG`, main; 51 threads, none outside section 2. Linux 3cce93: `Xapiand:MAIN` 9 (8 reactors and main, which inherit main's name there), `W` 8, `FS` 5, `WL` 4, `LOG` 1; 27 threads, none outside section 2 | PASS (macOS, Linux) |
| T2 E2E on strata | 0 regressions, 0 guard hits, 0 ASan reports; `stored` at 2x the cap and just under it byte-equal | rows 93efd0 and Linux 3cce93: `t2_e2e_asan`, `t2_stored_asan`, `t2_restore_stream_asan` (info); e2e 3013ac; CI ASan+UBSan | PASS (macOS, Linux) |
| T3 recovery | `wal_recovery_check.sh`, `engine_crash_check.sh --reactors 2` | recovery f9dc7f; rows 93efd0 and Linux 3cce93: `t3_crash`, `t3_crash_asan` | PASS |
| T4 reactor stall | p99 lag at most 1,024 us on every loop under the mixed load; p99.9, max; a script-heavy row (X4-R1) | t45 261113-125927-90f8a3 (master 6ede7a0ce, generator fix): loaded, every one of the 16 HTTP loops over the edge in both head arms, worst p99 6,144 us (http2), p99.9 12,288 and 24,576 us; pre-7c's HTTP loops stay under it (worst loop control, 1,280 us). Same-index run: worst p99 8,192 us. At 2026-11-11's code (c9ab68): worst p99 24,576-28,672 us. Script-heavy: t10 261113-125943-736919 | MISS, 6x the edge (was 24-28x) |
| T5 light-query p99 | Loaded at most 2x unloaded + 1 ms and below pre-7c; same-index run as Q6's cost | t45 261113-125927-90f8a3, light 200/s, 64 heavy clients and a RESTORE stream, bodies prebuilt. Client side: head unloaded p99 3.90 and 4.24 ms (limit 9.14 ms), loaded p99 36.3 and 28.4 ms (median 32.3 ms); pre-7c loaded 547.6 and 380.6 ms; head/pre 0.070 (0.066-0.075). Server side (`Response-Time`), loaded p99: all light 40.2 ms (36.6-43.7), search 37.4 ms, pair (PUT then GET) 58.7 ms; head/pre 0.079. Generator wait p99 1.4 ms on head (it was the 50-159 ms P1 stall). Same index: client p99 78.3 ms. 2026-11-11 (old generator, c9ab68): loaded p99 76.8 ms | MISS on the first half: client-side loaded p99 32.3 ms is 3.5x the 9.14 ms limit, which is built from the client-side unloaded p99 (server-side 40.2 ms; PLAN.md P1: server search p99 about 44 ms, pair about 53 ms, under 63 heavy clients); met on the second (14x lower than pre-7c) |
| T6 reactor faults | Major faults per 1,000 queries at 2x RAM, reported (design: at most 1 on reactor threads, gated at Phase 6) | Linux 3cce93, Release, `stall_load.py --index-size 2xram`, 200,000 accounts: the node's cgroup gets `memory.high` at its anonymous memory plus half the index on disk (index 227 MB, anon 528 MB, cap 642 MB; 94.8 MB of file cache left after the cap); major faults per thread from `/proc/PID/task`. Unloaded: 1,752 over 6,000 queries, **292 per 1,000**, 1,679 of them on `Xapiand:MAIN` (the reactors); light p99 10.5 ms, worst loop p99 2,560 us. Loaded: 439,351 over 5,762 queries, **76,250 per 1,000**, 437,992 on the reactors; light p50 1.94 s, p99 21.1 s, worst loop p99 1,310,720 us (http1). In RAM (no cap): 0 per 1,000 unloaded and loaded (12,495 queries), loaded light p99 37.9 ms, worst loop p99 8,192 us | Reported. Against the design's Phase 6 target of 1 per 1,000: 292x over unloaded, 76,000x loaded. The reactors take the faults themselves (99.7% loaded): evaluate reads mapped segments on the loop (design D6, item 3), so at 2x RAM a fault stalls a reactor for its read and a loaded node's loops stall for seconds. The loaded value also carries `memory.high`'s reclaim throttling of the allocating threads, so it is an upper bound on fault cost, not on fault count |
| T7 concurrency, shutdown | 0 TSan, 0 ASan; dump equals the model; SIGTERM at 300 s exits 0, writes durable | t7-tsan 1a49cf; rows 93efd0 and Linux 3cce93: `t7_asan` (dump equals the model, 0 sanitizer reports; Linux 13,788 model documents) | PASS |
| T8 overload | Non-zero 503s in both overloads; peaks; RSS within 10% of T4's; 16-record registry gives 503s | rows 93efd0: 6 held rounds each of 2,500 against 2 x 1,000; every round admitted 2,000 and refused 500 (in-flight 503); admitted searches past the reader registry got `Too many searches are reading at once` 503s (light 4,160, heavy 11,676); 0 other errors. Peaks: in-flight 1,000 (the cap), demoted 0 light, 49 heavy (cap 256 not reached). RSS 482 and 411 MiB at 20,000 documents and 2 reactors; T4's run (16 reactors) is not comparable. Linux 3cce93: the same, peaks 1,000 and 0 / 24. build-test 3cf199: `test_readers_cap` | PASS on 503s, peaks and the registry; RSS within 10% of T4's and "503s within one slice of the cap" not shown (R3, R7) |
| T9 cancellation | A disconnected read frees its reactor in one slice; a half-closed write is durable | quest x7-cancel-rows (landed): `test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted` (the reactor let the search go 2,078 us after the close, `xapiand_http_requests_cancelled` rises), the same in its first schema load (3,082 us), `test_a_put_whose_client_half_closes_completes_durably`; its build-test 261110-173050-487b1b, mutants 261110-164634-b7eb0b and 261110-164637-c92fef fail them. At this head: build-test 261113-111813-3cf199 | PASS on the counter and the durable half-closed write; "within one slice" not shown: freed 2.1-3.1 ms after the client's close, which includes the close reaching the reactor |
| T10 Lua bounds | Every case 4xx; T4 holds with four loops; 16-client flood keeps a light GET's p99 near 20 ms | rows 93efd0 (macOS Release and ASan) and Linux 3cce93 (ASan): every case passes. t10 736919 (strict, macOS): light GET p99 0.79 ms alone, 0.76 ms beside 4 scripts, 0.57 ms beside 4 library loops, **45.61 ms** (max 48.55 ms) beside the 16-client flood (2026-11-11: 50.94 ms); T4 beside 4 looping scripts: worst loop p99 6,144 us (http2), light p99 67.7 ms. Linux 3cce93 `t10_lua` (Release, light job in the VM): every case passes; flood p99 118.0 ms against `lua_bounds.py`'s 100 ms limit, so the row exits 1 | functional PASS; flood MISS (2.3x the 20 ms, strict on macOS); T4 with loops MISS (6x the edge). Linux's 118.0 ms is not a timing result: a light job runs at `nice 19` beside other holds in a VM with no pinned cores, so it cannot decide a p99 bound; the Linux flood value is unproven, not a pass |
| T11 dispatch cost | At most half the offload hop's, CPU and wall | t11 7a746f (Xapiand:27d45f101's `src`, strata 5d742b24): dispatch CPU 0.0017 ms per request against the hop's 0.0188 ms, head/pre 0.073 (0.073-0.098); wall: head has no hop to time (`--compare` prints `-`), the hop's median was 25.05 us | PASS (13.7x less CPU) at Xapiand:27d45f101's `src`; not rerun on master's code |
| T12 no throughput loss | RESTORE docs/s and saturated QPS at least 1.0x pre-7c, CPU count stated | t12q 4f5407: RESTORE 8,587 against 7,588 docs/s, head/pre **1.121** (1.030-1.141); QPS 4,271 against 1,518, **2.642** (2.638-3.105); query p99 0.301 (0.291-0.324). t12b 90ebdc: RESTORE 1.052 (1.046-1.222), CPU per document 0.641 (0.579-0.674) of pre-7c's; arm 2's sampled profile is invalid (147 samples), its smoke values stand. 16 logical CPUs. 2026-11-11 (before fix (c), x7-loop-fix-c): RESTORE 0.288 | PASS (RESTORE 1.12x, QPS 2.6x); no pair interval straddles 1.0 |
| T13 every gate | ASan, UBSan, TSan; coverage at the targets; restore corpora; per-open-index cost at 10,000 | rows 93efd0 (ASan+UBSan `ctest`, 39 of 39); Linux 3cce93 (ASan+UBSan rows, gcc 13); t7-tsan 1a49cf; CI; x3-5 d979c7 (Xapiand:27d45f101's `src`, strata 5d742b24; the open path changed twice since): 10,001 open indexes, resident 10.1 to 353.3 MiB, 35.1 KiB per open index; master's own A16 run (4ec27a, strata 86fe7c47) shows 107.6 KiB at 200 open. Coverage (x7-coverage, 261111-144409-ecb5b2, `harness/coverage.sh`): collectors 96.4% lines / 94.5% branches, strata 91.5 / 82.8, seam 94.1 / 92.0 meet TESTING.md; decoders 59.9 / 53.5 (target 98 / 95), server 75.2 / 48.4, database 55.5 / 46.2, aggregations 75.2 / 70.9, geospatial 28.6 / 31.6, lua 89.4 / 78.2 (90 / 80) are below. Corpora (`tests/corpora`, alpha9 and master; HTTP RESTORE and `--restore`, 3,000 of 3,000 each): 2f9e75, Linux 3cce93 | sanitizers PASS; coverage MISS in 6 of 9 areas; corpora PASS; the reader registry's bytes not measured (MISS) |

## Carried conditions

| Condition | Evidence | Status |
|---|---|---|
| C5: Linux and FreeBSD functional rows (arm64 before 7b) | Linux 3cce93: 13 of 14 gating rows pass (`t1_strict_owed`, `t1_census`, `t2_e2e_asan`, `t2_stored_asan`, `t3_crash`, `t3_crash_asan`, `t7_asan`, `t8_overload_light`, `t8_overload_heavy`, `t10_lua_asan`, `x6_shutdown`, `gate_tools`) plus the info row; `t10_lua` fails only its flood timing limit (118.0 ms against 100 ms, light job: T10); restore corpora pass. FreeBSD: no machine runs the rows (`freebsd.yml` builds release tags only; strata's FreeBSD VM cannot build Xapiand without being regrown) | Linux PASS except T10's flood timing (unproven); FreeBSD MISS, stated (owner D86: a FreeBSD CI rows workflow after 7c); arm64 not run |
| Wide searches: 10,000-index latency and peak pinned views; SR3 with `stall.py` | Cause: the strata bump Xapiand:55741956f (strata 5d742b24 to 86fe7c47) made a writable open sync the manifest log and directory under one device flush, and the catalog evict as soon as it passes its soft cap (`--database-pool-size`, default 200), so a 2,500-index search reopens about 2,300 indexes at about 8.7 ms each. Bisect (`docs/perf/a16-many-index-regression.md` on explore/a16-bisect, each arm built in its own job, 2,500 indexes, 10 searches, strict): Xapiand:27d45f101 p50 98 ms, Xapiand:5c0738825 108 ms, Xapiand:55741956f 19,977 ms. Fix: strata 397c02d5 (sync-at-first-commit: an open leaves the flush to its first commit), pinned by Xapiand:b6a25cf18 (owner D89, which also sizes the pool from memory and makes reopening cheap: quests pool-from-memory, cheap-reopen). Numbers and the build behind each: a16 4ec27a (e52352's binary: master 6ede7a0ce's `src`, strata 86fe7c47): p50 23,136 ms, p99 29,177 ms, peak pinned 100, 200 open after the first search, 107.6 KiB each; 2567dc (19268b's binary: 27d45f101's `src`, strata 5d742b24; its log names Xapiand:77d649648, the harness): p50 108 ms, p99 1,176 ms, 1,382 open, 42.1 KiB each; strata 397c02d5 (bisect arm 0d97b2da7, built in-job): p50 884 ms, p99 1,011 ms at the default pool (261113-155318-cb2a87), 132 ms at `--database-pool-size 5000` (261113-155320-bf8f29). 10,000 indexes only at 27d45f101's `src` (x3-5 d979c7, light, pool 10,000): p50 686 ms, peak pinned 5,000. SR3 (a16 4ec27a), each loop's lag over the 10 timed searches: http0, http1 and control p99 and p99.9 in the 1,280 us bucket; the 99.0 ms max (http1) is the node's max since start, which includes the opening search. Reruns at Xapiand:d53d85c22, each building its binary: 2,500 at the default pool 261113-164936-336058 and at pool 5,000 261113-164938-d63930; 10,000 created by a light job (261113-164941-738ff2) and timed by a strict one on its data (261113-164951-e5baa9); SR3's max now read over the timed searches only (the window's highest bucket) | reported value at 2,500 on strata 86fe7c47; 10,000 on master's code not shown until e5baa9 is read; SR3 not closed (its max is not over the timed searches until the reruns) |
| Owed async rows decided (D4 to D7), `--strict --owed` gates | build-test; rows `t1_strict_owed` | PASS |
| D16 restated to the code's order; an index that cannot close in 35 s exits 75 | `docs/design/phase7c.md` D16 | done (reviewer checks it against `src/manager.cc`) |
| X6: one stop bound; `--committers`, `num_committers`, `DatabaseCleanup` gone | removed; rows: `x6_shutdown` | PASS |
| X3-1 C3: the non-allocating `claimed()`; admission at `readers_per_index` | build-test; `StrataReaders` is gone since x-pool (Xapiand:d43d00046): the cap is strata's reader records (`src/database/pool.cc`, `host.readers_per_index`) | done at master's code |
| One shape everywhere: CPU and p99 per request (Linux, macOS), allocations and handlers | a21 (macOS): CPU per light request after/before 0.958 (0.930-1.019), p99 0.962 (0.960-0.972), within D62 (over 5% CPU or 10% p99 fails). Allocations and asio handlers per light request, 261111-200223-9495ee (a DYLD malloc interposer and asio handler tracking, ABBA x3): allocations 0.988 (0.987-1.005), bytes 1.000 (0.997-1.000), handlers 0.986 (0.967-1.000); about 1,000 allocations, 1.6 MB and 68 handlers per request on both. Linux (strata's VM, b6b8f7534 against 927acf570, ABBA x3), 261113-111904-2e33d9: CPU per light request 1.008 (0.991-1.017), p99 1.001 (0.989-1.082), within D62 | PASS (macOS and Linux) |
| X3-4: `Shard::view()` at 1 and 8 threads | quest x7-view-bench (landed), 261111-175937-76ae57 (strict, `docs/perf/shard-view.md`): median 18.6 ns per call at 1 thread, 20.3 ns (one shared shard) and 21.4 ns (a handle each) at 8; p90 29.8 against 18.9 ns; 52.5 against 298 Mcalls/s in total (5.7x on 8 threads). macOS x86_64; no concurrent writer | measured |
| T4's treatment of the control loop's idle probe | D53: read only while busy | decided |
| Perf: the sliced-search residual and the watch byte's read before T12 | master pins prospect 850d4e8 (contains prospect:5c98f81) and http:51d153a (Xapiand:1596eeed7); T12's jobs ran at a master pinning both | settled |
