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

1. Run the new tests locally, commit, then `campaign verify x-pool` at the new head.
2. Jobs at 3f4114f30 (the adopted code, the last four commits): build-test 261108-214834-ad7d40, e2e
   261108-214834-bcc874, asan 261108-214834-1c3b6b, tsan 261108-214834-44536b.
3. Mutants for A5 ("no retry") and the D12 assertion; the Linux VM job for A6; `wide_search_check.py` for A15.
4. Ledger `docs/status/phase7c-x-pool.md` brought current.

## Blocked on

Nothing.

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
