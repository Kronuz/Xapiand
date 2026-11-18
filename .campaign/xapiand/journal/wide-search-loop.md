# wide-search-loop: journal

State: waiting on jobs 261118-050242-f5a684 050258-4c6f40 050259-a1757e 050300-6ee958 050301-091601 050302-0cee31 050303-e61fcf
Quest: .campaign/xapiand/quests/wide-search-loop.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 2 (lead widened the set to the call sites and `src/index_resolver_lru.*`): call sites applied as code
(Xapiand:ca68b2e39, patch deleted); harnesses take `--node-arg`; the stall sampler beats on a 250 us timer and prints
the loop thread's CPU time over each late beat (Xapiand:1ccd11a5e): on CPU = a long turn, off CPU = preempted/blocked.
Open misses: T5 at 64 clients (light p99 5.3 ms vs 4.2 ms), T4 at 10,000 (max lag 6.6-9.2 ms vs 1 ms).

| Job | Level | What |
|---|---|---|
| 261118-050242-f5a684 | light | build-probe at 1ccd11a5e (branch src + new sampler) |
| 261118-050258-4c6f40 | strict | T5 control: starve 64, default cache |
| 261118-050259-a1757e | strict | T5 hypothesis: starve 64, `--resolver-cache-size 100000` |
| 261118-050300-6ee958 | strict | T4 control: gate 10,000, default cache |
| 261118-050301-091601 | strict | T4 hypothesis: gate 10,000, `--resolver-cache-size 100000` |
| 261118-050302-0cee31 | strict | T5 attribution: starve 64, `XAPIAND_LAG_SPIN=2000` |
| 261118-050303-e61fcf | strict | T4 attribution: gate 10,000, `XAPIAND_LAG_SPIN=2000` |

Session 1:

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

1. Read the session-2 jobs. Decision table: if the 100,000-entry cache arm clears a miss, the cause is the resolver
   (redesign `src/index_resolver_lru.*`: sharded locks, one entry per index, a cap >= `--max-search-indexes`
   (100,000) so the widest admitted search resolves from cache, its memory stated); if late beats are mostly off CPU
   with the control loop lagging too, the miss is host scheduling, not a loop turn (exclusive rerun decides); if on
   CPU, the symbolized stacks name the turn to slice. Then rerun A2 (2,500 and 10,000, strict, no sampling) and A3 (starve 16,64, strict) as `--for` jobs
   at the landed code, then CI (A4) and the correctness review.
2. 10,000 indexes: master 9d63ea loops' max 395 / 475 ms; probe 0e3792 6.6 / 9.2 ms, still over T4's 1,024 us.
   Next experiment: stall capture at 10,000 with XAPIAND_LAG_SPIN=5000 on build-probe to attribute those turns.

## Blocked on

Nothing. (Session 1's call-site blocker is resolved: the lead widened the set; applied in Xapiand:ca68b2e39.)

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/wide-search-loop.md` (Attribution: jobs 261118-043331-286e1f, 261118-022616-80ea60) |
| A2 | before: 261118-022616-80ea60 (2,500: window max 98 ms bucket); 10,000: 261118-043346-9d63ea (max 395 / 475 ms); after: blocked (probe 0e3792: 6.6 / 9.2 ms) |
| A3 | before: 261118-022618-928443 (64 names: light max 1,153 ms, loops 1.8-2.1 s); after: blocked |
| A4 | suite 261118-034210-a84949 at Xapiand:b46f8e262 (44/44, test_wide_search with the Pins tests); CI pending |

## Result

Attribution done: the stall is per-name resolution (each name a resolver-cache miss that decodes its settings
document on the loop, one turn for the whole list); under concurrency the misses contend on the resolver's
`std::mutex`. Mechanism in the set: `src/database/wide_search.{h,cc}` (`wide_search::resolve`, sliced;
`wide_search::Pins`, node-wide FIFO bound on pinned indexes), tests in `tests/test_wide_search.cc`, design in
`docs/design/wide-search-loop.md`. Probe (call sites patched, job 261118-043334-e2a804 against master 043331-286e1f):
SR3 2,500 loops' max 98-229 ms -> 0.6-1.5 ms buckets; S2 64 names light p99 83.8 -> 5.3 ms, max 1,819 -> 26 ms.
Harness: `wide_search_check.py` (`--sample`, creation resume), `wide_search_starve.py` (S2 probe),
`wide_search_stalls.py` + `wide_search_reactor_stalls.patch` (stall attribution).

## Rulings I made

- R1. S2's probe is `harness/wide_search_starve.py` (the review's `wide_starve.py` under the file set's glob), with
  `--create-only/--reuse`; its verdict is T5's bound on the name-list phases: light p99 <= 2x alone p99 + 1 ms.
- R2. A2's "within T4's slice" is read as R23 of x7-gate: every loop's window max at the 1,024 us edge.
- R3. macOS `sample` suspends the node for seconds (11.7-20 s lag in 9e4e4d, 1d4a33, 400442), so sampled runs are
  not SR3 evidence; attribution uses the reactor watchdog instead. 400442 (10,000 before) is replaced by 9d63ea.
- R4. The slot's `.deps/reactor-src` carries `wide_search_reactor_stalls.patch` from 261118-043329-3f59f9 on: inert
  unless `XAPIAND_LAG_SPIN` is set (one getenv at loop start). Evidence jobs after it say so in their `what`.
- R5. Pins apply only to searches wider than one batch: a single-batch search pinning would queue light searches
  behind wide batches; single-batch searches stay bounded by admission.
