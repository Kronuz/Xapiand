# x-pool: journal

State: waiting on jobs 261109-014325-59bf18 261109-014325-433dc2 261109-014325-c420dd 261109-014325-aad33e 261109-014336-a36d6f 261109-014336-74371a 261109-014336-d6fd48 261109-014336-8c1b28
Quest: .campaign/xapiand/quests/x-pool.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at head Xapiand:41dba47c5: the tsan check's two test-side failures fixed (lead's ruling, below),
on top of master merged at Xapiand:9851b57e2. Product code last changed at Xapiand:161e1db5d.

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

1. Gate checks at 41dba47c5: build-test 261109-014325-59bf18, e2e 261109-014325-433dc2, asan 261109-014325-c420dd,
   tsan 261109-014325-aad33e (the fixes' passing side; master's 261108-223749-0e083d is the failing side).
2. Reruns at 41dba47c5: docs E2E newman 6 261109-014336-a36d6f (was f35eff); Linux VM tsan 261109-014336-74371a,
   asan 261109-014336-d6fd48; wide search 261109-014336-8c1b28.
3. When they pass, point the Evidence rows at them, write Result, State "ready for review".
4. History: the strict e2e check at 9851b57e2 (the same product code) passed: 261109-013630-5e7a1c, VERDICT GREEN,
   smoke e2e tls passed, 0 skipped. The other jobs at 9851b57e2 were cancelled for 41dba47c5.

## Blocked on

Nothing. (Resolved: the tsan check's failures are master's tests, which the lead ruled this quest fixes, R4.
The `e2e` check's stale report and newman 4.5 were fixed by e2e-harness, Xapiand:c60138850.)

## Evidence

Code at Xapiand:161e1db5d = 465f56bb5 (the merge changes only docs). Pending rows name the job that proves them.

| Criterion | Evidence |
|---|---|
| A1 | build-test 261108-224742-122302 (`test_reactor_requests` "a pin"); docs E2E on an asserts build 261108-223127-f35eff; mutant "no check in `pin`": M1 in `docs/status/phase7c-x-pool.md` (step 1), the check unchanged since |
| A2 | build-test 261108-224742-122302 (`test_pool`: `test_a_cold_pin_reads_what_the_log_replays`, `test_a_fenced_index_is_replaced_and_replays_its_log`, `test_a_resident_commit_on_an_owed_index_lets_it_close`); mutants m1, m2 in `docs/status/phase7c-x-pool.md` |
| A3 | build-test 261108-224742-122302 (`test_the_sweep_closes_only_an_unpinned_index`, sweep at 1 kHz); mutants s6, s7 in the ledger |
| A4 | build-test 261108-224742-122302 (`test_the_shutdown_commits_and_logs_an_uncommitted_write`, `test_the_sweeps_close_syncs_a_merges_output`); mutant s8 in the ledger |
| A5 | build-test 261108-224742-122302 (`test_a_write_to_a_demoted_index_applies_after_one_promote`, `test_a_third_owed_answer_is_internal`, `test_soft_open_is_at_most_the_descriptor_cap`); mutants x1 to x4 above |
| A6 | asan 261108-224742-9b5d88, tsan 261108-224742-6f7cbc; Linux VM 261108-223155-2703e7 (tsan), 261108-223156-d3fd0f (asan) |
| A7 | build-test 261108-224742-122302 (`async_check --strict --owed --extern strata=.deps/strata-src`) |
| A8 | `test_a_root_destroyed_while_it_waits_for_a_pin_releases_it` in asan 261108-224742-9b5d88 and tsan 261108-224742-6f7cbc |
| A9 | `docs/design/x-pool.md` D3 ("The step 2 review's CR2, ruled") |
| A10 | build-test 261108-224742-122302 (`test_a_destroyed_root_that_held_a_pin_lets_the_pool_join`) |
| A11 | build-test 261108-224742-122302 (`test_a_cancelled_pin_stops_waiting`: resident pin and FLUSH cancelled; `pin_chore` rethrows `task::cancelled`) |
| A12 | build-test 261108-224742-122302 (`test_pool` `test_a_cancelled_pin_stops_waiting`; `test_reactor_requests` `test_a_cancelled_request_leaves_the_pin_queue`); mutant x6 |
| A13 | `docs/design/x-pool.md` D13 (budget numbers); `test_the_sweep_erases_a_missing_index_negative_entries` in build-test 261108-224742-122302; mutant x5 |
| A14 | `docs/design/x-pool.md` D2 (why not, ruled); `test_the_hostless_path_and_the_catalog_never_share_an_index` in build-test 261108-224742-122302 |
| A15 | build-test 261108-224742-122302 (`test_node` `test_a_search_batch_pins_a_repeated_index_once`, `test_pool` soft_open); `wide_search_check.py check` 261108-223539-df1848 |
| A16 | build-test 261108-224742-122302 at `CMakeLists.txt` pinning strata:5d742b2 (strata main, items 5 and 5b merged) |
| A17 | docs E2E with newman 6, 666 of 666 required: 261108-223127-f35eff (the `e2e` check cannot show it: Blocked on) |
| A18 | `docs/mechanisms/index-registry-on-catalog.md` |

## Result

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
