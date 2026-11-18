# wide-search-loop: journal

State: waiting on jobs 261118-112500-d8889b 112959-e5f703 113018-3774af 113037-a28a54 113056-647326 113058-7cb4e4 113452-f9ea27 113513-265869
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

| Job | Level | For | What |
|---|---|---|---|
| 261118-112500-d8889b | light | A4 | suite at 700ffc0c3 |
| 261118-112959-e5f703 | exclusive | A2 before | SR3 2,500, build-prof (master src) |
| 261118-113037-a28a54 | exclusive | A2 after | SR3 2,500, build-probe (branch src) |
| 261118-051433-c76a1c | exclusive | A2 before | SR3 10,000 master src (the tool reused it at 700ffc0c3: only merges since) |
| 261118-051434-89f2f5 | exclusive | A2 after | SR3 10,000 branch src (reused likewise) |
| 261118-113018-3774af | strict | A3 before | starve 64, master src, exits on T5's verdict |
| 261118-113056-647326 | strict | A3 after | starve 64, branch src, exits on T5's verdict |
| 261118-113058-7cb4e4 | strict | A1 | `docs/design/wide-search-loop.md` (Attribution; Branch measured); attribution rerun 261118-113058-7cb4e4 |
| A2 | 10,000: 261118-051433-c76a1c before (max 322 / 373 ms), 261118-051434-89f2f5 after (2.07 / 2.07 ms); 2,500: 261118-112959-e5f703, 261118-113037-a28a54 (pending). T4's 1,024 us edge: see R6 |
| A3 | 261118-113018-3774af before, 261118-113056-647326 after (pending); session-2 reads: 051429-5d9b36 vs 050258-4c6f40 |
| A4 | suite 261118-112500-d8889b (pending); CI on 700ffc0c3 running |

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
- R6. A2's max reads against the control loop's floor: on this macOS host the control loop (no searches) shows a
  2,048-2,560 us p99 in every run, so T4's 1,024 us edge is below what an idle loop's wake reaches here. The branch
  meets the floor (2.07 ms), not 1,024 us; the lead rules whether that is A2's pass or Linux gates it.
