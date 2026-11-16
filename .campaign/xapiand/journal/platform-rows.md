# platform-rows: journal

State: waiting on CI runs 43847919063 (FreeBSD) 43847919086 (arm64)
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Run 5 (arm64 43840819547, FreeBSD 43840819520). arm64: only t1_census fails (expected until the repin, P1); all 13 other rows pass.

PRODUCT FINDING P2 (FreeBSD, named): a UBSan abort kills the ASan node at its first GET /:metrics. The node log ends with `.deps/system-src/memory_stats.cc:245:37: runtime error: signed integer overflow: 9009393984544989 * 4096 cannot be represented in type 'int64_t'` and then its SUMMARY. The source (Kronuz/system at 20ffc71647, get_total_virtual_memory, FreeBSD branch) reads `vm.stats.vm.v_page_count`, a 32-bit u_int, into an uninitialized `int64_t total_pages` (the sysctl writes 4 bytes, the other 4 are garbage), then multiplies by getpagesize(). Fix (library, for the lead): read a u_int, or zero-initialize total_pages, then widen. This is the cause of t2_e2e_asan's 5 "HTTP None" assertions: /:metrics comes late in the docs collection, next to the WAL pages, so those requests find the node dead (the walapi PUT and WAL requests are collateral, not separate bugs), and of t10_lua_asan's RemoteDisconnected in lag_buckets (also GET /:metrics). Evidence: run 43840819520 job log, node log tail. The Release build does not trap, so t10_lua does not share it: its 2 of 4 library-call loops answering 400 (run 5: [400, 204, 400, 204]) stay UNEXPLAINED; lua_bounds.py now prints each non-2xx body, read it from the next FreeBSD run.

FreeBSD in run 5: 11 of 14 rows pass; the failing rows are t2_e2e_asan and t10_lua_asan (P2) and t10_lua (unexplained).

## Next

1. Read the next runs: the FreeBSD node log for P2, the arm64 t10_lua.
2. After the repin, a green arm64 census; then the green run links into docs/dev/freebsd.md and docs/dev/platforms.md (A1, A2); fill Evidence.

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
