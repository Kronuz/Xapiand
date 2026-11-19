# wide-search-loop review: correctness

Commit: Xapiand:3e2edbef4336 (src at Xapiand:adc71378b, identical in `src/` to Xapiand:cffd0190a)
Verdict: CHANGES
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite at cffd0190a, 261118-174750-e0b022 (pass); test_node x5 174748-eec809 (5/5); its fails-without
  174903-ca5c07 and the half-close probe 174834-15710d / mutant 174835-9a2a47. `src/` is unchanged from
  cffd0190a to the head, so these stand for the head's code.
- A2/A3 at the head have no evidence: every rerun was skipped. build-probe 204934-64d3ad failed (exit 3:
  it ran in slot `xapiand-wide-search-loop`, which has no `build-probe`; the binaries live in slot
  `wide-search-loop`), and the VM create 204950-07c772 failed. The lead has to resubmit both chains.
  Pre-fix reads (A2 10,000 exclusive 6328f9/2ba112, 2,500 09c53e/c9dd00) carry over for searches only.
  A list resolution is the same before and after cffd0190a.

## What I checked myself

Scripts are in `.scratch/xapiand/review-wide-search-loop-correctness/` (home checkout). The binaries in
`bin/` are copies of slot builds `build-hc-master` and `build-hc-branch` (RelWithDebInfo, LTO off,
master's src and the branch's src at cffd0190a). They are checked by symbol (`wide_search::` 0 vs 21)
and neither carries the LAG_SPIN beat.

| Job | Probe | Result |
|---|---|---|
| 261118-205937-29ccee | `halfclose_multi.py`: non-search requests that name a list, half-closed, branch vs master | FAIL: 5 rows differ (F1) |
| 261118-210143-ef66d1 | `equal_results.py`: 40 indexes x 30 docs, `--concurrent-search-indexes 5 --database-pool-size 10` (wide path, pins contended); 16 queries (sorts, offsets, relevance, OR, `_values`/`_stats`/nested `_avg` aggs, collapse, count) by list and by pattern; 12 clients x 4 rounds beside 4 threads that reset or half-close wide searches (8,812 / 18,555 abandoned) | PASS: 768/768 answers equal the sequential reference on each binary; branch reference equals master's 16/16; a wide search after the stress answers in 0.39 s (no leaked hold) |
| 261118-205951-6c2597 | `tsan.sh`: one TSan build of the branch (clang, macOS, asserts on, own deps tree); `xapiand_test_wide_search` x30; `equal_results.py` against the TSan node (30 indexes, 6 clients, 6,669 abandoned) | PASS: 30/30, 0 TSan reports in unit runs and node; stress 192/192 equal |
| 261118-211702-e05d8c | `idle_cpu.py`: idle node CPU-seconds and `top` idle wakeups over 30 s windows, fresh and after wide searches (incl. abandoned), master/branch x2 | Branch 0.16-0.19 s / 30 s (0.55-0.63%), master 0.16-0.18 s (0.53-0.60%), equal within ps's 10 ms resolution; wakeup deltas overlap (master 0-480, branch 116-616 per window). Strict, not exclusive: it counts CPU-seconds, not latency; the exclusive copy 205949 was cancelled. |
| 261118-210116-568ffe | `ab_starve.sh`: A3/T5 by repeats, master/branch ABABAB at 64 clients, same build config, exclusive | queued behind light holds until ~23:40; see F3 |

Code read: `wide_search.{h,cc}`, `search_batch`, `search_batched` (engine/search.cc), `expand_paths` and every
caller of `resolve_index_endpoints` in `search_views.cc`, `task.hh` (budget, event, async_mutex) at the
slot's pin.

Answers to the lead's questions:

