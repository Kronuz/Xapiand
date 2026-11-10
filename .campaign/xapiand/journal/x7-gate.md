# x7-gate: journal

State: waiting on jobs 261110-052957-f5e762 261110-052925-7da5aa 261110-052925-5d7e5b 261110-052925-b5f523
Quest: .campaign/xapiand/quests/x7-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Launch 1 (inventory, functional jobs, report skeleton) done; four jobs queued (light queue ~46 deep).

## Next

1. When the jobs end: read `rows/rows.tsv`, `ctest-asan.log` (job 261110-052957-f5e762) and the three checks; fill the
   rows in `docs/gates/phase7c.md` with results; a failing row gets a rerun at master's commit before it is called
   the branch's or pre-existing.
2. Harnesses under ~100 lines, then their jobs: `harness/coverage.sh` (A13), the T9 half-close PUT check (A9's durable
   half), per-open-index RSS at 10,000 indexes in `wide_search_check.py gate` (A13's X3-5).
3. The lead's timing rows (commands below) after write-path-fixes and docs-site-refresh land.

## Inventory (launch 1, master Xapiand:ddaca9d01)

Prior evidence predates x-pool (2026-11-09) unless a job id is named, so every row reruns at master's code.

| Crit. | On master already | Still owed |
|---|---|---|
| A1 T1 | X6 census 2026-11-02 (macOS, pre-x-pool); `--strict --owed` in build-test 261110-032054-0d5177 | census at master: rows `t1_census`; Linux `/proc` census (A15) |
| A2 T2 | e2e check 261110-032054-4e0406; CI 42820866169 ASan+UBSan E2E; Linux 2026-11-03 smoke (pre-x-pool) | ASan E2E + `stored_stream.sh` on macOS: rows `t2_*` |
| A3 T3 | X3 ledger 2026-11-01 (D: 2 writers, 6,714 prefix) | recovery 261110-052925-5d7e5b; rows `t3_crash`, `t3_crash_asan` |
| A4 T4 | pre-7c baseline only (`docs/perf/phase7c-baseline.md`) | lead, exclusive; A23 first |
| A5 T5 | pre-7c baseline only | lead, exclusive |
| A6 T6 | none; `stall_load.py --index-size 2xram` is a stub ("not run") | Linux VM + the 2xram load (harness gap) |
| A7 T7 | X6 mixed load 2026-11-02 (17,148 docs, 0 reports) | t7-tsan 261110-052925-7da5aa; rows `t7_asan` |
| A8 T8 | pre-7c: 0 503s (offload window never reached); `test_readers_cap` in build-test | rows `t8_overload_*` (dry-run size); RSS against T4's run (lead) |
| A9 T9 | cancelled turn/pin tests in build-test (`test_reactor_requests`) | no disconnect or half-close test; no cancel counter in `:metrics` (Blocked on) |
| A10 T10 | X4-R1 2026-11-02 (33 cases, flood p99 57.95 ms); Linux ASan failed the 50 ms light GET | rows `t10_lua`, `t10_lua_asan`; T4 with loops (lead) |
| A11 T11 | `docs/perf/t11/`, closing baseline 2026-11-04 (idle memory 1.28x REGRESSION) | lead, exclusive |
| A12 T12 | Linux 2026-11-03 smoke: RESTORE 0.18x, QPS 1.22x | lead, exclusive, after A24's repin |
| A13 T13 | CI 42820866169 (ASan+UBSan, TSan: smoke, load, E2E, TLS); 7a coverage (2026-10-28) | ASan+UBSan ctest (rows job); coverage harness; restore corpora; per-open-index cost (X3-5) |
| A14 | none | `docs/gates/phase7c.md` (skeleton, this launch) |
| A15 C5 | Linux 2026-11-03 AWS smoke (pre-x-pool) | Linux VM jobs; FreeBSD (no functional CI job: `freebsd.yml` builds release tags only) |
| A16 | wide-search ledgers: `check` passes; gate run never made; security re-check open | lead, strict: `wide_search_check.py gate`; SR3 with `stall.py` |
| A17 | phase7c-gate-prep (owed 0); build-test 261110-032054-0d5177 | met |
| A18 | `docs/design/phase7c.md` D16 (lines 383-415), matches `src/manager.cc` Stop 1-5 and `_Exit(EX_TEMPFAIL)` | met (path) |
| A19 | X6 ledger; removed_options in build-test | removed 261110-052925-b5f523; rows `x6_shutdown` |
| A20 | pins-2026-11 C3; `StrataReaders` deleted by x-pool (Xapiand:d43d00046), cap now strata's reader records (`src/database/pool.cc:522`) | met at master's code (build-test) |
| A21 | perf-watch-peek A/B (http, macOS) | owner's threshold (PLAN q7); Linux run |
| A22 | none (`Shard::view()` in `src/engine/strata/shard.h:120`) | a benchmark in `tests/` (outside the file set) |
| A23 | S1 finding 1 | decision (Blocked on) |
| A24 | prospect:5c98f81 is in prospect pin 850d4e8 | http:51d153a is NOT in Xapiand's http pin c813531 (Blocked on) |

## Timing commands (the lead's; not run)

Builds, once (light): slot `x7-timing`, refs `pre=142b2ca77` (pre-7c baseline) and `hop=/Users/kronuz/code/Kronuz/http@10c5074`.

```sh
campaign job submit --quest x7-gate --level light --minutes 150 --slot x7-timing --ref pre=142b2ca77 \
  --ref hop=/Users/kronuz/code/Kronuz/http@10c5074 --what "x7-gate timing builds" -- bash -c '
F=-fno-omit-frame-pointer; B="-DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF"
cmake -S . -B build-cur -DFETCHCONTENT_BASE_DIR="$PWD/.deps" $B -DCMAKE_CXX_FLAGS=$F && cmake --build build-cur -j4 --target xapiand &&
cmake -S "$JOB_REF_PRE" -B build-pre $B -DCMAKE_CXX_FLAGS=$F && cmake --build build-pre -j4 --target xapiand &&
cmake -S "$JOB_REF_PRE" -B build-pre-hop $B "-DCMAKE_CXX_FLAGS=$F -DHTTP_TRACE_HOP" -DFETCHCONTENT_SOURCE_DIR_HTTP_LIB="$JOB_REF_HOP" &&
cmake --build build-pre-hop -j4 --target xapiand'
```

Smoke (strict, one per arm, `--after BUILD`, same slot): `harness/baseline_7c.sh build-ARM/bin/xapiand TRACE
"$JOB_OUT/smoke-ARM" --accounts-docs 20000 --stall-secs 5`, TRACE = `build-pre-hop/bin/xapiand` for pre and
`build-cur/bin/xapiand` for cur. It prints each step's seconds: if cur's projected 1,000,000-document run passes 25
minutes (RESTORE at 1,800 docs/s is about 9 minutes of load), split T12 into its own jobs.

Confirm (exclusive, ten jobs, one arm each, order pre cur cur pre pre cur cur pre pre cur, each `--after` the smoke):
`harness/baseline_7c.sh build-ARM/bin/xapiand TRACE "$JOB_OUT/ARM-N"`: T4, T5, T8 and the census (`stall_load.py
--suite --census`, 1,000,000 accounts, 30 s phases), T11 (`profile_light.sh TRACE OUT --secs 20`), T12
(`profile_bulk.sh --mult 20` and `loadtest.py` 16 clients x 3 trials). Then `python3 harness/baseline_report.py DIR`
per run, `harness/profile_light.sh --compare PRE.json POST.json` for T11, and the bootstrap over per-run medians.

- T10's timing half: `python3 harness/lua_bounds.py build-cur/bin/xapiand --loops 4 --out "$JOB_OUT/t10.json"` (strict).
- A16: `python3 harness/wide_search_check.py gate build-cur/bin/xapiand "$JOB_OUT/wide" 18970 --indexes 10000 --runs 30` (strict).
- A24: the T12 jobs only at a master whose http pin contains http:51d153a.

## Blocked on

1. **A23, the lead's decision: T4 and the control loop's 100 ms idle probe** (kqueue coalescing makes the idle tick
   about 1.5 ms late, p99 2,560 us idle: `docs/perf/phase7c-baseline.md`).
   - a. Probe the control loop at 1 ms always: one line; one more loop waking 1,000 times a second on every idle node
     (on kqueue each tick writes the wake pipe and runs one extra iteration); idle CPU unmeasured on master.
   - b. T4 reads the control loop only while busy: `Tasks::spawn` calls `note_activity()` (today it does not:
     `docs/mechanisms/control-plane.md` Limits), and the lag probe keeps idle-interval ticks out of the gated
     histogram (a reactor library change); no idle cost.
   - c. Gate the control loop on lateness past its own interval (lag minus the platform's coalescing slack): no
     code, but the bound differs per platform and hides a real 1.5 ms block.
   Recommendation: b. T4 asks whether work blocks a loop; an idle timer's lateness measures the OS, and (a) puts a
   1 kHz wake on every idle node of the many-indexes host. If the lead wants no library change before the gate, (a)
   with its idle CPU measured in T4's jobs.
2. **A24: Xapiand's http pin (http:c813531) lacks perf-watch-peek (http:51d153a; E2 at http:6ec7324).** A repin in
   `CMakeLists.txt` (outside this file set) must land before T12's jobs.
3. **A9/T9: no harness and no counter.** No test disconnects a heavy search after 50 ms or half-closes a PUT, and
   `:metrics` exports no cancel counter (`grep cancel` in `src/` metrics: none). The counter is `src/` work for
   another quest; the half-close durability check fits a short harness here (Next 2).
4. **A22: `Shard::view()` at 1 and 8 threads needs a benchmark in `tests/`** (outside this file set): one index, N
   threads calling `view()` and dropping it, ns per call and its spread.
5. **A6 and A15: a Linux VM** (no AWS; Detent's VMs on 18762/18763 are not usable) for T6, the `/proc` census and the
   functional rows; and `stall_load.py --index-size 2xram` builds no index today (a harness change plus the VM).
   FreeBSD: `freebsd.yml` builds release tags only, so the functional rows need a FreeBSD job or VM.
6. **A21: the owner's threshold** (PLAN open question 7) before its jobs mean anything.
7. **A13 pieces:** "restore corpora replayed" names no corpus I can find in `harness/` or `docs/dev/testing.md`;
   the lead names it. Coverage and per-open-index cost are Next 2.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | rows 261110-052957-f5e762 (`t1_census`, `t1_strict_owed`) queued; build-test 261110-032054-0d5177 |
| A2 | rows 261110-052957-f5e762 (`t2_e2e_asan`, `t2_stored_asan`) queued; e2e 261110-032054-4e0406; CI run 42820866169 |
| A3 | recovery 261110-052925-5d7e5b and rows 261110-052957-f5e762 (`t3_crash*`) queued |
| A7 | t7-tsan 261110-052925-7da5aa and rows 261110-052957-f5e762 (`t7_asan`) queued |
| A8 | rows 261110-052957-f5e762 (`t8_overload_*`) queued; build-test 261110-032054-0d5177 (`test_readers_cap`) |
| A10 | rows 261110-052957-f5e762 (`t10_lua`) queued |
| A13 | rows 261110-052957-f5e762 (ASan+UBSan ctest) and t7-tsan queued; CI run 42820866169 |
| A14 | `docs/gates/phase7c.md` (skeleton) |
| A17 | build-test 261110-032054-0d5177 (`async_check.sh --strict --owed`) |
| A18 | `docs/design/phase7c.md` (D16) |
| A19 | removed 261110-052925-b5f523 and rows 261110-052957-f5e762 (`x6_shutdown`) queued |
| A20 | build-test 261110-032054-0d5177; `src/database/pool.cc` |

## Result

Not yet. Launch 1: `harness/gate7c_rows.sh` (Xapiand:e7e8e03c5), `docs/gates/phase7c.md` skeleton.

## Rulings I made

- R1. The checks were verified at Xapiand:a26d6b856 (`campaign verify --commit`), master's code, not at the head:
  the head adds `harness/gate7c_rows.sh`, which would rerun build-test and e2e too (six jobs past the lead's four).
  The landing verify at the final head reruns them.
- R2. One rows job builds Release (asserts off: the CI note on a restore-path assertion under `-DASSERTS=ON`) and
  ASan+UBSan, runs `ctest` under ASan, then `gate7c_rows.sh`; rows run to their end so one failure hides no other.
  `t2_restore_stream_asan` and `t10_lua_asan` are info rows: the first has the known kill-before-commit timing
  (x-pool R6), the second's 50 ms light-GET limit is a timing claim under ASan (Linux 2026-11-03).
- R3. T8 runs at dry-run size (20,000 documents): the row's functional claims are 503s and no other errors; RSS
  within 10% of T4's run needs the lead's 1,000,000-document run.
