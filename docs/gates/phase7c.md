# Gate 7c: Xapiand that never blocks a reactor

> Status: DRAFT for review (2026-11-13). Every row is scored at Xapiand:d53d85c22, whose `src` and build equal master
> Xapiand:db349389f's (strata 397c02d5, sync-at-first-commit), except where a row names older code: T9's mutants, T11
> (Xapiand:27d45f101's `src`), coverage, A21 and X3-4, which are fixed-commit measurements not rerun. Misses: T4 (worst
> loop p99 6,144 us against 1,024 us), T5's first half (client-side loaded light p99 33.0 ms against 8.91 ms), T10's
> flood (light GET p99 48.0 ms against about 20 ms) and T4 beside four looping scripts (5,120 us), T13's coverage (6 of
> 9 areas) and the reader registry's bytes, SR3 (the HTTP loops stall 49-100 ms during a 2,500-index wide search), C5
> on FreeBSD (owner D86: a stated miss). Reported values: a wide search over 10,000 indexes takes p50 4,106 ms, p99
> 10,887 ms at the default pool; over 2,500, p50 884-1,364 ms (two runs of the same code). Not shown: T8's RSS within
> 10% of T4's and its 503s within one slice, T9's "within one slice", T10's flood timing on Linux (a light job).

Rows are `docs/design/phase7c.md` section 4. The code under test is Xapiand:d53d85c22: master Xapiand:db349389f merged
(Xapiand:0fd94ad9a), plus this quest's `harness/` and `docs/`; `git diff db349389f d53d85c22` outside `docs`, `harness`
and `.campaign` is empty. Xapiand:c43ebf6c2 and the merge Xapiand:7f978b2bd change `docs/` only. Functional rows run as
`light` jobs on this host (macOS 15 x86_64, Intel Core i9-9980HK, 8 cores, 16 logical CPUs) and on strata's Linux VM (8
vCPUs, 15 GiB, Ubuntu 24.04, kernel 6.8). Every job below builds the binary it runs, in the job.

Jobs, functional (light, at Xapiand:d53d85c22, all pass unless named):

- **rows** 261113-164955-f95555 (macOS): Release and ASan+UBSan builds, `ctest` under ASan+UBSan (39 of 39), every row
  of `gate7c_rows.sh`: 14 of 14 rows pass.
- **build-test** 261113-165003-4bde4b, **e2e** 261113-165003-fb9907, **t7-tsan** 261113-165003-2b6d6d, **recovery**
  261113-165004-4bafe6, **removed** 261113-165004-905bcf.
- **restore corpora** 261113-191724-166dd3 (Xapiand:c43ebf6c2, its own build directory `build-corpora`). The first run,
  261113-164958-204ccb, failed on the host, not the code: it linked in the `x7-rows` slot, whose `.deps` held the rows
  job's ASan dependency builds, and stopped on undefined `___asan_*` symbols (R10's mechanism).
- **linux** 261113-165001-16b520 (failed on one row): Release (clang 18) and ASan+UBSan (gcc 13, as CI) builds, Lua from
  the pinned tarball (R22), every row of `gate7c_rows.sh`, T6 at 2x RAM and in RAM, the restore corpora. 13 of 14
  gating rows and the info row pass; `t10_lua` fails on its flood timing limit (T10, C5), which a light VM job cannot
  decide (R19).
- **T13 per-open-index cost** 261113-164953-250830 (light): `wide_search_check.py gate --reuse --pool 10000 --runs 3` on
  the 10,000 indexes 261113-164941-738ff2 created (534 s).
- **CI** run 42820866169 at Xapiand:ae2ea6c9b: ASan+UBSan and TSan on Linux (smoke, load, docs E2E, TLS).

