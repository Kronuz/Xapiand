# platform-rows: journal

State: waiting on CI runs 43828387680 (FreeBSD) 43828387576 (arm64)
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Runs 43822616893 (arm64) and 43822616910 (FreeBSD) failed; findings from their artifacts and logs. Fixes pushed; re-runs: FreeBSD 43828387680, arm64 43828387576.

PRODUCT FINDING P1 (arm64 t1_census, any Linux): 5 `Xapiand:MAIN` threads at 4 reactors. The 4 extras are the reactor threads started by `reactor::TcpServer::start(unsigned short)::{lambda()#1}` (std::thread running `asio::scheduler::run`, one per reactor); they never call set_thread_name, so on Linux they inherit the creator's comm "Xapiand:MAIN". The creator is the main thread (XapiandManager::run_loop, tid = pid). Fix for the lead: name the thread at the top of that lambda (the census allowlist then needs that prefix). Evidence: run 43822616893, artifact arm64-rows-logs: thread_stacks.txt (tids 25240-25243) and t1_census.log (`offender Xapiand:MAIN`). The harness caps MAIN at exactly 1.

arm64 t2_e2e_asan (51 failures): HARNESS. All trace to the `RESTORE /bank/` request ("HTTP None", response lost) and the checks after it (doc_count 856 not 1000, counts and sort order off). The node log shows "Request took too long! ~49.9s" and no sanitizer report or crash: newman's `--timeout-request 30000` expired on a slow ASan restore. Now `E2E_TIMEOUT_REQUEST` (default 30000), set to 180000 on arm64 and FreeBSD. If it still fails, the 50 s ASan restore of 1000 documents on 4 arm64 cores is itself a perf question for the lead.

arm64 t10_lua (Release, 105 s): "the foreign-script node did not come up"; t10_lua_asan passed. HARNESS: lua_bounds.py's readiness loop spun without sleeping on a non-200 reply. Now sleeps every iteration (a port+1 clash is the other candidate).

arm64 t3 rows now pass; the other arm64 rows pass.

FreeBSD: build and 40/40 ctest passed; ASan tree built at --parallel 2. Rows pass: t1_strict_owed, t2_stored_asan, t2_restore_stream_asan (info), t7_asan, t8 light and heavy. Rows fail: t1_census (24 s), t2_e2e_asan (14 s), t3_crash and t3_crash_asan (0 s), t10_lua and t10_lua_asan (0 s), gate_tools (25 s). Artifacts did not come back from the VM, so causes are unknown; the workflow now prints each failed row's log tail in the job log.

## Next

1. Read the new runs; classify the FreeBSD row failures from the printed logs.
2. Add green run links to docs/dev/freebsd.md and new docs/dev/platforms.md (A1, A2); fill Evidence.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
