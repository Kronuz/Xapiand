# x-pool: journal

State: working
Quest: .campaign/xapiand/quests/x-pool.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 4 and the review's items, built locally (`.scratch/build-plain`), not yet committed: `apply_settling`
(`src/engine/strata/owed.h`) with `map_owed` and two owed answers at most (D9, A5), the `unleased_maps()`
assertions in `Turn::release` and `Shard::release` (D12), `soft_open()` clamped to the descriptor cap and K to it
(D11, A15), `IndexSweep::negative_ttl`; tests in `test_pool` (XP5 x2, D11, negative entries, hostless vs catalog
for A14) and `test_reactor_requests` (the pin's token, A12); design revision 3 (A9, A13, A14) and the note (A18).

The docs E2E failure at Xapiand:0638e61fd is host overload, not a defect (ruling R1): see Rulings.

Local tools in `.scratch/` (untracked): `build.sh plain|asan|tsan [targets]` (ccache), `probe.sh`, `probe2.sh`,
`probe3.sh` (RESTORE wall and node CPU), `e2e.sh BIN TAG PORT`.

## Next

1. Jobs at 3f4114f30 (the adopted code: the last four commits verified): build-test 261108-214834-ad7d40 done
   exit 0, asan 261108-214834-1c3b6b done exit 0 (its docs E2E did not run: newman 4.5.0, see Blocked on), e2e
   261108-214834-bcc874 "GREEN" from a stale report (Blocked on), tsan 261108-214834-44536b failed exit 8: 35 of
   37 pass; `test_index_pattern` (a TSan race inside its own `test_the_walk_queue_is_bounded`, and
   `test_a_zero_cap_is_refused`'s option exits) and `test_node`'s
   `test_a_write_waiting_for_its_turn_is_let_finish_while_the_budget_has_time` (the abandon child's thread-leak
   report makes its exit non-zero; the ledger ran `test_node` with `report_thread_leaks=0` for this). Both tests are
   master's, untouched here; job 261108-223749-0e083d runs them at master 2173c17fe.
2. Jobs at 8c06fff93: the real docs E2E 261108-223127-f35eff (newman 6, plain and asserts builds); Linux VM
   261108-223155-2703e7 (tsan), 261108-223156-d3fd0f (asan); the e2e check at master 261108-221920-0fcf05.
3. Local mutants for step 4 (`.scratch/mutants.py`), then merge master (only the ledger conflicts: master marks it
   HISTORY), then `campaign verify x-pool` at the merged head.
4. `wide_search_check.py check` job (A15).

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

| Criterion | Evidence |
|---|---|

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