1. **Leaks.** No leaks found. A cancelled search unwinds `search_batch` through the `Hold` (gives its
   count back), the partial `lock_shard`s and `PinCount`. `task::cancelled` is not an `engine::Error`,
   so the retry loop does not swallow it. The equality probe's 27,000+ abandoned searches leave no held
   count: the next wide search answers at once, under TSan too. The pin line resumes waiters through
   their own scheduler (`event::set`, `async_mutex::unlock` post to the waiter's scheduler), so no
   request runs on another loop's thread. Cancellation still reaches requests that are not searches,
   though: F1.
2. **Identical results.** Batches run one after another per search (`search_batched` awaits each
   `run`), and the merge (`partial_sort`/`sort`, collapse) has no checkpoint. Yielding only happens
   before a pin or an `engine_for_read`, so the top-k cannot change. Measured: equal to master,
   including ordering, totals and aggregations, under concurrency and cancellation (210143-ef66d1,
   6c2597).
3. **Wake timer.** None ships. The 250 us beat exists only in
   `harness/wide_search_reactor_stalls.patch` (env `XAPIAND_LAG_SPIN`), applied to slot `.deps` by hand.
   The reactor's CMake pin is unchanged and the branch diff adds no timer. An idle node costs the same
   CPU as master (e05d8c).
4. **A3 T5 miss.** The data (F3) do not settle it, and the deciding measurement is 568ffe.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A list of names still checkpoints the resolution of requests that are not searches, so a client's EOF (or shutdown) cancels them before they run. On the branch, half-closed `HEAD /hc,hc2/1` and `POST /hc,hc2/:commit` answer 503 x3 where master answers 200 x3. The half-closed list commit does not commit: after kill -9 the uncommitted `PUT /hc/2` is gone on the branch (404) and kept on master (200). Refused list writes and reads (`PUT /hc,hc2/9`, `GET /hc,hc2/1`) turn 400 into 503. Cause: `wide = paths.size() > 1` (`src/database/wide_search.cc:136`). `document_exists_view` (`search_views.cc:843`) and `commit_database_view` (`:1569`) take lists, and every `> 1` view resolves the whole list before it refuses. `wide_search.h`'s "every document and write view names one" does not hold. It is the same class as the half-closed PUT that cffd0190a fixed. | 261118-205937-29ccee | Checkpoint only where master already did, in search and count: gate on `query_field.patterns` (set only by `QUERY_FIELD_SEARCH`, `search_views.cc:2429`) as well as `paths.size() > 1`, passed into `wide_search::resolve`. Test: a test_node case for a half-closed `POST /a,b/:commit` that survives a restart (fails today), plus `HEAD /a,b/1` and `POST /a,b/:commit` rows in `harness/wide_search_halfclose.py`; `halfclose_multi.py` must PASS. |
| F2 | no | gap | The pin bound (D3) has no suite test through `search_batch`, so nothing in the suite fails if the `take` is removed or a `Hold` leaks. The unit tests cover only `Pins` in isolation and order threads with `sleep_for(20ms)`. In `test_a_cancelled_batch_leaves_the_line`, if `behind` reaches the line before `head`, it becomes the uncancellable head and `behind.join()` waits on `two`, which is released only after the join: a hang under load. It passed 30/30 under TSan here. | `tests/test_wide_search.cc` (the three new cases); 6c2597 | Order the threads by `held()`/line state, or by a hook, not by sleep; add a handler-level case where a cancelled wide search returns its count (`pins().held() == 0`). |
| F3 | no | gap | A3 (T5 at 64 clients) is not decided. In the same strict conditions, the branch's light p99 is above master's in both pairs: 1.69 vs 1.28 ms (050258 / 051429) and 2.85 vs 1.26 ms (243767 / bb97fa). Its max is far lower (3.4-16.5 vs 928-1,140 ms). Slicing makes many light GETs wait one 200 us slice instead of a few waiting one long turn. The light GET's own resolution also takes the resolver's global `std::mutex`, which 64 wide searches' misses contend for on 16 loops, so a preempted holder adds host noise to the light p99. 243767 also ran beside 6 light holds with loop maxima of 65 / 41 ms. So: partly the change, amplified by noise; the bound sits near the host's 2.0-2.6 ms idle-wake floor. | 243767, bb97fa, 050258, 051429 (logs) | Deciding measurement: 568ffe (exclusive, ABABAB x3, same build config, same 2,000-index data). If the branch misses T5 in 2 of 3 and master passes, it is the change. Then run the beat-on sampler at 64 clients to split late beats into on-CPU (slice too long) and `__psynch_mutexwait` (resolver mutex). |
| F4 | no | nit | Under contention, wide-search throughput drops by design. The equality stress (light, indicative only) took 152 s on the branch against 74 s on master. In S2 the branch's wide-search p50 is 12.4 / 18.4 s against master's 7.5 / 8.8 s. A user-visible trade of the node-wide bound, so the owner's call: keep it, or size the bound above soft_open. | 210143-ef66d1, 243767, bb97fa | Owner: keep (the bound is the quest's point) or widen; state the trade in the design doc's Limits. |
| F5 | no | nit | `Pins::try_take` has no production caller. `search_batch` counts positions (`end - begin`), but pins only distinct shards, so a path named twice holds one count too many. Conservative. | `wide_search.h:87`, `handler.cc:1477` | Drop `try_take` or give it a caller; take `shards.size()` after deduplication, or document the over-count. |

## Not run

- A2 on Linux and on macOS at the head, and A3 at the head: the contributor's reruns were skipped
  (slot mismatch, VM create failure); not reproduced here.
