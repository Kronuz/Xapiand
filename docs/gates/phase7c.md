# Gate 7c: Xapiand that never blocks a reactor

> Status: DRAFT (2026-11-11) the functional rows pass on macOS. T4, T5, T10's flood and T12's RESTORE miss their targets. T11
> and T12's QPS meet theirs. Linux and FreeBSD are owed.

Rows are `docs/design/phase7c.md` section 4. The code under test is Xapiand:0cec18474: master at Xapiand:78716404c
plus this quest's harness changes (`harness/stall_load.py`, `gate7c_rows.sh`, `wide_search_check.py`,
`test_gate_tools.py`); master merged since adds no engine change the jobs did not run. Functional rows run as `light`
jobs on this host (macOS x86_64, 16 logical CPUs). The timing rows (T4, T5, T11, T12) are the lead's, on master under
`exclusive`: five interleaved trials, a 95% bootstrap interval, smoke first.

Jobs, functional (Xapiand:49ebb4481, the head's code; light holds):

- **rows** 261111-171145-b4549d: a Release and an ASan+UBSan build, `ctest` under ASan+UBSan, then every row of
  `gate7c_rows.sh` (`rows/rows.tsv` in the job's output). The run before it, 261111-142844-048b3b at Xapiand:0cec18474,
  passed 14 of 14 rows and 37 of 37 tests.
- **build-test** 261111-171146-002593, **e2e** 261111-142834-e6b01e, **t7-tsan** 261111-142834-da0a9e,
  **recovery** 261111-142835-bb67a2, **removed** 261111-142835-15e20c: all pass.
- **x3-5** 261111-165347-d979c7: per-open-index resident cost and a 10,000-index search (light hold).
- **CI** run 42820866169 at Xapiand:ae2ea6c9b: ASan+UBSan and TSan on Linux (smoke, load, docs E2E, TLS); the
  quest's branch is pushed for its own run.

Jobs, timing (D59: one job of at most 10 minutes each, holding ABBA pairs; head Xapiand:49ebb4481 against pre-7c
Xapiand:142b2ca77, both RelWithDebInfo with frame pointers; host macOS 15, Intel Core i9-9980HK, 8 cores, 16 logical
CPUs; 16 reactors on head, 10 on pre-7c, each binary's default). Sizes are 20,000 accounts, not the design's
1,000,000, which takes about 5 minutes per arm to load; the light rate is 200/s open loop on both arms.
Ratios are head over pre-7c, the median of the pairs with their range.

- **t45** 261111-171041-c9ab68 (exclusive): `stall_load.py --phase both` pre, cur, cur, pre; a same-index run on cur.
- **t11** 261111-171043-7a746f (exclusive): `profile_light.sh` (20 s), pre-7c's trace/hop build against head, 3 pairs.
- **t12q** 261111-171045-06e893 and **t12b** 261111-171048-c39d8d (exclusive): `loadtest.py` (RESTORE of 20,000 and
  16 clients for 8 s) and `profile_bulk.sh --mult 20`, 3 pairs each.
- **t10** 261111-171050-a82ab4 (strict): `lua_bounds.py --loops 4` and the loaded phase beside 4 looping scripts.
- **a21** 261111-171055-51a31b (exclusive): before (Xapiand:b6b8f7534) and after (Xapiand:927acf570) the one-shape
  wave, unloaded light, 3 pairs.

## Rows

| Row | Target | Evidence | Status |
|---|---|---|---|
| T1 no offload pool | Census: section 2's threads only; `--strict` exits 0 | rows: `t1_strict_owed`; `t1_census` under load: 16 unnamed (the reactors), 16 `W`, 9 `FS`, 8 `WL`, `LOG`, main; 51 threads, none outside section 2 | PASS on macOS (Linux `/proc` census: C5) |
| T2 E2E on strata | 0 regressions, 0 guard hits, 0 ASan reports; `stored` at 2x the cap and just under it byte-equal | rows: `t2_e2e_asan`, `t2_stored_asan`, `t2_restore_stream_asan` (info); CI ASan+UBSan; e2e | PASS (macOS ASan+UBSan, Linux CI) |
| T3 recovery | `wal_recovery_check.sh`, `engine_crash_check.sh --reactors 2` | recovery; rows: `t3_crash`, `t3_crash_asan` | PASS |
| T4 reactor stall | p99 lag at most 1,024 us on every loop under the mixed load; p99.9, max; a script-heavy row (X4-R1) | t45: loaded (64 heavy clients, a RESTORE stream), every one of the 16 HTTP loops over the edge in both head arms: worst p99 24,576 and 28,672 us, p99.9 49,152 and 57,344 us, max 56.9 and 71.1 ms. Pre-7c kept its HTTP loops under the edge (worst loop is control, 2,048-2,560 us). Script-heavy (t10, 4 looping scripts): worst p99 24,576 us | MISS, 24-28x the edge |
| T5 light-query p99 | Loaded at most 2x unloaded + 1 ms and below pre-7c; same-index run as Q6's cost | t45: head unloaded p99 3.96 ms (limit 8.93 ms), loaded p99 76.8 ms; pre-7c loaded p99 395.3 ms; head/pre 0.194 (0.193-0.195); unloaded head/pre 0.147. Same index (RESTORE into `accounts`): p99 98.2 ms | MISS on the first half (8.6x its limit); met on the second (5.1x lower than pre-7c) |
| T6 reactor faults | Major faults per 1,000 queries at 2x RAM, reported | Linux VM; `stall_load.py --index-size 2xram` does not build the index yet | owed |
| T7 concurrency, shutdown | 0 TSan, 0 ASan; dump equals the model; SIGTERM at 300 s exits 0, writes durable | t7-tsan; rows: `t7_asan` (108,462 model documents, dump equal, 0 sanitizer reports) | PASS |
| T8 overload | Non-zero 503s in both overloads; peaks; RSS within 10% of T4's; 16-record registry gives 503s | rows: 6 held rounds each of 2,500 against 2 x 1,000; every round admitted 2,000 and refused 500 (in-flight 503); admitted searches past the reader registry got `Too many searches are reading at once` 503s (light 9,919, heavy 11,690); 0 other errors. Peaks: in-flight 1,000 (the cap), demoted 0 light, 23 heavy (cap 256 not reached). RSS 424 and 436 MB at 20,000 documents and 2 reactors; T4's run (16 reactors) reads 675-747 MB, so the 10% comparison needs both at one reactor count. build-test: `test_readers_cap` | PASS but RSS within 10% of T4's: not measured comparably |
| T9 cancellation | A disconnected read frees its reactor in one slice; a half-closed write is durable | build-test: `test_a_put_whose_client_half_closes_completes_durably` (`tests/test_node.cc:906`); `xapiand_http_requests_cancelled` rises on a cancelled read (`src/server/search_views.cc:3234`); no test disconnects a heavy search (quest x7-cancel-rows) | owed |
| T10 Lua bounds | Every case 4xx; T4 holds with four loops; 16-client flood keeps a light GET's p99 near 20 ms | rows: every case passes on Release and ASan. t10 (strict): light GET p99 0.83 ms alone, 0.62 ms beside 4 scripts, 0.83 ms beside 4 library loops, 50.94 ms (max 56.15 ms) beside the 16-client flood; T4 beside 4 looping scripts misses as T4 does (worst p99 24,576 us) | functional PASS; flood MISS (2.5x the 20 ms); T4 with loops MISS |
| T11 dispatch cost | At most half the offload hop's, CPU and wall | t11: dispatch CPU 0.0017 ms per request against the hop's 0.0188 ms, head/pre 0.073 (0.073-0.098); wall: the hop's median 25.05 us is gone (head has no hop) | PASS (13.7x less CPU) |
| T12 no throughput loss | RESTORE docs/s and saturated QPS at least 1.0x pre-7c, CPU count stated | t12q: RESTORE 2,124 against 7,413 docs/s, head/pre 0.288 (0.279-0.305); QPS 3,925 against 1,510, 2.599 (2.371-2.617), p99 0.351. t12b: RESTORE 0.284 (0.274-0.284) while CPU per document is 0.810 of pre-7c's. 16 logical CPUs | RESTORE MISS (3.5x slower); QPS PASS (2.6x) |
| T13 every gate | ASan, UBSan, TSan; coverage at the targets; restore corpora; per-open-index cost at 10,000 | rows (ASan+UBSan `ctest`); t7-tsan; CI; x3-5: 10,001 open indexes, resident 10.1 to 353.3 MiB, 35.1 KiB per open index | partial: coverage and corpora wait on x7-coverage (restore bug: quest restore-mixed-sign-integers); the reader registry's bytes owed |

## Carried conditions

| Condition | Evidence | Status |
|---|---|---|
| C5: Linux and FreeBSD functional rows (arm64 before 7b) | Linux VM; FreeBSD has no functional CI job | owed |
| Wide searches: 10,000-index latency and peak pinned views; SR3 with `stall.py` | x3-5 (light hold): 10,000 indexes, 3 searches, p50 686 ms, p99 694 ms, peak pinned 5,000; a strict run at 2,500 indexes is 261111-200215-2567dc (10,000 take 1,233 s to create, past D59's 10 minutes) | measured at 10,000 (light); strict pending; SR3 with `stall.py` owed |
| Owed async rows decided (D4 to D7), `--strict --owed` gates | build-test; rows `t1_strict_owed` | PASS |
| D16 restated to the code's order; an index that cannot close in 35 s exits 75 | `docs/design/phase7c.md` D16 | done (reviewer checks it against `src/manager.cc`) |
| X6: one stop bound; `--committers`, `num_committers`, `DatabaseCleanup` gone | removed; rows: `x6_shutdown` | PASS |
| X3-1 C3: the non-allocating `claimed()`; admission at `readers_per_index` | build-test; `StrataReaders` is gone since x-pool (Xapiand:d43d00046): the cap is strata's reader records (`src/database/pool.cc`, `host.readers_per_index`) | done at master's code |
| One shape everywhere: CPU and p99 per request (Linux, macOS), allocations and handlers | a21 (macOS): CPU per light request after/before 0.958 (0.930-1.019), p99 0.962 (0.960-0.972), within D62 (over 5% CPU or 10% p99 fails). Allocations and asio handlers per request: 261111-200223-9495ee (pending) | macOS CPU and p99 PASS; allocations, handlers pending; Linux owed (VM) |
| X3-4: `Shard::view()` at 1 and 8 threads | no benchmark exists | owed |
| T4's treatment of the control loop's idle probe | D53: read only while busy | decided |
| Perf: the sliced-search residual and the watch byte's read before T12 | master pins prospect 850d4e8 (contains prospect:5c98f81) and http:51d153a (Xapiand:1596eeed7) | owed: T12's jobs (lead) |
