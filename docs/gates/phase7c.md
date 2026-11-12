# Gate 7c: Xapiand that never blocks a reactor

> Status: DRAFT (2026-11-11) every functional row passes on macOS; the timing rows, Linux and FreeBSD are owed.

Rows are `docs/design/phase7c.md` section 4. The code under test is Xapiand:0cec18474: master at Xapiand:78716404c
plus this quest's harness changes (`harness/stall_load.py`, `gate7c_rows.sh`, `wide_search_check.py`,
`test_gate_tools.py`); master merged since adds no engine change the jobs did not run. Functional rows run as `light`
jobs on this host (macOS x86_64, 16 logical CPUs). The timing rows (T4, T5, T11, T12) are the lead's, on master under
`exclusive`: five interleaved trials, a 95% bootstrap interval, smoke first.

Jobs (all at Xapiand:0cec18474):

- **rows** 261111-142844-048b3b: a Release and an ASan+UBSan build, `ctest` under ASan+UBSan (37 of 37 pass), then
  every row of `gate7c_rows.sh` (`rows/rows.tsv` in the job's output): 14 of 14 rows pass.
- **build-test** 261111-162221-c4aa56 (rerun of 261111-142834-9c25fd, which failed 1 of 406 on a master timing
  flake in `test_pool`, `tests/test_pool.cc:2203`, under load about 50): passes.
- **e2e** 261111-142834-e6b01e, **t7-tsan** 261111-142834-da0a9e, **recovery** 261111-142835-bb67a2,
  **removed** 261111-142835-15e20c: all pass.
- **x3-5** 261111-165347-d979c7: per-open-index resident cost at 10,000 open indexes.
- **CI** run 42820866169 at Xapiand:ae2ea6c9b: ASan+UBSan and TSan on Linux (smoke, load, docs E2E, TLS); the
  quest's head needs its own CI run.

## Rows

| Row | Target | Evidence | Status |
|---|---|---|---|
| T1 no offload pool | Census: section 2's threads only; `--strict` exits 0 | rows: `t1_strict_owed`; `t1_census` under load: 16 unnamed (the reactors), 16 `W`, 9 `FS`, 8 `WL`, `LOG`, main; 51 threads, none outside section 2 | PASS on macOS (Linux `/proc` census: C5) |
| T2 E2E on strata | 0 regressions, 0 guard hits, 0 ASan reports; `stored` at 2x the cap and just under it byte-equal | rows: `t2_e2e_asan`, `t2_stored_asan`, `t2_restore_stream_asan` (info); CI ASan+UBSan; e2e | PASS (macOS ASan+UBSan, Linux CI) |
| T3 recovery | `wal_recovery_check.sh`, `engine_crash_check.sh --reactors 2` | recovery; rows: `t3_crash`, `t3_crash_asan` | PASS |
| T4 reactor stall | p99 lag at most 1,024 us on every loop under the mixed load; p99.9, max; a script-heavy row (X4-R1) | lead, exclusive | owed (the control loop read only while busy, D53) |
| T5 light-query p99 | Loaded at most 2x unloaded + 1 ms and below pre-7c; same-index run as Q6's cost | lead, exclusive | owed |
| T6 reactor faults | Major faults per 1,000 queries at 2x RAM, reported | Linux VM; `stall_load.py --index-size 2xram` does not build the index yet | owed |
| T7 concurrency, shutdown | 0 TSan, 0 ASan; dump equals the model; SIGTERM at 300 s exits 0, writes durable | t7-tsan; rows: `t7_asan` (108,462 model documents, dump equal, 0 sanitizer reports) | PASS |
| T8 overload | Non-zero 503s in both overloads; peaks; RSS within 10% of T4's; 16-record registry gives 503s | rows: 6 held rounds each of 2,500 against 2 x 1,000; every round admitted 2,000 and refused 500 (in-flight 503); admitted searches past the reader registry got `Too many searches are reading at once` 503s (light 9,919, heavy 11,690); 0 other errors. Peaks: in-flight 1,000 (the cap), demoted 0 light, 23 heavy (cap 256 not reached). RSS 424 and 436 MB at 20,000 documents. build-test: `test_readers_cap` | PASS (RSS against T4's run: lead) |
| T9 cancellation | A disconnected read frees its reactor in one slice; a half-closed write is durable | build-test: `test_a_put_whose_client_half_closes_completes_durably` (`tests/test_node.cc:906`); `xapiand_http_requests_cancelled` rises on a cancelled read (`src/server/search_views.cc:3234`); no test disconnects a heavy search (quest x7-cancel-rows) | owed |
| T10 Lua bounds | Every case 4xx; T4 holds with four loops; 16-client flood keeps a light GET's p99 near 20 ms | rows: `t10_lua` and `t10_lua_asan` pass every case; Release light GET p99 1.03 ms alone, 0.58 ms beside 4 scripts, 1.00 ms beside 4 library loops, 57.10 ms beside the 16-client flood (light hold) | PASS functional; the flood p99 against 20 ms and T4 with loops are the lead's timing |
| T11 dispatch cost | At most half the offload hop's, CPU and wall | lead, exclusive (`profile_light.sh --compare`) | owed |
| T12 no throughput loss | RESTORE docs/s and saturated QPS at least 1.0x pre-7c, CPU count stated | lead, exclusive (`profile_bulk.sh`, `loadtest.py`) | owed (needs http repinned: Perf) |
| T13 every gate | ASan, UBSan, TSan; coverage at the targets; restore corpora; per-open-index cost at 10,000 | rows (ASan+UBSan `ctest`, 37 of 37); t7-tsan; CI; x3-5 (pending) | partial: coverage and corpora wait on x7-coverage (restore bug: quest restore-mixed-sign-integers) |

## Carried conditions

| Condition | Evidence | Status |
|---|---|---|
| C5: Linux and FreeBSD functional rows (arm64 before 7b) | Linux VM; FreeBSD has no functional CI job | owed |
| Wide searches: 10,000-index latency and peak pinned views; SR3 with `stall.py` | `wide_search_check.py gate` under `strict` (lead) | owed |
| Owed async rows decided (D4 to D7), `--strict --owed` gates | build-test; rows `t1_strict_owed` | PASS |
| D16 restated to the code's order; an index that cannot close in 35 s exits 75 | `docs/design/phase7c.md` D16 | done (reviewer checks it against `src/manager.cc`) |
| X6: one stop bound; `--committers`, `num_committers`, `DatabaseCleanup` gone | removed; rows: `x6_shutdown` | PASS |
| X3-1 C3: the non-allocating `claimed()`; admission at `readers_per_index` | build-test; `StrataReaders` is gone since x-pool (Xapiand:d43d00046): the cap is strata's reader records (`src/database/pool.cc`, `host.readers_per_index`) | done at master's code |
| One shape everywhere: CPU and p99 per request (Linux, macOS), allocations and handlers | D62: fails over 5% CPU or 10% p99 after optimizing | owed |
| X3-4: `Shard::view()` at 1 and 8 threads | no benchmark exists | owed |
| T4's treatment of the control loop's idle probe | D53: read only while busy | decided |
| Perf: the sliced-search residual and the watch byte's read before T12 | master pins prospect 850d4e8 (contains prospect:5c98f81) and http:51d153a (Xapiand:1596eeed7) | owed: T12's jobs (lead) |
