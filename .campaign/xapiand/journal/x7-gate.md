# x7-gate: journal

State: waiting on jobs 261111-142834-9c25fd 261111-142834-e6b01e 261111-142834-da0a9e 261111-142835-bb67a2 261111-142835-15e20c 261111-142844-048b3b 261111-142846-66c8ef
Quest: .campaign/xapiand/quests/x7-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged master (Xapiand:78716404c) into the branch: it resolves Blocked on 1 (catalog timer on the control loop,
Xapiand:c0d0208ee), 2 (`xapiand_admission_inflight_peak`, `xapiand_scheduler_demoted_peak` in `src/metrics.cc`) and
4 (http repinned, Xapiand:1596eeed7). The T8 rows now gate on both peaks and name each 503's cause (R8). Every row and
check reruns at Xapiand:0cec18474: checks build-test 261111-142834-9c25fd, e2e 261111-142834-e6b01e, t7-tsan
261111-142834-da0a9e, recovery 261111-142835-bb67a2, removed 261111-142835-15e20c; rows 261111-142844-048b3b (every
row, ASan ctest); X3-5 per-open-index cost 261111-142846-66c8ef (after the rows job). The same jobs queued at
Xapiand:2abe48746 were cancelled before they ran: the X3-5 commit changed the code they would prove.

### Rows job 261110-052957-f5e762 (Xapiand:e7e8e03c5, master's code + driver), each failure read

| Row | rc | Cause | Kind |
|---|---|---|---|
| t1_census | 1 | `Xapiand:DB-TIMER` 1 beside section 2's list (unnamed 16 = reactors, W 16, FS 9, WL 8, LOG 1, main 1; total 52). It is the catalog's timer, a second scheduler wheel: `src/database/pool.cc:423` (`Wheel wheel{"DB-TIMER"}`), added by x-pool at Xapiand:d43d00046 | master: a thread section 2 does not list (Blocked on 1) |
| t8_overload_light | 1 | 0 503s, light p50 10.2 s at 4x of 3,200/s: `run_light` sends through a pool of `--light-conns` 64 (`stall_load.py` `pool.get()`), so the server never held more than 64 requests against an in-flight cap of 1,000 per reactor (`src/manager.cc:950`) x 16 | harness: fixed at Xapiand:e57e6ef0d |
| t8_overload_heavy | 1 | 0 503s; 64 heavy clients, same bound (demoted cap 256 per reactor, `src/server/search_views.cc:3071`) | harness: same fix |
| t8 (both) | - | peaks `inflight` and `demoted` null: `stall_load.py` reads `xapiand_admission_inflight_peak` and `xapiand_scheduler_demoted_peak`; `src/metrics.h` exports neither (only `xapiand_search_pinned_indexes*`, `:106-107`) | master: T8's "peaks reported" cannot be met (Blocked on 2) |
| t10_lua_asan | 1 (info) | client timeout at 30 s on "closing variable swallows the cap (pcall)": 8.32 s on Release, ASan is slower; `--max-bound-secs` default 30 | threshold under ASan: driver now checks answers with 180 s / 2 s / 5 s limits and gates the row (R5) |

Info from passing rows (not gate values; light hold, shared host): T1's dry-run loaded phase worst loop p99 lag
24,576 us (http9), light p99 160.8 ms; T10 Release flood light-GET p99 48.33 ms against the row's "near 20 ms"
(lua_bounds limit 100 ms). Both are T4/T10 timing for the lead's exclusive runs.

Rerun 261110-092614-ccc9e3 (3805-era runner fix resubmit of b8ca87): t8_overload_light rc 0, t8_overload_heavy rc 0,
t10_lua_asan rc 1: the ten concurrent swallowers hit the harness's fixed 120 s client timeout (TimeoutError; one alone
28-38 s under ASan; fresh GETs 20/20). Lead fixed the timeout at Xapiand:47015d7ff (four bounds each, times printed);
rerun 261110-103010-fc5f2d. Still timing out at 720 s would be a master finding.

