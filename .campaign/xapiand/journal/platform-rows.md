# platform-rows: journal

State: waiting on CI runs 43828387680 (FreeBSD) 43828387576 (arm64)
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Run 3 (arm64 43828387576, FreeBSD 43828387680): arm64 passes 13 of 14 rows (e2e_asan and t10_lua now pass after the timeout and sleep fixes). The only red arm64 row is t1_census: EXPECTED-FAIL until the Xapiand repin picks up reactor:2ef28dc (reactor names its own threads, product finding P1 below); not touched in the harness.

PRODUCT FINDING P1 (arm64 t1_census, any Linux): 5 `Xapiand:MAIN` threads at 4 reactors; the 4 extras are `reactor::TcpServer::start(unsigned short)::{lambda()#1}` threads (asio::scheduler::run) that never named themselves. Evidence: run 43822616893 thread_stacks.txt (tids 25240-25243). Fixed in reactor:2ef28dc; lands here with the repin in CMakeLists.txt.

FreeBSD rows (run 43828387680), logs printed by the workflow:
- t3_crash, t3_crash_asan, t10_lua, t10_lua_asan, gate_tools: HARNESS/WORKFLOW. mkdir/mkdtemp "Permission denied" in the repo's `.scratch`, which the root-run job owns while the rows run as nobody. Fix: chown the repo's .scratch to nobody.
- t1_census: HARNESS. The census came back empty ("no census"): thread_census.sh reads /proc or macOS `sample`, and FreeBSD has neither. Fix: a FreeBSD branch reading `procstat -t` (the TDNAME column); `ps -M` only on Darwin.
- t2_e2e_asan: 5 of 761 assertions fail; the detail was not printed. The job now prints assert.txt for this row. UNCLASSIFIED until the next run.
- Passing: t1_strict_owed, t2_stored_asan, t2_restore_stream_asan (info), t7_asan, t8 light and heavy, x6_shutdown.

Pushed the fixes; runs below.

## Next

1. Read the next FreeBSD run; classify t2_e2e_asan's 5 failures and confirm the census.
2. After the repin, a green arm64 census; then write green run links into docs/dev/freebsd.md and docs/dev/platforms.md (A1, A2) and fill Evidence.

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
