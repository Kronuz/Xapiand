# bulk-load-rerun: journal

State: waiting on jobs 261120-190932-efa931 261120-190934-5e5b1b 261120-190936-9f9465 261120-190938-506908 261120-190940-a549e5 261120-190942-2c3d9d 261120-190944-c56cfa 261120-190945-47638c 261120-190947-713aad 261120-190948-e382e6 261120-190950-a4cd57 261120-190951-0ac3cc
Quest: .campaign/xapiand/quests/bulk-load-rerun.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Harness written (harness/bench/bulk_load.sh|py; ES 9.5.4 darwin-x86_64 pinned by SHA-512). Queued: Release build (d4210e, slot build/bin/xapiand), tarball fetch (747a60), A1 dry runs 1 trial exclusive: xapiand daf23a, es 7b4aef. Host: i9-9980HK x86_64 macOS, 16 threads.

## Next

1. Dry runs passed (xapiand daf23a, es 7b4aef; lead, 11-20). Exclusive jobs, 5 trials, one per arm and dataset (exclusive jobs last at most 10 min), JSON in ~/.scratch/xapiand/bulk-load-rerun/ARM-DATASET.json: the ids in State.
2. Write docs/perf/bulk-load-rerun.md (table, job ids, 2018 column, one paragraph on what moved).
3. Push the branch.

## Blocked on

Nothing

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

- ES is 9.5.4 (latest 9.5.x tarball for darwin-x86_64 on 2026-11-20); security off, single-node, geoip downloader off: the minimum for plain local HTTP.
- logs load into one index per engine (2018 had per-day logstash indices); ES action lines are rebuilt without _type/_index.
- Durable ack: Xapiand `RESTORE ?commit` returns after the strata commit (phase7c D7); default macOS barrier fsync, with an extra `--full-flush-ordering` arm (F_FULLFSYNC). ES `_bulk?refresh=wait_for` returns after the translog fsync (durability=request) and a refresh; an extra arm chunks at 5,000 docs.
