# x7-gate: journal

State: waiting on jobs 261113-111748-e52352 261113-111801-2b8264 261113-111808-93efd0 261113-111813-3cf199 261113-111813-3013ac 261113-111814-1a49cf 261113-111814-f9dc7f 261113-111814-e12ef2 261113-111821-2f9e75 261113-111826-184241 261113-111855-8bc27e 261113-111904-2e33d9 261113-112020-7142fc 261113-112044-32612d 261113-112052-f5a8e0 261113-112059-d3b9ff
Quest: .campaign/xapiand/quests/x7-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 2026-11-13 (master 22d096502 merged: x7-loop-fix-c, loop-scheduling, schema-load-once, one-id). Code under test:
Xapiand:0a0321691 (master Xapiand:6ede7a0ce's `src` + harness), Xapiand:b5499421b for a16 (SR3 lag in
`wide_search_check.py`). Every functional and timing row reruns there:

| Job | Level | Criteria | What |
|---|---|---|---|
| 261113-111748-e52352 | light | builds | timing builds (rerun of 19268b) |
| 261113-111801-2b8264 | exclusive | A4 A5 | T4/T5 ABBA (c9ab68's command), after e52352 |
| 261113-112044-32612d, 261113-112052-f5a8e0 | exclusive | A12 A24 | T12 loadtest, profile_bulk (fix (c) changed RESTORE) |
| 261113-112059-d3b9ff | strict | A10 | T10 flood, T4 with loops |
| 261113-112020-7142fc | strict | A16 | 2,500 indexes + SR3 per-loop lag |
| 261113-111808-93efd0 | light | A1 A2 A3 A7 A8 A10 A13 A19 | rows (rerun of b4549d) |
| 261113-111813-3cf199 ... e12ef2 | light | checks | `campaign verify`: build-test, e2e, t7-tsan, recovery, removed |
| 261113-111821-2f9e75 | light | A13 | restore corpora replay (macOS) |
| 261113-111826-184241 | light | A6 A15 A13 | Linux VM: Release + ASan builds, all rows, T6 2xram and in RAM, corpora |
| 261113-111855-8bc27e, 261113-111904-2e33d9 | light, exclusive | A21 | Linux s0/s1 builds, ABBA CPU and p99 |

Read since: A16 strict 2567dc (2,500 indexes: p50 108 ms, p99 1,176 ms, peak pinned 100, 42.1 KiB per open index);
A21 9495ee (allocations 0.988, bytes 1.000, handlers 0.986: no worse); A9 from x7-cancel-rows, A22 from x7-view-bench,
A13 coverage from x7-coverage (6 of 9 areas below TESTING.md). All in `docs/gates/phase7c.md`.

Done so far: e2e 261113-111813-3013ac GREEN at Xapiand:0a0321691.

### Earlier sessions

Timing read and scored into `docs/gates/phase7c.md` (head/pre-7c, median of 3 pairs, range): T4 MISS (all 16 HTTP
loops over 1,024 us loaded, worst p99 24.6-28.7 ms, p99.9 49-57 ms; pre-7c's under it); T5 MISS on 2x unloaded + 1 ms
(76.8 ms against 8.93 ms) but 0.194 of pre-7c; T10 flood p99 50.94 ms against about 20 ms (MISS); T11 PASS, CPU 0.073
(0.073-0.098), hop wall gone; T12 RESTORE 0.288 (0.279-0.305; profile_bulk 0.284) MISS, QPS 2.599 (2.371-2.617)
PASS; A21 macOS CPU 0.958 (0.930-1.019), p99 0.962, within D62; X3-5 35.1 KiB per open index at 10,001 open.
A16 af8ee2 timed out (10,000 indexes take 1,233 s to create): resubmitted at 2,500 as 261111-200215-2567dc (strict).
R13 rejected by the lead: allocations and handlers per request now measured by 261111-200223-9495ee (a DYLD
malloc interposer, and asio handler tracking builds 261111-200219-6bf1ed), ABBA x3 each.


Timing rows, mine by the lead's 11-11 message (D59: one job of at most 10 minutes each, holding ABBA pairs), at
Xapiand:49ebb4481 (adds node CPU per phase to `stall_load.py`):

| Job | Level | Criteria | What |
|---|---|---|---|
| 261111-171014-19268b | light | builds | build-cur (head), build-pre (142b2ca77), build-pre-hop (+ http 10c5074), slot `x7-timing` |
| 261111-171016-01a84c | light | builds | build-s0 (b6b8f7534), build-s1 (927acf570), slot `x7-shape` |
| 261111-171041-c9ab68 | exclusive | A4 A5 | stall_load pre cur cur pre, both phases; same-index run on cur |
| 261111-171043-7a746f | exclusive | A11 | profile_light pre-hop/cur ABBA x3, `--compare` per pair |
| 261111-171045-06e893 | exclusive | A12 A24 | loadtest RESTORE docs/s, QPS, ABBA x3 |
| 261111-171048-c39d8d | exclusive | A12 A24 | profile_bulk RESTORE, ABBA x3 |
| 261111-171050-a82ab4 | strict | A10 | lua_bounds --loops 4 (flood p99), T4 beside 4 looping scripts |
| 261111-171052-af8ee2 | strict | A16 | wide_search_check gate, 10,000 indexes, 10 searches |
| 261111-171055-51a31b | exclusive | A21 | before/after the one-shape wave, ABBA x3: CPU per light request, p99 |

The CPU change makes rows 048b3b and build-test c4aa56 other code: reruns 261111-171145-b4549d and
261111-171146-002593 (cite them once they pass). Each job prints its pairs' cur/pre ratios (`pairs.py`, written into `$JOB_OUT` by the command). Read them into
`docs/gates/phase7c.md`; a pair interval straddling a target earns one second run (D59).


Read at Xapiand:0cec18474 (master merged again since: Xapiand:03ba6478a, no engine change the brief counts):
rows 261111-142844-048b3b 14 of 14 rows pass, ASan ctest 37 of 37; t1_census 51 threads, none outside section 2;
T8 6 enforced rounds each, peaks in-flight 1,000, demoted 0 light / 23 heavy, admitted 503s are the reader
registry's "Too many searches are reading at once" (light 9,919, heavy 11,690), 0 other errors; T10 Release flood
light GET p99 57.10 ms (timing: lead). e2e, t7-tsan, recovery, removed pass. build-test 9c25fd failed 1 of 406 (test_pool
`tests/test_pool.cc:2203`, master timing flake under load; lead's hunt row), rerun 261111-162221-c4aa56 by the lead.
X3-5 66c8ef exit 2: the slot's `.deps` held the rows job's ASan dependency builds (FetchContent keeps them in its
base dir), so the Release link missed `___asan_*`; resubmitted as 261111-165347-d979c7 in its own slot `x7-x35`.
`docs/gates/phase7c.md` carries these results.

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

1. Read the 2026-11-13 jobs (Doing) into `docs/gates/phase7c.md`: each row's value at Xapiand:0a0321691, the status
   line, T4/T5's light p99 as a miss with PLAN.md P1 (diagnosis: explore/p1-light-wait, not here).
2. Paste `campaign brief`'s Evidence table; State "ready for review".

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

8. **C5 FreeBSD (A15), for the lead.** No machine runs the rows: `freebsd.yml` builds and attaches a release package
   for a tag (it writes the release) and runs no rows; strata's FreeBSD VM has 4 CPUs, 4 GiB, 4.2 GB disk free (88%
   used, mostly `/root/preflight`), no bash and no ninja. Options: (a) regrow that VM (disk, memory), `pkg install
   bash ninja`, and run `gate7c_rows.sh` on a Release build (no ASan); (b) a FreeBSD CI workflow on quest branches that
   builds and runs the Release rows (`.github/`, outside this file set); (c) rule FreeBSD a stated miss for 7c and
   carry it to Phase 8. Recommendation: (b), since CI is where the other platforms prove; the report states the miss.

7. **T4, T5, T10's flood, T12's RESTORE miss (the lead's: the gate's outcome).** With no offload pool every HTTP loop
   stalls 25-29 ms at p99 under the mixed load (pre-7c: under 1 ms), and RESTORE runs at 0.29x pre-7c while using
   0.81x its CPU per document (wall lost to serialization, not work). Options: (a) a profiling quest on the loaded
   phase (which work runs unsliced on the reactors: the RESTORE stream, aggregations, or merges) and on RESTORE's
   wall; (b) rule the targets for 7c and carry them to Phase 8. Recommendation: (a), the gate's claim is "never
   blocks a reactor". The report states the misses as they are.

0. A6, A15 and A21's Linux half wait on the Linux VM (Detent is timing on it, lead 11-11). FreeBSD: no functional
   CI job. A9: x7-cancel-rows. A13 coverage, corpora: x7-coverage. A22: x7-view-bench. R4 is ruled (lead, quest
   Decisions). A21 allocations and handlers per request: owed (R13).

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

## Messages

- msg-1 (lead, phase7c.md amendment from x7-loop-fix-c): applied at Xapiand:0a0321691 now that fix (c) is on master;
  the prepare chores' class follows the code (`flush`), see R14.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | rows 261111-171145-b4549d (`t1_strict_owed`, `t1_census`: only section 2's threads) |
| A2 | rows 261111-171145-b4549d (`t2_e2e_asan`, `t2_stored_asan`); e2e 261111-142834-e6b01e |
| A3 | recovery 261111-142835-bb67a2; rows 261111-171145-b4549d (`t3_crash`, `t3_crash_asan`) |
| A4 | 261111-171041-c9ab68: MISS, every HTTP loop's p99 24.6-28.7 ms against 1,024 us (Blocked on 7) |
| A5 | 261111-171041-c9ab68: MISS on 2x unloaded + 1 ms (76.8 against 8.93 ms); 0.194 of pre-7c; same index 98.2 ms |
| A6 | <no evidence yet> (Linux VM) |
| A7 | t7-tsan 261111-142834-da0a9e; rows 261111-171145-b4549d (`t7_asan`) |
| A8 | rows 261111-171145-b4549d (`t8_overload_*`: enforced rounds, peaks 1,000 / 23, 0 other errors); RSS within 10% of T4's not comparable (2 against 16 reactors) |
| A9 | <no evidence yet> (quest x7-cancel-rows) |
| A10 | rows 261111-171145-b4549d (every case); 261111-171050-a82ab4: flood p99 50.94 ms MISS, T4 with loops MISS |
| A11 | 261111-171043-7a746f: PASS, dispatch CPU 0.073 (0.073-0.098) of the hop's, its wall gone |
| A12 | 261111-171045-06e893, 261111-171048-c39d8d: RESTORE 0.288 / 0.284 MISS; QPS 2.599 PASS; 16 logical CPUs |
| A13 | rows 261111-171145-b4549d (ASan+UBSan ctest); t7-tsan 261111-142834-da0a9e; 261111-165347-d979c7 (35.1 KiB per open index at 10,001); coverage, corpora: x7-coverage |
| A14 | `docs/gates/phase7c.md` (draft, every row scored) |
| A15 | <no evidence yet> (Linux VM, FreeBSD) |
| A16 | 261111-165347-d979c7 (10,000 indexes, light: p50 686 ms, peak pinned 5,000); strict 261111-200215-2567dc pending |
| A17 | build-test 261111-171146-002593; rows 261111-171145-b4549d (`t1_strict_owed`) |
| A18 | `docs/design/phase7c.md` (D16) |
| A19 | removed 261111-142835-15e20c; rows 261111-171145-b4549d (`x6_shutdown`) |
| A20 | build-test 261111-171146-002593 |
| A21 | 261111-171055-51a31b (macOS CPU 0.958, p99 0.962: within D62); allocations, handlers 261111-200223-9495ee pending; Linux owed |
| A22 | <no evidence yet> (quest x7-view-bench) |
| A23 | D53 (T4 reads the control loop only while busy) |
| A24 | pins http:51d153a, prospect 850d4e8; T12's jobs 261111-171045-06e893, 261111-171048-c39d8d (RESTORE miss stands with both) |

## Result

Not yet. Launch 1: `harness/gate7c_rows.sh` (Xapiand:e7e8e03c5), `docs/gates/phase7c.md` skeleton.

## Rulings I made

- R14 (msg-1). Applied the x7-loop-fix-c amendment to `docs/design/phase7c.md` (Xapiand:0a0321691): D4's flush row
  gains the RESTORE writes, D12's RESTORE sentence is fix (c)'s description. One correction against the code: the
  prepares run in `flush` chores (`src/database/handler.cc` `DocIndexer::index`, `chores.run(engine::ChoreClass::flush,
  pull)`), not `query` chores as the amendment's text said; D1's barriers and schema changes in input order kept.
- R15. T6's "2x RAM" is a cgroup cap, not a 30 GB index: the VM has 16 GiB and D59 bounds a job. `stall_load.py
  --index-size 2xram` runs the node in its own `systemd-run --user --scope` and, after loading, sets `memory.max` to its
  anonymous bytes plus half the index on disk; each phase reports each thread's major faults from `/proc/PID/task`
  (tests in `test_gate_tools.py`). Counts, not timing: a light job.
- R16. SR3 is read from the loop-lag histograms the node exports (`stall_load.parse_metrics`, `lag_delta`) over the
  wide-search gate's timed searches, the lag `stall.py` would sample, in the same strict job as A16.
- R17. T10, T12 rerun with T4/T5: x7-loop-fix-c rewrote RESTORE's path and loop-scheduling the loops' turns, so their
  2026-11-11 values describe code master no longer has.

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
- R10. X3-5 builds in its own slot: FetchContent keeps dependency build directories in its base dir, so a Release
  build sharing `.deps` with an ASan build links ASan objects.
- Lead rulings (2026-11-11): A13's corpora wait on x7-coverage (restore bug, quest restore-mixed-sign-integers); A21
  uses D62 (over 5% CPU or 10% p99 after optimizing).
- R11. Timing rows at 20,000 accounts, not the design's 1,000,000: loading 1,000,000 takes about 5 minutes an arm
  (`phase7c-baseline.md`), past D59's 10-minute job; both arms run the same size in one job, so the ratio holds and
  the absolute values are stated at that size. The light rate is fixed at 200/s for both arms (the baseline used
  half its saturating rate, 210/s at 1,000,000).
- R12. The pre-7c binary (142b2ca77) runs through a wrapper adding `--solo` and `XAPIAND_ENGINE=strata`, which X1
  removed (Xapiand:4ef308995); the harness is the head's for both arms, so measurement code is one.
- R13 (allocations unmeasured: rejected by the lead; now measured by 261111-200223-9495ee). A21's macOS half compares the commits around the one-shape wave, Xapiand:b6b8f7534 (before reactor step R)
  and Xapiand:927acf570 (http H1 + H2 + the affinity fix); the merge inside it (Xapiand:4ff5eb99d) changes two `src`
  files.
- R3. T8 runs at dry-run size (20,000 documents): the row's functional claims are 503s and no other errors; RSS
  within 10% of T4's run needs the lead's 1,000,000-document run.
