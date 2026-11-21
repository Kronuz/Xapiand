# bulk-load-rerun: journal

State: ready to land (low risk: A1 dry runs, A2 the results doc from the 12 exclusive jobs; lead wrote A2).
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
| A1 | 261120-145950-daf23a, 261120-145951-7b4aef |
| A2 | `docs/perf/bulk-load-rerun.md`: "Today it loads faster than Elasticsearch on every" |

## Result

Xapiand master on strata loads the 2018 Kibana samples faster than Elasticsearch 9.5.4 on this host:
shakespeare 1.35× faster (5.85 s against 7.92 s), logs 1.88× (4.54 s against 8.53 s), accounts 9.1×; in 2018
it was about 2× slower. Indexes 51-57% smaller. Open: peak RSS of 1.3-1.8 GB on loads of 19-46 MB indexes
(hunt row); Elasticsearch's accounts time is likely its refresh interval (unverified). Table and jobs:
docs/perf/bulk-load-rerun.md.

## Rulings I made

- ES is 9.5.4 (latest 9.5.x tarball for darwin-x86_64 on 2026-11-20); security off, single-node, geoip downloader off: the minimum for plain local HTTP.
- logs load into one index per engine (2018 had per-day logstash indices); ES action lines are rebuilt without _type/_index.
- Durable ack: Xapiand `RESTORE ?commit` returns after the strata commit (phase7c D7); default macOS barrier fsync, with an extra `--full-flush-ordering` arm (F_FULLFSYNC). ES `_bulk?refresh=wait_for` returns after the translog fsync (durability=request) and a refresh; an extra arm chunks at 5,000 docs.
