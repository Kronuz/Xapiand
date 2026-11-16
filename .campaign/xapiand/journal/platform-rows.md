# platform-rows: journal

State: waiting on CI runs 43840819520 (FreeBSD) 43840819547 (arm64)
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Run 4 (arm64 43834602759, FreeBSD 43834602347). Only t1_census (expected-fail until the repin, P1) and t10_lua fail on arm64; FreeBSD passes 11 of 14 rows (census now works via procstat and passes; t3, gate_tools, x6, t7, t8 pass).

arm64 t10_lua (Release): "the foreign-script node did not come up" again after the sleep fix (2nd run in a row). HARNESS: lua_bounds.py started that node on port+1, and the flood phases just before it leave client sockets across the ephemeral range, so port+1 can be taken. It now takes a fresh free port. Third look at this row: the readiness loop was the first guess and wrong; the port was the cause.

PRODUCT FINDING P2 (FreeBSD only, suspected): valid requests are closed without a response.
- t2_e2e_asan: 5 of 761 assertions fail, all "HTTP None": GET /:metrics, PUT /walapi/, PUT /walapi/doc/1, WAL /walapi/, WAL /walapi-five/. The node survives (later requests answer).
- t10_lua_asan: `GET /:metrics` raises RemoteDisconnected ("Remote end closed connection without response") in lag_buckets.
- t10_lua (Release): 2 of 4 library-call loops answer 400 not 204 ([400, 204, 204, 400]); related or a Lua library difference on FreeBSD, unconfirmed.
Evidence: run 43834602347 job log (assert block and tracebacks). Not harness: the same rows pass on arm64 Linux. The node's own log was not printed; the job now prints xe2e_node.log's tail. The cause is for the lead (src/) once that log shows it.

Pushed; re-runs FreeBSD 43840819520, arm64 43840819547.

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
