# x-pool: journal

State: waiting on jobs 261108-224742-122302 261108-224742-404c84 261108-224742-9b5d88 261108-224742-6f7cbc 261108-223127-f35eff 261108-223155-2703e7 261108-223156-d3fd0f 261108-223539-df1848 261108-221920-0fcf05 261108-223749-0e083d
Quest: .campaign/xapiand/quests/x-pool.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs. Head Xapiand:465f56bb5 (master merged in; it changes no code, only the ledger's status line).
Code last changed at Xapiand:161e1db5d (step 4, tests); design revision 3 at 3df4d681a.

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

1. Gate checks at 465f56bb5 (the lead's -j4 commands): build-test 261108-224742-122302, e2e 261108-224742-404c84,
   asan 261108-224742-9b5d88, tsan 261108-224742-6f7cbc.
2. At 8c06fff93 (the same code): the real docs E2E 261108-223127-f35eff (newman 6, plain and asserts builds);
   Linux VM 261108-223155-2703e7 (tsan), 261108-223156-d3fd0f (asan); wide search 261108-223539-df1848.
3. At master: the e2e check 261108-221920-0fcf05; tsan's two failing tests 261108-223749-0e083d.
4. At the adopted code 3f4114f30 (the last four commits verified): build-test 261108-214834-ad7d40 exit 0; asan
   261108-214834-1c3b6b exit 0 (its docs E2E did not run, newman 4.5.0); e2e 261108-214834-bcc874 "GREEN" from a
   stale report (Blocked on); tsan 261108-214834-44536b exit 8: 35 of 37 pass; `test_index_pattern` (a TSan race
   inside its own `test_the_walk_queue_is_bounded`, and `test_a_zero_cap_is_refused`'s option exits) and
   `test_node`'s `test_a_write_waiting_for_its_turn_is_let_finish_while_the_budget_has_time` (the abandon child's
   thread-leak report makes its exit non-zero; the old ledger ran `test_node` with `report_thread_leaks=0`). Both
   are master's tests, untouched here.
5. When they pass, fill the Evidence rows and Result; State "ready for review".

## Blocked on

The lead (the checks' definitions and `harness/`, outside this quest's files): the `e2e` check proves nothing on
this host. The job's PATH has newman 4.5.0, so `e2e_capture.sh` exits before it writes a report ("needs newman 6 or
later"), and `run_all.sh`'s `run_e2e` then grades the fixed path `/tmp/xa_e2e_ours.json`, a stale report of
2026-11-04 01:38, as GREEN (job 261108-214834-bcc874; the `tls` layer is skipped for the same reason). The `asan`
check's `engine_e2e.sh` hits the same newman and its failure does not fail the check (261108-214834-1c3b6b exits 0
with a Python traceback for the missing `strata.json`). Options: put newman 6 first on the jobs' PATH (a wrapper
`exec npx -y newman@6.2.3`, as `tests/vm_sanitizers.sh` does), make `run_e2e` write to a fresh per-run path and fail
when the capture fails, and make `engine_e2e.sh` exit with the assert's status. Recommendation: all three. Until
then this quest's A17 evidence is its own job with newman 6 (261108-223127-f35eff). The same check at master's
commit (261108-221920-0fcf05) shows it is pre-existing.

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
