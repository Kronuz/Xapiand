# platform-rows: journal

State: blocked on the lead: green runs need the repin (reactor:2ef28dc, system:563880a) and a ruling on P3 (FreeBSD t10_lua memory limit)
Quest: .campaign/xapiand/quests/platform-rows.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Run 6 (arm64 43847919086, FreeBSD 43847919063), no harness changes needed. arm64: only t1_census fails (waits on reactor:2ef28dc via the repin). FreeBSD: t2_e2e_asan and t10_lua_asan fail on P2 (waits on system:563880a via the repin); t10_lua (Release) is new:

PRODUCT FINDING P3 (FreeBSD Release t10_lua, unexplained): library-call loops 0 and 2 of 4 answer `400 Bad Request: Script exceeded its memory limit of 67108864 bytes` (run 43847919063; [400,204,400,204] in run 5 too, different loops). LIB_LOOP holds an 8 MiB string and makes ~200 MB of 1 MB garbage strings (`s:sub(1,1000000):upper()` 100 times); the cap applies to the live bytes the Lua allocator counts (src/lua/processor.cc:593, :743), so the loops pass only if the GC keeps pace. The same row passes on arm64 Linux in every run. Hypothesis for the lead: GC pacing against the allocator's counted bytes differs under FreeBSD's libc malloc or its slower VM, so the 64 MiB cap trips on garbage not yet collected; not proven. It is not harness: the answers are the server's. The ASan variant never reached the loops (P2 killed it first).

Docs written: docs/dev/platforms.md (new) and docs/dev/freebsd.md, with the run links and findings.

Landing: A3 holds (each failure is fixed or a named finding). A1 and A2 ask for green runs, and no run is green on either platform yet. Waiting on: arm64 t1_census and FreeBSD t2_e2e_asan/t10_lua_asan need the repin; FreeBSD t10_lua needs a product answer on P3.

## Next

1. After the repin lands, re-run both platforms; then fill the green run links.
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
