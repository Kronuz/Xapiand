# agg-shard-parity: journal

State: waiting on jobs 261112-000934-6482ef 261112-000944-ab7bd4
Quest: .campaign/xapiand/quests/agg-shard-parity.md (the lead's; never edit it)

## Doing

Test, CMake line and journal committed. A1 is the quest's build-test check (verify --for A1), A2 the mutant job after it.

## Next

1. Read the A1 job; fix compile errors or real mismatches (a mismatch is a finding: kind and values below, src/ is not mine).
2. A2 mutant submitted (done).
3. Write docs/dev/agg-shard-parity.md (A3) with the kinds and what differs by design.

## Blocked on

Nothing. (Lead ruled 11-12: CMakeLists.txt line and docs/dev/agg-shard-parity.md are in the file set; the check now builds tests.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | check build-test 261112-000934-6482ef (pending) |
| A2 | job 261112-000944-ab7bd4 (mutant, after A1; pending) |
| A3 | docs/dev/agg-shard-parity.md (pending) |

## Result

(pending)

## Rulings I made

- In-process node: the engine seam (scratch shards, engine::search with one MatchSpy), as tests/test_engine_sort.cc and
  test_wide_search.cc do. Production searches all shards with ONE spy (src/engine/strata/search.cc), so the primary
  path is one spy over n shards; per-shard spies merged through serialise_results/merge_results (bytes and objects)
  are tested too, as the remote path and as the merge the mutant drops a partial from.
- Mutant hook is test-side: env AGG_PARITY_DROP_SHARD=k leaves shard k out of every n-shard run; n=1 is unaffected for k=1.
- Floating values compare to 1e-9 relative (values are multiples of 0.25, so sums are exact; variance may round).