Rerun 261110-103010-fc5f2d (Xapiand:47015d7ff): t10_lua_asan rc 0 (ten swallowers 400 in 144-148 s), t8_overload_heavy
rc 0 (5,807 heavy + 931 light 503s), t8_overload_light rc 1: 0 503s. Cause, from `stall_load.py`: the light overload
was open loop at 4x `--saturate`'s rate through a pool, so the server held rate x latency in flight. This run's
saturate step read 144/s (one p99 spike at 200/s: 48 ms over a 37 ms limit), so 576/s at p50 1.8 ms put about 1
request in flight against 1,000 x 2 reactors; ccc9e3 read 1,900/s, offered 7,600/s past the server's throughput and
queued (p50 3.2 s, 37,666 503s). The 503s depended on the measurement and host load. Heavy had the same shape (open
loop at 4x a measured closed rate). Fixed at Xapiand:94eaf3dd5 (R7): held rounds. Jobs 261110-123313-fe03a9 (T8
both + gate_tools, pass 1), 261110-123314-96a9e6 (T8 both, pass 2, after pass 1).

### T8 held overloads (harness count per round: in flight at the barrier / refused with 503 before it; held 2,500, cap 2,000)

| Job | Row | Rounds | In flight at barrier | Refused at barrier | 503s by reason | rc |
|---|---|---|---|---|---|---|
| 261110-123313-fe03a9 | light | 6 | 2,000 each | 500 each (p50 47 ms) | plain 3,000; JSON 11,465 (cause cut at 40 chars); 535 ok | 0 |
| 261110-123313-fe03a9 | heavy | 6 | 2,000 each | 500 each (p50 55 ms) | plain 3,000; JSON 11,771; 229 ok; beside-light 814 503s of 1,200 | 0 |
| 261110-123314-96a9e6 | light | 3 | 2,000 each | 500 each (p50 85 ms) | plain 1,500; JSON 5,856; 144 ok | 0 |
| 261110-123314-96a9e6 | heavy | 3 | 2,000 each | 500 each (p50 106 ms) | plain 1,500; JSON 5,898; 102 ok; beside-light 809 503s | 0 |

