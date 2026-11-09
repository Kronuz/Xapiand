# x-pool: journal

State: waiting on jobs 261109-043217-4b0f0b 261109-043218-12479c 261109-043219-431f4f
Quest: .campaign/xapiand/quests/x-pool.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixing the correctness review's findings (`.campaign/xapiand/reviews/x-pool-correctness.md`, CHANGES at
41dba47c5). Fixed at Xapiand:fe3fc2b12:
- C1 (blocking): a `Shard` made for an index that was missing at its pin (`lock_shard::pin`'s not_found catch)
  remembers that (`missing_`), so its `endpoint()` raises not_found "No such database" (404) rather than `closed`
  (500). `ready()` clears it on a successful pin. Search batches skip a missing index as before
  (`test_a_search_batch_skips_a_missing_index`). Test: `test_node` `test_a_write_to_a_missing_index_is_not_found`
  (`set_metadata` with `DB_OPEN | DB_WRITABLE` is not_found, and no index directory appears).
- C2: `ShardEndpoint::commit(flags, wal, cancel)` passes the token to `take_turn`. `DatabasePool::flush` passes
  its token and stops before the next key once cancelled. Test: `test_pool`
  `test_a_cancelled_flush_stops_waiting_for_a_held_turn` (a held turn; the cancelled FLUSH ends within 5 s, not
  after 30 s, with `task::cancelled`, and commits nothing). A joined commit flight is still waited for without
  the token; it holds the turn and is already running, so it ends without waiting on another holder.
Jobs at fe3fc2b12:
- build-test 261109-043217-4b0f0b.
- The reviewer's probe `missing_index_write.sh`, slot `xapiand-x-pool`: 261109-043218-12479c.
- The revert check 261109-043219-431f4f: both fixes reverted in `src/database`; both new tests must fail.
asan and tsan are CI's (`ci:AddressSanitizer`, `ci:ThreadSanitizer`) once the lead pushes.

Step 4 mutants (local, `.scratch/mutants.py`, one string each, rebuilt, its test run, restored): 6 of 6 killed.
| Mutant | Killed by |
|---|---|
| x1 no retry (an owed answer returned) | `test_a_write_to_a_demoted_index_applies_after_one_promote` (internal: "did not apply") |
| x2 `promote()` not run on map_owed | the same ("kept owing I/O") |
| x3 the bound unchecked | `test_a_third_owed_answer_is_internal` (applies 3, settles 2) |
| x4 `soft_open` not clamped | `test_soft_open_is_at_most_the_descriptor_cap` |
| x5 `negative_ttl` ignored | `test_the_sweep_erases_a_missing_index_negative_entries` |
| x6 the use's pin without the token | `test_a_cancelled_request_leaves_the_pin_queue` (hangs: killed by the 300 s timeout) |

## Next

1. Read the three jobs at fe3fc2b12. Once they pass, repoint the Evidence rows to fe3fc2b12 (the code changed), and
   cite C1's probe for `campaign accept x-pool correctness --confirm C1=261109-043218-12479c`.
2. After the lead pushes: `ci:AddressSanitizer` and `ci:ThreadSanitizer` in the A6 and A8 rows.

## Blocked on

Follow-ups for the lead, none blocking this quest:

1. The correctness review's non-blocking C3 to C7, not done here:
   - C3: `test_the_sweep_erases_a_missing_index_negative_entries` checks only an upper bound. Add a check that
     the entries are still there at about TTL / 2, and widen the gap between the TTL and the wait.
   - C4: a debug check (a per-task pinned-key count on `TaskCounters`) for the one-pin-per-key rule (optional).
   - C5: the node stop tests should match "abandon" and "1 pin" on the abandoned index's own line.
   - C6: `Shard::ready()` re-pins with `use = true`, so a request that reopens counts two uses (CT-R31).
   - C7: rerun the mutants m1, m2, s6 to s8 and x1 to x6 as one job at the landing commit.
   Recommendation: C3 and C5 are small test edits worth a follow-up quest along with C6. C4 and C7 are optional.
2. In `harness/`, outside this quest's files:
`harness/restore_stream_check.sh` sends kill -9 before the restore has ended. After a client leaves, the node goes
on indexing what it had buffered (more than 20 s on ASan: R6). The script breaks out once the count holds still
for 1 s, then waits 5 s. Under load a 1 s pause ends that wait while the restore still runs, so the kill comes
before the closing commit and the uncommitted documents go (`DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT`). That is
the one failure in 261109-014325-c420dd. Fix: wait for the restore to end before the kill, for example by polling
until the node logs the restore's end or the count holds still for several seconds, or by giving the node an
explicit "restore over" signal. Then assert that the count survives.

## Evidence

Code at Xapiand:41dba47c5: the tsan fixes, on top of master merged at 9851b57e2 (product code unchanged since
161e1db5d). Jobs at 41dba47c5: build-test 261109-014325-59bf18 (37 of 37), e2e (strict, newman 6)
261109-014325-433dc2, tsan 261109-014325-aad33e (37 of 37), asan 261109-014325-c420dd (37 of 37 in ctest,
0 ASan/UBSan reports, docs E2E 666 of 666; failed only at the harness's restore kill -9 step: R6).

| Criterion | Evidence |
|---|---|
| A1 | build-test 261109-014325-59bf18 (`test_reactor_requests` `test_a_task_that_holds_a_view_waits_for_nothing`, its "a pin" case); docs E2E on an asserts build 261109-014336-a36d6f (666 of 666, 0 asserts or aborts in the node log); mutant "no check in `pin`": M1 in `docs/status/phase7c-x-pool.md` (step 1), the check unchanged since |
| A2 | build-test 261109-014325-59bf18 (`test_pool`: `test_a_cold_pin_reads_what_the_log_replays`, `test_a_fenced_index_is_replaced_and_replays_its_log`, `test_a_resident_commit_on_an_owed_index_lets_it_close`); mutants m1, m2 in `docs/status/phase7c-x-pool.md` |
| A3 | build-test 261109-014325-59bf18 (`test_the_sweep_closes_only_an_unpinned_index`, sweep at 1 kHz); mutants s6, s7 in the ledger |
| A4 | build-test 261109-014325-59bf18 (`test_the_shutdown_commits_and_logs_an_uncommitted_write`, `test_the_sweeps_close_syncs_a_merges_output`); mutant s8 in the ledger |
| A5 | build-test 261109-014325-59bf18 (`test_a_write_to_a_demoted_index_applies_after_one_promote`, `test_a_third_owed_answer_is_internal`, `test_soft_open_is_at_most_the_descriptor_cap`); mutants x1 to x4 above |
| A6 | tsan 261109-014325-aad33e (37 of 37, 0 reports); ASan: `ci:AddressSanitizer` after the push (the local asan check at 41dba47c5 had ctest 37 of 37, the docs E2E 666 of 666 and 0 reports, and failed at the harness's restore step: R6); Linux VM tsan 261109-014336-74371a (suites, graceful shutdown, path boundary, docs E2E: 0 reports); restore layer on ASan 5 of 5 261109-024406-e5ab5b (0 reports); CI `ci:AddressSanitizer`, `ci:ThreadSanitizer` after the push |
| A7 | build-test 261109-014325-59bf18 (`async_check --strict --owed --extern strata=.deps/strata-src`) |
| A8 | `test_a_root_destroyed_while_it_waits_for_a_pin_releases_it` (`tests/test_pool.cc`) in tsan 261109-014325-aad33e (`test_pool` passed) and `ci:AddressSanitizer` after the push (passed in the local asan check: R6) |
| A9 | `docs/design/x-pool.md` D3 ("The step 2 review's CR2, ruled") |
| A10 | build-test 261109-014325-59bf18 (`test_a_destroyed_root_that_held_a_pin_lets_the_pool_join`) |
| A11 | build-test 261109-014325-59bf18 (`test_a_cancelled_pin_stops_waiting`: resident pin and FLUSH cancelled; `pin_chore` rethrows `task::cancelled`) |
| A12 | build-test 261109-014325-59bf18 (`test_pool` `test_a_cancelled_pin_stops_waiting`; `test_reactor_requests` `test_a_cancelled_request_leaves_the_pin_queue`); mutant x6 |
| A13 | `docs/design/x-pool.md` D13 (budget numbers); `tests/test_pool.cc` (test_the_sweep_erases_a_missing_index_negative_entries, passed in build-test 261109-014325-59bf18); mutant x5 |
| A14 | `docs/design/x-pool.md` D2 (why not, ruled); `test_the_hostless_path_and_the_catalog_never_share_an_index` in build-test 261109-014325-59bf18 |
| A15 | build-test 261109-014325-59bf18 (`test_node` `test_a_search_batch_pins_a_repeated_index_once`, `test_pool` soft_open); `wide_search_check.py check` 261109-014336-8c1b28 |
| A16 | build-test 261109-014325-59bf18 at `CMakeLists.txt` pinning strata:5d742b2 (strata main, items 5 and 5b merged) |
| A17 | e2e check 261109-014325-433dc2 (strict, newman 6: smoke, e2e, tls passed, 0 skipped); docs E2E 261109-014336-a36d6f: 666 of 666, plain and asserts builds |
| A18 | `docs/mechanisms/index-registry-on-catalog.md` |

## Result

Steps 2 to 4 of `docs/design/x-pool.md` (revision 3): the index registry is strata's catalog payload. Requests pin
before views, cold pins open and replay, the sweep alone closes, commits run as catalog chores, `map_owed` gets
one `promote()` retry, and pins take the request's token. Code in `src/database/` (pool, shard, handler, lock),
`src/engine/strata/` and `src/manager.cc`; tests in `tests/test_pool.cc`, `tests/test_reactor_requests.cc` and
`tests/test_node.cc`. How it works: `docs/mechanisms/index-registry-on-catalog.md`. The proof ledger is
`docs/status/phase7c-x-pool.md`. The tsan check's two test-side failures (master's tests) are fixed here (R4).
Evidence above, all at Xapiand:41dba47c5.

## Rulings I made

- R1. The docs E2E failure at Xapiand:0638e61fd needs no fix: the 1,000-document RESTORE outran newman's 30 s
  request timeout on a host at load 120 on 16 cores (`x-pool-step3/.scratch/chain5.log` records "load 119.76" and
  "121.92" at the ASan and TSan E2E runs). The node finished it ("Request took too long ~30852 ms"); the client had
  gone, so the docs saw no response and 679 documents. The very binary that failed then (`x-pool-step3/.scratch/build-x3`,
  banner git:2711eae31; 0638e61fd changes only a report string in `src/`) passes on this host at load ~9: 666 of
  666, RESTORE 2.1 s (`.scratch/e2e-x3/`). Its cost is not a regression: node CPU for the
  RESTORE, three runs each (`probe3.sh`, load ~6): step 3 2.28/2.33/2.48 s, step 2 2.27/2.29/2.70 s, pre-pool
  (restore-stream) 2.07/2.02/2.59 s. The race the lead suspected leaves no trace: no sanitizer report in any run.
- R2. D9's bound is two owed answers: strata answers each kind once per transaction, and a demoted index holds no
  buffered record, so after `promote()` no flush is owed; map or flush then drain is the longest sequence.
- R3. CR6 (A14) is recorded, not built: the hostless path serves only the engine's tests and `engine::check`;
  a private catalog would need a host it lacks. Moving those tests to a `DatabasePool` is the follow-up that
  deletes the adapter's registry.
- R4. The lead ruled (reversing "don't fix") that the tsan check's two pre-existing failures are fixed here,
  in `tests/`: they failed the same at master (261108-223749-0e083d) and the branch (261108-224742-6f7cbc).
  Fixed at Xapiand:41dba47c5: the Walker stores `retry_after` before the atomic `state` that publishes it;
  `test_a_zero_cap_is_refused` runs first, before any thread; `RunningNode::join()` joins the control
  loop's thread before the children's `_Exit` (`exit_code()` removed; no `report_thread_leaks=0`). Cause of
  697/701: TSan's `_exit` interceptor exits 66 when reports were counted, and the forked child inherited the
  parent's race report, so fixing the race alone likely fixes them; running it first keeps it independent.
- R5. Merged master instead of rebasing: the branch already carries merges of master, and master changed only
  `harness/` and campaign files.
- R6. The asan check's one failure (261109-014325-c420dd: 2629 counted, 43 after kill -9) is the harness's
  timing, not durability, and not this branch. The layer passes 5 of 5 on ASan at master 6fded97cc
  (261109-024405-538134) and 5 of 5 at 41dba47c5 (261109-024406-e5ab5b), each run keeping 12,886 to 13,076
  documents across the kill. The failing run came at host load 21 to 28. RESTORE runs `DB_DISABLE_WAL |
  DB_DISABLE_AUTOCOMMIT`, so only its closing commit makes what a departed client sent durable, and the
  script can kill before that commit runs. The timeline probe (`.scratch/left_probe.sh`, 261109-025021-2dfba3
  at master, 261109-025023-89b71a at the branch) ran correctly, but its fixed 20 s window cut the restore off.
  The count still rose in every 0.5 s sample (for example 9 to 2,352 in 20 s at load 10), so every run killed
  before the closing commit, on master and on the branch alike (master kept 309 to 528, the branch 112 to 487).
  It shows no documents lost after a commit ran. The probe is dropped; the harness fix is the lead's (Blocked on).