Jobs, timing (D59: one job of at most 10 minutes each, holding ABBA pairs; head against pre-7c Xapiand:142b2ca77, both
RelWithDebInfo with frame pointers; 16 reactors on head, 10 on pre-7c, each binary's default). Sizes are 20,000
accounts, not the design's 1,000,000 (R11); the light rate is 200/s open loop on both arms. Ratios are head over pre-7c,
the median of the pairs with their range. 261113-164916-d5bb9d (light) builds the `x7-timing` slot's `build-cur` at
Xapiand:d53d85c22; each timing job then runs `cmake --build build-cur` itself (a no-op after it) and prints the build's
source stamp (`git ls-tree` of CMakeLists.txt, cmake, config headers, contrib and src, hashed; R20). Every timing job
below printed stamp 8833ba980, strata 397c02d5, binary built 2026-11-13 16:55:16. The pre-7c arms run
261111-171014-19268b's build of 142b2ca77.

- **t45** 261113-164926-89b9ef (exclusive): `stall_load.py --phase both --restore-prebuilt 8` pre, cur, cur, pre; a
  same-index run on cur.
- **t12q** 261113-164929-53a2d2 and **t12b** 261113-164931-09250a (exclusive): `loadtest.py` (RESTORE of 20,000, then
  16 clients for 8 s) and `profile_bulk.sh --mult 20`, 3 pairs each.
- **t10** 261113-164933-460810 (strict): `lua_bounds.py --loops 4`, then the loaded phase beside 4 looping scripts.
- **a16** 261113-191709-809e9e (strict, Xapiand:c43ebf6c2, same stamp): `wide_search_check.py gate`, 2,500 indexes, 10
  searches, each loop's lag over the timed searches (SR3); 261113-164938-d63930 (strict) the same at
  `--database-pool-size 5000`; 261113-164951-e5baa9 (strict) 10 searches over 738ff2's 10,000 indexes. The first
  default-pool run, 261113-164936-336058, failed on the host, not the code: creating the indexes stopped on `OSError:
  [Errno 49] Can't assign requested address`, the loopback port exhaustion of another agent's run (Codex C7).
- **a21** 261113-111904-2e33d9 (exclusive, Linux VM) and 261111-171055-51a31b (exclusive, macOS): before
  (Xapiand:b6b8f7534) and after (Xapiand:927acf570) the one-shape wave, 3 pairs; allocations and handlers
  261111-200223-9495ee.
- **t11** 261111-171043-7a746f (exclusive, 19268b's binary: Xapiand:27d45f101's `src`, strata 5d742b24; not rerun):
  `profile_light.sh` (20 s), pre-7c's trace/hop build against head, 3 pairs.

**P1 generator fix.** `stall_load.py` built each 20,000-document RESTORE body on its own asyncio loop, stalling light
requests 50-159 ms (PLAN.md "P1 diagnosis"). T4/T5 run with `--restore-prebuilt 8` (bodies built before the clock
starts, written apart from their head) and report light latency client-side and server-side (`Response-Time`).

## Rows

| Row | Target | Evidence | Status |
|---|---|---|---|
| T1 no offload pool | Census: section 2's threads only; `--strict` exits 0 | rows f95555: `t1_strict_owed`; `t1_census` under load: 16 unnamed (the reactors), 16 `W`, 9 `FS`, 8 `WL`, `LOG`, main; 51 threads, none outside section 2. Linux 16b520: `Xapiand:MAIN` 9 (8 reactors and main, which inherit main's name there), `W` 8, `FS` 5, `WL` 4, `LOG` 1; 27 threads, none outside section 2 | PASS (macOS, Linux) |
| T2 E2E on strata | 0 regressions, 0 guard hits, 0 ASan reports; `stored` at 2x the cap and just under it byte-equal | rows f95555 (761 of 761 docs assertions) and Linux 16b520: `t2_e2e_asan`, `t2_stored_asan`, `t2_restore_stream_asan` (info); e2e fb9907 (smoke, e2e, TLS: GREEN); CI ASan+UBSan | PASS (macOS, Linux) |
| T3 recovery | `wal_recovery_check.sh`, `engine_crash_check.sh --reactors 2` | recovery 4bafe6 (GREEN); rows f95555 and Linux 16b520: `t3_crash`, `t3_crash_asan` | PASS |
| T4 reactor stall | p99 lag at most 1,024 us on every loop under the mixed load; p99.9, max; a script-heavy row (X4-R1) | t45 89b9ef, loaded: all 16 HTTP loops over the edge in both head arms, worst p99 6,144 us (http2), worst p99.9 16,384 us; pre-7c's 10 HTTP loops stay under it (worst loop control, 2,560 us). Same-index run: worst p99 7,168 us. Script-heavy: t10 460810 | MISS, 6x the edge |
| T5 light-query p99 | Loaded at most 2x unloaded + 1 ms and below pre-7c; same-index run as Q6's cost | t45 89b9ef, light 200/s, 64 heavy clients and a RESTORE stream, bodies prebuilt. Client side: head unloaded p99 3.76 and 4.15 ms (limit 2 x 3.95 + 1 = 8.91 ms), loaded p99 33.7 and 32.3 ms (median 33.0 ms); pre-7c loaded 385.8 and 326.8 ms; head/pre 0.093 (0.087-0.099). Server side (`Response-Time`), loaded p99: all light 38.1 ms (34.7-41.5), search 34.3 ms, pair (PUT then GET) 82.4 ms (49.4-115.4); head/pre 0.092. Generator wait p99 1.5 ms on head. Same index: client p99 66.7 ms | MISS on the first half: client-side loaded p99 33.0 ms is 3.7x the 8.91 ms limit (server-side 38.1 ms); met on the second (10.8x lower than pre-7c) |
| T6 reactor faults | Major faults per 1,000 queries at 2x RAM, reported (design: at most 1 on reactor threads, gated at Phase 6) | Linux 16b520, Release, `stall_load.py --index-size 2xram`, 200,000 accounts: the node's cgroup gets `memory.high` at its anonymous memory plus half the index on disk (index 227 MB, anon 527 MB, cap 641 MB; 94.7 MB of file cache left after the cap); major faults per thread from `/proc/PID/task`. Unloaded: 1,983 over 6,000 queries, **330.5 per 1,000**, 1,909 of them on `Xapiand:MAIN` (the reactors); light p99 6.6 ms, worst loop p99 2,560 us. Loaded: 555,419 over 5,045 queries, **110,093 per 1,000**, 550,674 on the reactors; light p50 3.38 s, p99 23.2 s, worst loop p99 786,432 us (http1). In RAM (no cap): 0 per 1,000 unloaded and loaded (12,402 queries), loaded light p99 43.8 ms, worst loop p99 8,192 us | Reported. Against the design's Phase 6 target of 1 per 1,000: 330x over unloaded, 110,000x loaded. The reactors take the faults themselves (99.1% loaded): evaluate reads mapped segments on the loop (design D6, item 3), so at 2x RAM a fault stalls a reactor for its read and a loaded node's loops stall for seconds. The loaded value also carries `memory.high`'s reclaim throttling of the allocating threads, so it is an upper bound on fault cost, not on fault count |
| T7 concurrency, shutdown | 0 TSan, 0 ASan; dump equals the model; SIGTERM at 300 s exits 0, writes durable | t7-tsan 2b6d6d (0 failures, 0 sanitizer reports, 22,647 model documents); rows f95555 and Linux 16b520: `t7_asan` (dump equals the model, 0 sanitizer reports; 143,951 and 39,042 model documents) | PASS |
| T8 overload | Non-zero 503s in both overloads; peaks; RSS within 10% of T4's; 16-record registry gives 503s | rows f95555: 6 held rounds each of 2,500 against 2 x 1,000; every round admitted 2,000 and refused 500 (in-flight 503); admitted searches past the reader registry got `Too many searches are reading at once` 503s (light 1,111, heavy 11,636); 0 other errors. Peaks: in-flight 1,000 (the cap), demoted 0 light, 46 heavy (cap 256 not reached). RSS 508 and 420 MiB at 20,000 documents and 2 reactors; T4's run (16 reactors) is not comparable. Linux 16b520: the same, peaks 1,000 and 0 / 55. build-test 4bde4b: `test_readers_cap` | PASS on 503s, peaks and the registry; RSS within 10% of T4's and "503s within one slice of the cap" not shown (R3, R7) |
| T9 cancellation | A disconnected read frees its reactor in one slice; a half-closed write is durable | quest x7-cancel-rows (landed): `test_a_search_whose_client_leaves_frees_its_reactor_and_is_counted` (the reactor let the search go 2,078 us after the close, `xapiand_http_requests_cancelled` rises), the same in its first schema load (3,082 us), `test_a_put_whose_client_half_closes_completes_durably`; its build-test 261110-173050-487b1b, mutants 261110-164634-b7eb0b and 261110-164637-c92fef fail them (fixed-commit mutants, not rerun). At this head: build-test 4bde4b | PASS on the counter and the durable half-closed write; "within one slice" not shown: freed 2.1-3.1 ms after the client's close, which includes the close reaching the reactor |
| T10 Lua bounds | Every case 4xx; T4 holds with four loops; 16-client flood keeps a light GET's p99 near 20 ms | rows f95555 (macOS Release and ASan) and Linux 16b520 (ASan): every case passes. t10 460810 (strict, macOS): light GET p99 1.06 ms alone, 0.83 ms beside 4 scripts, 0.57 ms beside 4 library loops, **48.04 ms** (max 52.89 ms) beside the 16-client flood; T4 beside 4 looping scripts: worst loop p99 5,120 us (http2), light p99 63.5 ms. Linux 16b520 `t10_lua` (Release, light job in the VM): every case passes; flood p99 227.0 ms (227 GETs, 165 flood requests answered) against `lua_bounds.py`'s 100 ms limit, so the row exits 1; the ASan row's flood p99 is 47.7 ms | functional PASS; flood MISS (2.4x the 20 ms, strict on macOS); T4 with loops MISS (5x the edge). Linux's 227.0 ms is not a timing result: a light job runs at `nice 19` beside other holds in a VM with no pinned cores, so it cannot decide a p99 bound (R19); unproven, not a pass |
| T11 dispatch cost | At most half the offload hop's, CPU and wall | t11 7a746f (Xapiand:27d45f101's `src`, strata 5d742b24): dispatch CPU 0.0017 ms per request against the hop's 0.0188 ms, head/pre 0.073 (0.073-0.098); wall: head has no hop to time (`--compare` prints `-`), the hop's median was 25.05 us | PASS (13.7x less CPU) at Xapiand:27d45f101's `src`; not rerun on master's code |
| T12 no throughput loss | RESTORE docs/s and saturated QPS at least 1.0x pre-7c, CPU count stated | t12q 53a2d2: RESTORE 8,521 against 7,790 docs/s, head/pre **1.089** (1.088-1.140); QPS 4,571 against 1,624, **2.815** (2.345-3.085); query p99 0.285 (0.236-0.355). t12b 09250a: RESTORE 1.120 (1.058-1.173), CPU per document 0.614 (0.609-0.643) of pre-7c's; arm 2's sampled profile is invalid (191 wall samples), its smoke values stand. 16 logical CPUs | PASS (RESTORE 1.09x, QPS 2.8x); no pair interval straddles 1.0 |
| T13 every gate | ASan, UBSan, TSan; coverage at the targets; restore corpora; per-open-index cost at 10,000 | rows f95555 (ASan+UBSan `ctest`, 39 of 39); Linux 16b520 (ASan+UBSan rows, gcc 13); t7-tsan 2b6d6d; CI. Per-open-index cost 250830: 10,001 open indexes, resident 10.1 to 375.3 MiB, **37.4 KiB per open index** (40.3 KiB at 2,501 open, d63930; 102.2 KiB at 212 open, 809e9e). Coverage (x7-coverage, 261111-144409-ecb5b2, `harness/coverage.sh`, its own commit): collectors 96.4% lines / 94.5% branches, strata 91.5 / 82.8, seam 94.1 / 92.0 meet TESTING.md; decoders 59.9 / 53.5 (target 98 / 95), server 75.2 / 48.4, database 55.5 / 46.2, aggregations 75.2 / 70.9, geospatial 28.6 / 31.6, lua 89.4 / 78.2 (90 / 80) are below. Corpora (`tests/corpora`, alpha9 and master; HTTP RESTORE and `--restore`, 3,000 of 3,000 each): 166dd3, Linux 16b520 | sanitizers PASS; coverage MISS in 6 of 9 areas; corpora PASS; per-open-index cost 37.4 KiB at 10,000; the reader registry's bytes not measured (MISS) |

