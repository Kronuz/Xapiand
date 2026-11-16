# platform-rows: journal

State: waiting on CI runs 43822426951 (FreeBSD) 43822427028 (arm64) at Xapiand:7c5fac053
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

First runs (freebsd 43817110685, arm64 43817110777) failed. Findings:
- FreeBSD: Release build + 40/40 ctest passed; the ASan build's pool.cc compile was OOM-killed (exit 137, -parallel 4). Workflow: now -parallel 2.
- arm64 t3_crash, t3_crash_asan: strata_check tool not built (harness needs the target). Workflow: build the target in both trees.
- arm64 t1_census: offender Xapiand:MAIN; gate7c_rows.sh ALLOW lacked the main thread's name (src/main.cc:759 set_thread_name("MAIN")), so a harness gap, not platform. ALLOW now includes it (lead: confirm that 5 MAIN threads at 4 reactors is the intended census).
- arm64 t2_e2e_asan: 49 of 761 assertions fail; the cause is unknown (assert.txt was not uploaded). Now uploaded; newman 6 installed explicitly. Product vs harness undecided until the next run.
Passed on arm64: t1_strict_owed, t2_stored_asan, t7_asan, t8 light/heavy, t10 lua and lua_asan, x6_shutdown, gate_tools.

## Next

1. Read the new runs; classify t2_e2e_asan; FreeBSD rows are still unseen.
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
