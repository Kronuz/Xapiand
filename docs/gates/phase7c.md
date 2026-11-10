# Gate 7c: Xapiand that never blocks a reactor

> Status: DRAFT (2026-11-10) the functional rows' first run is in; the timing rows are owed.

Rows are `docs/design/phase7c.md` section 4. The code under test is Xapiand master at Xapiand:ddaca9d01 (the same
code as Xapiand:ae2ea6c9b and the quest's start, Xapiand:a26d6b856; they differ only in campaign files). Functional
rows run as `light` jobs on this host (macOS x86_64, 16 logical CPUs); the functional driver
`harness/gate7c_rows.sh` runs at Xapiand:e7e8e03c5, which adds only that driver to master's code. The timing rows
(T4, T5, T11, T12) are the lead's, on master under `exclusive`: five interleaved trials, a 95% bootstrap interval,
smoke first.

Jobs:

- **rows** 261110-052957-f5e762: a Release and an ASan+UBSan build, `ctest` under ASan+UBSan, then
  `gate7c_rows.sh` (`rows/rows.tsv` in the job's output).
- **build-test** 261110-032054-0d5177 (reused at master's code): Release with asserts, `ctest`,
  `async_check.sh --strict --owed`, `removed_options_check.sh`.
- **e2e** 261110-032054-4e0406 (reused): the docs E2E, plain and over TLS, strict.
- **t7-tsan** 261110-052925-7da5aa, **recovery** 261110-052925-5d7e5b, **removed** 261110-052925-b5f523: all pass.
- **rows2** 261110-081308-b8ca87 at Xapiand:e57e6ef0d: T8 and T10-under-ASan again, after the T8 harness fix.
- **CI** run 42820866169 at Xapiand:ae2ea6c9b: ASan+UBSan and TSan on Linux (smoke, load, docs E2E, TLS).

## Rows

| Row | Target | Evidence | Status |
|---|---|---|---|
| T1 no offload pool | Census: section 2's threads only; `--strict` exits 0 | rows: `t1_strict_owed` passes; `t1_census` finds `Xapiand:DB-TIMER` beside section 2's list (16 HTTP loops, 16 `W`, 9 `FS`, 8 `WL`, `LOG`, main; 52 threads): the catalog's own timer wheel, `src/database/pool.cc:423` | FAIL (Linux `/proc` census owed: C5) |
| T2 E2E on strata | 0 regressions, 0 guard hits, 0 ASan reports; `stored` at 2x the cap and just under it byte-equal | rows: `t2_e2e_asan`, `t2_stored_asan`; CI ASan+UBSan; e2e | PASS (macOS ASan+UBSan, Linux CI) |
| T3 recovery | `wal_recovery_check.sh`, `engine_crash_check.sh --reactors 2` | recovery; rows: `t3_crash`, `t3_crash_asan` | PASS |
| T4 reactor stall | p99 lag at most 1,024 us on every loop under the mixed load; p99.9, max; a script-heavy row (X4-R1) | lead, exclusive | owed (the control loop read only while busy, D53) |
| T5 light-query p99 | Loaded at most 2x unloaded + 1 ms and below pre-7c; same-index run as Q6's cost | lead, exclusive | owed |
| T6 reactor faults | Major faults per 1,000 queries at 2x RAM, reported | Linux VM; `stall_load.py --index-size 2xram` does not build the index yet | owed |
| T7 concurrency, shutdown | 0 TSan, 0 ASan; dump equals the model; SIGTERM at 300 s exits 0, writes durable | t7-tsan; rows: `t7_asan` | PASS |
| T8 overload | Non-zero 503s in both overloads; peaks; RSS within 10% of T4's; 16-record registry gives 503s | rows: 0 503s in both, because the harness sent through 64 connections (fixed, Xapiand:e57e6ef0d; rows2 at two reactors); in-flight and demoted peaks are not exported by `:metrics`; build-test (`test_readers_cap`) | FAIL (peaks: master; RSS against T4 owed) |
| T9 cancellation | A disconnected read frees its reactor in one slice; a half-closed write is durable | build-test covers cancelled turns and pins (`test_reactor_requests`); no test disconnects a heavy search or half-closes a PUT, and `:metrics` has no cancel counter | owed |
| T10 Lua bounds | Every case 4xx; T4 holds with four loops; 16-client flood keeps a light GET's p99 near 20 ms | rows: `t10_lua` passes (flood light-GET p99 48.33 ms, light hold); `t10_lua_asan` hit the client's 30 s limit on one case (rows2 checks answers only) | PASS on Release (T4 with loops and the 20 ms flood claim are timing) |
| T11 dispatch cost | At most half the offload hop's, CPU and wall | lead, exclusive (`profile_light.sh --compare`) | owed |
| T12 no throughput loss | RESTORE docs/s and saturated QPS at least 1.0x pre-7c, CPU count stated | lead, exclusive (`profile_bulk.sh`, `loadtest.py`) | owed (needs http repinned: Perf) |
| T13 every gate | ASan, UBSan, TSan; coverage at the targets; restore corpora; per-open-index cost at 10,000 | rows (ASan+UBSan `ctest` passes); t7-tsan; CI | partial: coverage, corpora and per-index cost owed |

## Carried conditions

| Condition | Evidence | Status |
|---|---|---|
| C5: Linux and FreeBSD functional rows (arm64 before 7b) | Linux VM; FreeBSD has no functional CI job | owed |
| Wide searches: 10,000-index latency and peak pinned views; SR3 with `stall.py` | `wide_search_check.py gate` under `strict` (lead) | owed |
| Owed async rows decided (D4 to D7), `--strict --owed` gates | build-test (`async_check.sh --strict --owed`) | done at master's code |
| D16 restated to the code's order; an index that cannot close in 35 s exits 75 | `docs/design/phase7c.md` D16 | done (reviewer checks it against `src/manager.cc`) |
| X6: one stop bound; `--committers`, `num_committers`, `DatabaseCleanup` gone | removed; rows: `x6_shutdown` | PASS |
| X3-1 C3: the non-allocating `claimed()`; admission at `readers_per_index` | build-test; `StrataReaders` is gone since x-pool (Xapiand:d43d00046): the cap is strata's reader records (`src/database/pool.cc`, `host.readers_per_index`) | done at master's code |
| One shape everywhere: CPU and p99 per request (Linux, macOS), allocations and handlers | rule D53: head inside the new run's 95% interval or above | owed |
| X3-4: `Shard::view()` at 1 and 8 threads | no benchmark exists | owed |
| T4's treatment of the control loop's idle probe | D53: read only while busy | decided |
| Perf: the sliced-search residual and the watch byte's read before T12 | prospect:5c98f81 is in master's prospect pin; http:51d153a is not in master's http pin (http:c813531) | owed |