## Carried conditions

| Condition | Evidence | Status |
|---|---|---|
| C5: Linux and FreeBSD functional rows (arm64 before 7b) | Linux 16b520: 13 of 14 gating rows pass (`t1_strict_owed`, `t1_census`, `t2_e2e_asan`, `t2_stored_asan`, `t3_crash`, `t3_crash_asan`, `t7_asan`, `t8_overload_light`, `t8_overload_heavy`, `t10_lua_asan`, `x6_shutdown`, `gate_tools`) plus the info row; `t10_lua` fails only its flood timing limit (227.0 ms against 100 ms, light job: T10); restore corpora pass. FreeBSD: no machine runs the rows (`freebsd.yml` builds release tags only; strata's FreeBSD VM cannot build Xapiand without being regrown) | Linux PASS except T10's flood timing (unproven); FreeBSD MISS, stated (owner D86: a FreeBSD CI rows workflow after 7c); arm64 not run |
| Wide searches: 10,000-index latency and peak pinned views; SR3 with `stall.py` | **10,000 indexes**, e5baa9 (default pool 200): p50 4,106 ms, p99 10,887 ms over 10 searches, peak pinned 100; the first search 14,697 ms. At pool 10,000 (250830, 3 searches, light): p50 648 ms, p99 670 ms, peak pinned 5,000. **2,500 indexes**, default pool: 809e9e p50 1,364 ms, p99 2,252 ms, peak pinned 100, 212 open after the first search (3,193 ms); the bisect's cb2a87 (Xapiand:0d97b2da7, its own build) p50 884 ms, p99 1,011 ms. At pool 5,000: d63930 p50 132 ms, p99 143 ms, peak pinned 2,500 (bisect bf8f29: 132 ms, 137 ms). The two default-pool runs are the same code: `git diff 0d97b2da7 d53d85c22` over `src`, CMakeLists.txt, cmake, config headers and contrib is empty (strata 397c02d5 both), both builds are RelWithDebInfo, LTO off, frame pointers, and the harness lines that differ (two `/:metrics` reads, the SR3 lines) run outside the timed searches. So the 1.54x is not code. The logs record no host load, so load is the remaining cause, not a measured one: what they show is that the pool-5,000 arm, which reopens nothing, repeats the bisect to the millisecond (132 against 132 ms, at 18:53; 809e9e ran at 19:21), while the default-pool arm, which reopens about 2,300 indexes a search through the file system, moved, and 809e9e's HTTP loops stalled 49-100 ms in its window. Cause of the regression from 98 ms: the strata bump Xapiand:55741956f; strata 397c02d5 (pinned by Xapiand:b6a25cf18) removes the per-open device flush; the rest is reopening past a 200-index pool (owner D89: quests pool-from-memory, cheap-reopen). Bisect: `docs/perf/a16-many-index-regression.md`. **SR3**, each loop's lag over the timed searches only (the window's highest histogram bucket): 809e9e http0 p99 384 us, p99.9 16,384 us, max 49-57 ms; http1 p99 384 us, p99.9 6,144 us, max 98-100 ms; control max 2,048 us. d63930 (pool 5,000, no reopens): http0 and http1 p99.9 115-131 ms. e5baa9 (10,000): http0 and http1 max up to 328-393 ms | 10,000-index latency and peak pinned measured (reported, no target); SR3 **not closed**: a wide search stalls an HTTP loop 49-100 ms at 2,500 indexes and up to 393 ms at 10,000, also with nothing to reopen. Which part of the search holds the loop (the per-name loop of `resolve_index_endpoints` that SR3 names, or the per-index work) is not attributed; the yield is a `src` change outside this quest |
| Owed async rows decided (D4 to D7), `--strict --owed` gates | build-test 4bde4b; rows f95555 `t1_strict_owed` | PASS |
| D16 restated to the code's order; an index that cannot close in 35 s exits 75 | `docs/design/phase7c.md` D16 | done (reviewer checked it against `src/manager.cc`; N8 open) |
| X6: one stop bound; `--committers`, `num_committers`, `DatabaseCleanup` gone | removed 905bcf; rows f95555: `x6_shutdown` | PASS |
| X3-1 C3: the non-allocating `claimed()`; admission at `readers_per_index` | build-test 4bde4b; `StrataReaders` is gone since x-pool (Xapiand:d43d00046): the cap is strata's reader records (`src/database/pool.cc`, `host.readers_per_index`) | done at master's code |
| One shape everywhere: CPU and p99 per request (Linux, macOS), allocations and handlers | a21 (macOS): CPU per light request after/before 0.958 (0.930-1.019), p99 0.962 (0.960-0.972), within D62 (over 5% CPU or 10% p99 fails). Allocations and asio handlers per light request, 261111-200223-9495ee (a DYLD malloc interposer and asio handler tracking, ABBA x3): allocations 0.988 (0.987-1.005), bytes 1.000 (0.997-1.000), handlers 0.986 (0.967-1.000); about 1,000 allocations, 1.6 MB and 68 handlers per request on both. Linux (strata's VM, b6b8f7534 against 927acf570, ABBA x3), 261113-111904-2e33d9: CPU per light request 1.008 (0.991-1.017), p99 1.001 (0.989-1.082), within D62. Fixed-commit A/Bs: the code they compare does not move with master | PASS (macOS and Linux) |
| X3-4: `Shard::view()` at 1 and 8 threads | quest x7-view-bench (landed), 261111-175937-76ae57 (strict, `docs/perf/shard-view.md`): median 18.6 ns per call at 1 thread, 20.3 ns (one shared shard) and 21.4 ns (a handle each) at 8; p90 29.8 against 18.9 ns; 52.5 against 298 Mcalls/s in total (5.7x on 8 threads). macOS x86_64; no concurrent writer | measured |
| T4's treatment of the control loop's idle probe | D53: read only while busy | decided |
| Perf: the sliced-search residual and the watch byte's read before T12 | master pins prospect 850d4e8 (contains prospect:5c98f81) and http:51d153a (Xapiand:1596eeed7); T12's jobs ran at a master pinning both | settled |
