# wide-search-loop: journal

State: waiting on jobs 261118-112500-d8889b 112959-e5f703 113018-3774af 113037-a28a54 113056-647326 113058-7cb4e4 113452-f9ea27 113513-265869 114932-476f73 114952-b7e8e8 115012-d054e8 115147-90db6b 115151-19c8d0 115153-2a8c6a 115155-b64c38
Quest: .campaign/xapiand/quests/wide-search-loop.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 3: master merged (Xapiand:700ffc0c3, no src change from master), branch pushed for CI (A4). Session-2 jobs read
into `docs/design/wide-search-loop.md` "Branch measured": at 10,000 exclusive the http loops' max 322 / 373 ms (master)
-> 2.07 / 2.07 ms (branch); S2 64 names light max 1,140 -> 3.4 ms, pattern p99 17.6 -> 1.9 ms; the 100,000-entry
resolver cache changes nothing (hypothesis dropped, `src/index_resolver_lru.*` untouched). Open: branch p99 at
10,000 exclusive 1.5-2.0 ms vs master 384-448 us (idle wake slack or short turns: the beat-on A/B decides).

Lead R6 (Xapiand:86a50e08e): A2's 1,024 us bound is read on Linux (strict jobs in the VM at ~/.scratch/vm-document-gate,
port 18782); on macOS each http loop's max is no worse than the same run's control loop; the beat A/B decides the p99
rise, and longer turns fail A2 on either OS. `harness/wide_search_vm.sh` (Xapiand:57818c369) boots the VM under a mkdir
lock, builds both arms (master = 8e5040da8's src reverted), creates the indexes, runs SR3, and stops it on exit.

| Job | Level | For | What |
|---|---|---|---|
| 261118-112500-d8889b | light | A4 | suite at 700ffc0c3 |
| 261118-112959-e5f703 | exclusive | A2 macOS before | SR3 2,500, build-prof (master src) |
| 261118-113037-a28a54 | exclusive | A2 macOS after | SR3 2,500, build-probe (branch src) |
| 261118-051433-c76a1c | exclusive | A2 macOS before | SR3 10,000 master src (reused at 700ffc0c3: only merges since) |
| 261118-051434-89f2f5 | exclusive | A2 macOS after | SR3 10,000 branch src (reused likewise) |
| 261118-113018-3774af | strict | A3 before | starve 64, master src, exits on T5's verdict |
| 261118-113056-647326 | strict | A3 after | starve 64, branch src, exits on T5's verdict |
| 261118-113058-7cb4e4 | strict | A1 | attribution rerun of 043331-286e1f at 700ffc0c3 |
| 261118-113452-f9ea27 | exclusive | R6 p99 | SR3 10,000 master src, XAPIAND_LAG_SPIN=1000000 (250 us beat keeps loops awake) |
| 261118-113513-265869 | exclusive | R6 p99 | same, branch src |
| 261118-114932-476f73 | light | | VM: apt deps, build branch and master arms |
| 261118-114952-b7e8e8 | light | | VM: create 2,500 gate indexes |
| 261118-115012-d054e8 | light | | VM: create 10,000 gate indexes |
| 261118-115147-90db6b | strict | A2 Linux before | SR3 2,500, master src |
| 261118-115151-19c8d0 | strict | A2 Linux after | SR3 2,500, branch src |
| 261118-115153-2a8c6a | strict | A2 Linux before | SR3 10,000, master src |
| 261118-115155-b64c38 | strict | A2 Linux after | SR3 10,000, branch src |

macOS builds are the session-2 binaries (f5a684 build-probe, 692132 build-prof): the tool matched them at 700ffc0c3.

Session 2 (jobs read; numbers in the design doc):

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
| 261118-051427-692132 | light | build-prof: master's src (8e5040da8 reverted in the job), same sampler |
| 261118-051429-5d9b36 | strict | A3 before (lead msg): starve 64 at master src, same sampler inert |
| 261118-051431-0d7e48 | strict | A2 before (lead msg): gate 10,000 at master src, same sampler inert |
| 261118-051433-c76a1c | exclusive | T4 host-or-engine: gate 10,000, master src |
| 261118-051434-89f2f5 | exclusive | T4 host-or-engine: gate 10,000, branch src |

Lead (session 2): A2/A3 before and after use binaries with the same timer-beat sampler, so session 1's before
jobs are superseded by 051429/051431; T4 is decided under exclusive before it is called engine or host.

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

1. Read session-3 jobs. A2 Linux: every http loop's max <= 1,024 us after, at 2,500 and 10,000. A2 macOS: each http
   loop's max <= the same run's control loop. Beat A/B: if both arms' p99 fall near 512 us, the branch's p99 rise is
   idle-wake slack; if only master's, slice the branch's short turns and rerun (fails A2 otherwise). A3 from 3774af /
   647326. A4: suite d8889b + CI.
2. Then the correctness review (lead launches).

## Blocked on

Nothing (R6 ruled by the lead at Xapiand:86a50e08e).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/wide-search-loop.md` (Attribution; Branch measured); attribution rerun 261118-113058-7cb4e4 |
| A2 | Linux: 115147-90db6b / 115151-19c8d0 (2,500), 115153-2a8c6a / 115155-b64c38 (10,000), pending. macOS 10,000: 051433-c76a1c before (max 322 / 373 ms), 051434-89f2f5 after (2.07 / 2.07 ms, control 2,560 us bucket); 2,500: 112959-e5f703, 113037-a28a54 pending |
| A3 | 261118-113018-3774af before, 261118-113056-647326 after (pending); session-2 reads: 051429-5d9b36 vs 050258-4c6f40 |
| A4 | suite 261118-112500-d8889b (pending); CI running |

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
- R6 (ruled by the lead at Xapiand:86a50e08e, see the quest's Decisions). A2's max reads against the control loop's floor: on this macOS host the control loop (no searches) shows a
  2,048-2,560 us p99 in every run, so T4's 1,024 us edge is below what an idle loop's wake reaches here. The branch
  meets the floor (2.07 ms), not 1,024 us; the lead rules whether that is A2's pass or Linux gates it.
