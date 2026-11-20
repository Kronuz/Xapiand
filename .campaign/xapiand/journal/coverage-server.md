# coverage-server: journal

State: waiting on jobs 261120-112532-8d4a2b
Quest: .campaign/xapiand/quests/coverage-server.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Driver harness/coverage_e2e_server.py (groups per node flag set) + tests/test_coverage_server.cc written blind against an old binary; coverage.sh has a `server` workload and NOBUILD=1. Job 444b1a is the first coverage build (slot xapiand-coverage-server keeps build-cov: run the driver against its bin/xapiand to fix expectations, then NOBUILD=1 jobs). Baseline (coverage-areas job 8642ea): server 76.8/49.4, search_views.cc 76.3/48.9, mime_types.cc 88.2/66.0.

## Next

00. Job 101643 ran the driver: 38 failures, server 74.3/48.9. Fixed the driver's expectations (commit "Fix the server driver's expectations"); all groups pass locally on the slot's build-cov. Rerun 8d4a2b measures coverage; read .scratch/cov-server/coverage-uncovered.txt (search_views.cc lines) and add scenarios until >= 90/80. Probes: ~/.scratch/xapiand/cov-server-probe (delete at the end).

0. 095920 failed (driver not executable; mode 755 committed); rerun 261120-101643-e0b959. 092943 failed: tests/test_coverage_server.cc lacked <unistd.h> (fixed); rerun is 261120-095920-b03f29. Merged origin/master at 72547e782 (2026-11-20). Job 261120-092943-82afc7 = build + server driver only (WORKLOADS=server, out .scratch/cov-server); 444b1a was only the build (driver never ran). Read its log, then:
1.  fix the driver's expectations against the real binary, then read coverage-uncovered.txt and add scenarios until server >= 90 lines / 80 branches.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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

## Hunt-list rows (product behavior the driver pins as PIN checks; not fixed here)

- QUIT and FLUSH (no flags) answer 200 but never stop the node: try_shutdown(true)/shutdown(0,0) reach shutdown_sig(0) with _shutdown_asap 0, so shutdown_impl does nothing (src/server/search_views.cc quit_view/flush_view, src/manager.cc shutdown_sig).
- A blob PUT without ?commit then GET answers 500 "Storage EOF".
- Accept "application/json;indent=N" is parsed (search_views.cc ~460) but the answer stays compact.
- POST with an _id in the body ignores it and generates one (Location names the generated id).
- version=abc is read as a stale version (409), not 400; limit=abc, offset=abc, check_at_least=abc, collapse_max=abc are not refused (read as 0).
- Two q parameters are ANDed; a metadata path selector (/t/:key.a) answers 404.
- INFO ?raw puts raw bytes into the JSON text (invalid UTF-8).
- RESTORE of an empty array answers took 4295797.02 (an unset start time).
- --max-search-indexes counts shards: /p1,p2/ with 5 shards each "names 10".

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

Lead ruling (msg): harness/coverage.sh is coverage-core-main's; my edit is reverted (bc5fd77fa, plain commit). It will discover harness/coverage_e2e_<name>.* as workload <name> and add NOBUILD=1. Until then run the driver directly on the slot's build-cov/bin/xapiand. coverage-server lands after coverage-core-main. Job 444b1a ran at b0957c548 (with my old edit) and is only the build.
