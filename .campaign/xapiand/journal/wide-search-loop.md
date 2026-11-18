# wide-search-loop: journal

State: working
Quest: .campaign/xapiand/quests/wide-search-loop.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Attribution on current master (src unchanged at Xapiand:92bcc001c; harness only). Slot `wide-search-loop`:
build-prof (RelWithDebInfo, -fno-omit-frame-pointer), data in build-wide2500, build-wide10k, build-starve2000.

| Job | Level | For | What |
|---|---|---|---|
| 261118-022548-0f3ded | light | | build-prof |
| 261118-022554-5856e1 | light | | create 2,500 gate + 2,000 starve indexes |
| 261118-022559-f653ff | light | | create 10,000 gate indexes: timed out at 30 min (5,779+ made) |
| 261118-033556-81775d | light | | completes the 10,000 (`--create-only --reuse`) |
| 261118-022612-9e4e4d | strict | | 2,500 gate + `sample` 10 s (attribution) |
| 261118-022616-80ea60 | strict | A2 before | 2,500 gate, SR3 |
| 261118-022618-928443 | strict | A3 before | starve probe 16,64 clients |
| 261118-033603-400442 | strict | A2 before | 10,000 gate + `sample` 15 s (replaces cancelled 4ad293) |
| 261118-024337-7a9aec | light | probe | build-probe: Xapiand:c33b7cc79 + `harness/wide_search_callsites.patch` |
| 261118-025903-1d4a33 | strict | probe | 2,500 gate + sample on build-probe |
| 261118-025905-94c734 | strict | probe | starve 16,64 on build-probe |
| 261118-034210-a84949 | light | A4 | suite (first try 0f4689 timed out in configure, fresh slot) |

Drafted (Xapiand:c33b7cc79): `src/database/wide_search.{h,cc}`, `wide_search::Pins` (node-wide FIFO bound on
indexes pinned by wide-search batches, sized to the pool's soft_open) and `wide_search::resolve` (per-name budget
checkpoint). Their call sites are outside the file set: `harness/wide_search_callsites.patch` applies them to
`src/server/search_views.cc` (resolve loop) and `src/database/handler.cc` (`search_batch`: take pins, checkpoint per
shard) for probe jobs only.

Read so far (src = master, build-prof):
- 80ea60 (A2 before, 2,500, no sampling): http0/http1 window max in the 98,304 us bucket (max since start 87.2 /
  91.5 ms), p99 2,048 us, p99.9 2,560 / 3,584 us; search p50 16.1 s (host loaded: the opens' `close()` dominate).
- 928443 (A3 before, 2,000 indexes): 64 clients by names: light p99 0.88 ms, max 1,153 ms, loops max in the
  1.8-2.1 s buckets; by pattern p99 4.58 ms, max 997 ms. T5 bound 2.61 ms passes on p99 (the searches were slow,
  p50 54 s); the max is the miss.
- 9e4e4d / 1d4a33 (`--sample`): macOS `sample` suspends the node ~11.7 s, so a sampled run's lag is invalid; the
  call graph shows the http loops idle in kevent and the work pool in `run_open` -> `close()`. Not enough samples on
  the loops to attribute; switched to the reactor stall watchdog (`restore_stall_reactor_lag.patch`,
  XAPIAND_LAG_SPIN) with `harness/wide_search_stalls.py`: jobs 040712-d69b8d (rebuild), 040719-143d1c (master),
  040720-9c118f (probe patch).

## Next

1. Read the samples: which frames hold http loops (per-name resolve, sort, search_batch pins, engine::search, merge).
2. Write the attribution and the bound in docs/design/wide-search-loop.md; implement in the file set.

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

- R1. S2's probe is `harness/wide_search_starve.py` (the review's `wide_starve.py` under the file set's glob), with
  `--create-only/--reuse`; its verdict is T5's bound on the name-list phases: light p99 <= 2x alone p99 + 1 ms.
- R2. A2's "within T4's slice" is read as R23 of x7-gate: every loop's window max at the 1,024 us edge.