Read: every round enforced (2,000 admitted, 500 refused with the plain in-flight 503), 0 other errors, 0 exceptions;
peaks null (master then exported none); RSS 299-367 MB at 20,000 docs. Of the admitted requests about 96% were
answered by a JSON 503 (`http_utils.h:72`: engine `busy`/`not_available`/`reopen`, likely `pool.cc:351` "The index
is busy: its turn did not come in time"); the demoted cap's "The server is busy" never showed. The harness cut the
cause at 40 chars, so it is unnamed: fixed (R8), rerun in the rows job below.

## Next

1. Read rows 261111-142844-048b3b: `rows/rows.tsv`, `t1_census.log` (DB-TIMER gone?), `t8_*.log` (503 causes by
   name, both peaks), `ctest-asan.log`; read the five checks; fill Evidence for A1 A2 A3 A7 A8 A10 A13 A17 A19 A20.
2. Read 261111-142846-66c8ef's "KiB per open index" line into A13 (X3-5); state the reader registry's bytes
   (strata's reader records x `default_readers_per_index` 128, `src/engine/open.h:50`) from code.
3. A9 is quest x7-cancel-rows's (D53): the half-close test exists (`tests/test_node.cc:906`); the counter
   `xapiand_http_requests_cancelled` rises at `src/server/search_views.cc:3234`; no disconnected-search test yet.
4. Write each row's result into `docs/gates/phase7c.md` at the head's commit.
5. The lead's timing rows (commands below); A24's T12 jobs can run now (http pin 51d153a, prospect 850d4e8).

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

1. **Resolved on master** (Xapiand:c0d0208ee runs the catalog's timer on the control loop; census reruns in
   261111-142844-048b3b). Was: **T1 (A1): the catalog timer thread `Xapiand:DB-TIMER`.** x-pool runs strata's
   `host::timer` on its own `Scheduler` wheel (`src/database/pool.cc:414-432`), a second wheel beside the
   debouncer's. Options: (a) run the catalog's timer on the existing debouncer wheel (one timer mechanism, one
   thread fewer; `src/`, a quest); (b) amend section 2's thread list to name it. Recommendation: (a), one
   implementation per mechanism; T1 stays failing until then.
2. **Resolved on master** (`src/metrics.cc:208-217`, raised in `SearchService::create_extension`); the T8 rows
   gate on both now. Was: **T8 (A8): no admission peaks in `:metrics`.** Export `xapiand_admission_inflight_peak` (the
   reactor gate's `inflight`, `src/manager.cc:1209` reads it) and `xapiand_scheduler_demoted_peak` (the scheduler's
   `demoted()`), the names `stall_load.py` reads. `src/` work.
3. **T8 procedure, for the lead's ruling (R4):** the functional overload runs at `--processors 2`. At the default
   16 reactors the light overload needs about 20,000 connections to reach 1,000 in flight per reactor, past one
   loopback address's 16,384 ephemeral ports (49152-65535). The lead's 16-reactor suite would need several
   client addresses or a lower cap; the 503-within-a-slice claim does not depend on the reactor count.
4. **Resolved**: Xapiand pins http:51d153a (Xapiand:1596eeed7) and prospect 850d4e8 (contains prospect:5c98f81);
   A24's T12 jobs are the lead's timing runs.
5. **A6, A15**: the local Linux VM (D53); FreeBSD: `freebsd.yml` builds release tags only.
6. **A13**: "restore corpora replayed" names no corpus I can find; the lead names it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | rows 261110-052957-f5e762: `t1_strict_owed` passed, `t1_census` FAILED (DB-TIMER, Blocked on 1); build-test 261110-032054-0d5177 |
| A2 | rows 261110-052957-f5e762 (`t2_e2e_asan`, `t2_stored_asan` passed); e2e 261110-032054-4e0406; CI run 42820866169 |
| A3 | recovery 261110-052925-5d7e5b passed; rows 261110-052957-f5e762 (`t3_crash`, `t3_crash_asan` passed) |
| A7 | t7-tsan 261110-052925-7da5aa passed; rows 261110-052957-f5e762 (`t7_asan` passed, 365 s) |
| A8 | held overloads (R7, Xapiand:94eaf3dd5): 261110-123313-fe03a9, 261110-123314-96a9e6 passed (every round enforced, 0 other errors); at the head: 261111-142844-048b3b; peaks unexported (Blocked on 2); build-test 261110-032054-0d5177 (`test_readers_cap`) |
| A10 | rows 261110-052957-f5e762 (`t10_lua` passed; `t10_lua_asan` timed out at 30 s); rerun 261110-081308-b8ca87 queued |
| A13 | rows 261110-052957-f5e762 (ASan+UBSan ctest rc 0); t7-tsan 261110-052925-7da5aa; CI run 42820866169 |
| A14 | `docs/gates/phase7c.md` (skeleton) |
| A17 | build-test 261110-032054-0d5177 (`async_check.sh --strict --owed`) |
| A18 | `docs/design/phase7c.md` (D16) |
| A19 | removed 261110-052925-b5f523 passed; rows 261110-052957-f5e762 (`x6_shutdown` passed) |
| A20 | build-test 261110-032054-0d5177; `src/database/pool.cc` |
| A21 | D53 sets the rule (head inside the new run's 95% interval or above); jobs owed |
| A23 | D53 (T4 reads the control loop only while busy) |

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
- R4. T8's functional overload runs at `--processors 2` with 1.25x the in-flight cap in connections (Blocked on
  3 for the lead's 16-reactor runs).
- R5. Under ASan, T10 checks each bound's answer, not its time (`--max-bound-secs 180 --max-light-ms 2000
  --max-flood-p99-ms 5000`), so the row gates; the times are the Release row's and the lead's.
- R6. A grep for 503 paths printed one line of `src/sortable_serialise.cc` (a match, not a read); no other GPL
  path was opened. Later greps exclude it.
- R7. T8's overloads hold the cap's excess by construction, not by rate: `--overload light|heavy 1.25` sends rounds of
  2,500 request heads (single-term searches; K3's heavy searches beside light at `--rate`) with bodies withheld.
  The reactor gate admits at headers-complete and holds until the response (`http_asio.h` serve_connection_body),
  so at most 2,000 are admitted and at least 500 refused; the barrier opens once they are, and a round short of
  them fails the phase (`enforced`). The design's T8 target is unchanged; its procedure cell and K3 bullet now say
  so (`docs/design/phase7c.md`). Open-loop 4x stays out: rate x latency in flight is host-dependent. 503s are
  counted by body, so the in-flight cap ("Service Unavailable") and the demoted cap ("The server is busy") show
  apart. "503s within one slice of the cap" is not measured: the client's refusal latency is Python's, not the
  server's (recorded as info, `refused_at_barrier`).
- R8. T8 names each 503 by its cause: `stall_load.py` reads a JSON error's `message` (the engine's reason) instead of
  the body's first 40 characters, and the T8 rows fail when either admission peak is missing (test
  `test_a_refusal_reads_its_cause` in `gate_tools`).
- R9. X3-5's per-open-index cost is resident memory, measured under a light hold (memory does not depend on CPU
  share): `wide_search_check.py gate --pool 10000` divides the first search's RSS growth by the indexes it left open.
- R3. T8 runs at dry-run size (20,000 documents): the row's functional claims are 503s and no other errors; RSS
  within 10% of T4's run needs the lead's 1,000,000-document run.
