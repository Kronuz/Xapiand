# agg-shard-parity: journal

State: waiting on jobs 261112-000453-a3919d 261112-000737-6be2d5
Quest: .campaign/xapiand/quests/agg-shard-parity.md (the lead's; never edit it)

## Doing

Test written and committed (tests/test_aggregation_shards.cc, xapiand-wt branch). First build+run job submitted (A1).
It adds the CMake registration line inside the job, see Blocked on.

## Next

1. Read the A1 job; fix compile errors or real mismatches (a mismatch is a finding: kind and values below, src/ is not mine).
2. Submit the A2 mutant: `--fails-without --env AGG_PARITY_DROP_SHARD=1 --after <A1 job>`, same build slot.
3. Write docs/dev/agg-shard-parity.md (A3) with the kinds and what differs by design.

## Blocked on

The lead, three things outside my file set (tests/test_aggregation_shards*, harness/agg_parity*):
1. CMakeLists.txt: add `aggregation_shards` to the engine-test `foreach (VAR_TEST ... root_routes)` list (line ~1894).
   Without it ctest never runs the test. My jobs apply the one-line edit inside the job checkout; the branch lacks it.
2. The quest's check `build-test` is `cmake -B build -DCMAKE_BUILD_TYPE=Release && ... ctest`: BUILD_TESTS is OFF
   there, so ctest prints "No tests were found!!!" (see job 261111-232734-59ff14's log, x7-loop-stalls). It proves
   nothing for any Xapiand test. Recommend the gate's form: `cmake -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps"
   -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF && cmake --build build -j4 && ctest ...`.
3. docs/dev/agg-shard-parity.md (A3's path) is outside the set; I write it as the criterion names it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261112-000453-a3919d (pending) |
| A2 | job 261112-000737-6be2d5 (mutant, after A1; pending) |
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