- 568ffe (A3 repeats) had not run when this was written; the lead reads it.
- Linux TSan/ASan: CI's (CI was still running at the head when I looked).
- Cluster (remote endpoints) pins and shutdown cancellation were not probed; shutdown reaches F1's views
  the same way an EOF does (the request token).

## Re-check

Commit: Xapiand:5b206efde (src at Xapiand:e649ebcb1 plus the master merge Xapiand:0b7e4b69a; docs-only after it)
Verdict: APPROVE
Date: 2026-11-19

Read `git log --first-parent 3e2edbef4336..5b206efde`: the fixes are Xapiand:9d827de68 (F1), Xapiand:78198eea6
(F2, F5) and their tests Xapiand:6fe65e78a, Xapiand:d9cc31863. No new probe: the jobs below settle every question.

| ID | Status | Evidence |
|---|---|---|
| F1 | closed | `search_views.cc:2291` passes the budget only when `query_field.patterns`, which is set at one place (`:2432`, under `QUERY_FIELD_SEARCH`) and taken only by `search_view` (`:1959`) and `count_view` (`:2117`); `wide_search.cc:112` yields only with a budget. My `halfclose_multi.py` at the fix, 214455-d04574: PASS, every row equals master, including the half-closed list commit surviving kill -9 (`GET /hc/2` 200). Harness rows 214452-f8d1d8 PASS; the mutant that passes the budget unconditionally fails them, 214453-f4a3ac (list HEAD and :commit 503). test_node `test_a_half_closed_commit_of_a_list_commits_durably` passes in e0caec (5/5) and fails at the test commit before the fix, 214416-55f6c4 (0 hits after restart). |
| F2 | closed | Unit cases now order by `task::when_all`, which starts each child in order on one thread up to its first wait (`task.hh` `when_all_awaiter::await_suspend`), so the cancelled-batch case can no longer make `behind` the head; no sleeps or threads remain in `tests/test_wide_search.cc`. Handler-level `test_a_cancelled_wide_search_gives_its_count_back` (`tests/test_node.cc:1712`) checks the count is taken, then given back on cancel; it fails with the `take` deleted, 214405-d4ad8f (`:1732`). |
| F3 | closed (non-blocking) | 568ffe (exclusive, ABABAB, at the pre-fix code; the fixes do not touch a search's path): T5 passes 6/6. Light p99 at 64 clients: branch 1.86 / 1.58 / 1.57 ms, master 1.51 / 0.68 / 1.09 (medians 1.58 vs 1.09); light max: branch 9.9 / 5.3 / 9.4 ms, master 473 / 1,299 / 1,982 ms. The branch trades about 0.5 ms of p99 for a light max 50-200x lower, inside T5. |
| F4 | open, owner's call (nit) | Unchanged. In 568ffe the branch also finishes more wide searches than master (205 / 256 / 256 vs 128 / 160 / 200) at a lower p50 (4.3 / 3.1 / 4.1 s vs 7.1 / 6.0 / 4.5 s); the slowdown shows only under pin contention (ef66d1). |
| F5 | closed | `try_take` is gone; `search_batch` takes `first.size()`, the distinct shards (`handler.cc:1474-1487`), and pins each once. `test_a_wide_batch_counts_a_repeated_index_once` passes in e0caec and fails at the commit before the fix, 214430-1d4cc0 (`:1690`). |

Master merge Xapiand:0b7e4b69a: its `src` change is only `database/schema.cc` (stem-language names), plus the
logger pin in `CMakeLists.txt`; neither touches `wide_search`, `search_batch`, `resolve_index_endpoints` or the
pins. The suite at the merged head passes 52/52, test_node and test_wide_search included (261119-015540-f34736,
at Xapiand:9b474d811; docs-only to the head).

A2 numbers (not a finding):

- macOS, exclusive, at e649ebcb1 (214551 / 214554 / 214552 / 214555): branch HTTP loops max at most 2,560 us at
  2,500 and 10,000 indexes, the control loop's own p99 (2,560 us). Master reaches 65-115 ms (2,500) and 262-328 ms
  (10,000).
- Linux VM, exclusive, guest binaries of e649ebcb1 (015840 / 015843 / 015846 / 015849): branch HTTP p99
  1,536-2,048 us, max at most 2,048 us (2,500) and 3,584 us (10,000); control loop p99 1,792-2,048 us. Master
  max at most 82 ms (2,500) and 393 ms (10,000). The branch's max sits at the VM's idle floor, above T4's 1,024 us:
  the owner's decision.

Not run: Linux TSan/ASan and CI arm64 (CI's; arm64 was still running at the head per `campaign brief`).
