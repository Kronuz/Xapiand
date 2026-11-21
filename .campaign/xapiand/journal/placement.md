# placement: journal

State: waiting on jobs 261121-154749-16ebbe 261121-154751-efa9ae 261121-154754-383773 261121-154756-1861fa 261121-154758-b0bc28 261121-154800-1f9e95 261121-154803-6be494 261121-154805-7c3e3a 261121-154807-24304f 261121-154810-adbda9 261121-154820-8b7277
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Resumed 11-21 15h. All three tasks are on the quest branch; head strata:7c16dd86.

- Task repair merged at strata:53ac259e (`git merge --no-ff` of strata:d30901ae). Its evidence is
  done at d30901ae (placement--repair.md); `campaign task merge` refused it only for six seed-1
  diagnosis runs at the unfixed pin (Rulings).
- Main (strata:cc30217f, ninja-only and faultlab) merged at strata:8b056187: no textual conflict.
  Resolutions:
  1. Ninja only (main's CMakeLists guard): placement's files configure nothing; every job below
     passes `-G Ninja` in fresh build dirs (`build-n*`), since the slot's old `build*` dirs are
     Makefiles trees the guard refuses.
  2. faultlab (main's e8645287 deleted `strata/core/faultfs.hh`): the earlier main merges
     (bfad24fc, c75b08f7) left placement's tests on the deleted header, which is why CI run
     44932372898 failed every platform at `root_world.hh:9` (not a placement fault). strata:7c16dd86
     ports `root_world.hh`, `root_group_test.cc`, `repair_world.hh`, `repair_plan_test.cc` to
     `faultlab/faultfs.h` and `kronuz::faultlab` as `fl`, as main's callers do.
- Jobs at 7c16dd86 (all `--after` build 261121-154749-16ebbe): A1 root tests and replay
  261121-154751-efa9ae, A1 libFuzzer 10 min 261121-154820-8b7277, A2 scan 261121-154754-383773,
  A3 plan 261121-154756-1861fa, A4 defer 3 seeds 261121-154758-b0bc28; A3 group cases at
  `--ref CONSENSUS=consensus@20198fc`, 3 seeds: replace 261121-154800-1f9e95, drain
  261121-154803-6be494, replicas 261121-154805-7c3e3a, waits 261121-154807-24304f.
- Probe (not evidence): every repair group case and the root group test at consensus 8780e44,
  group-heal--install's pin, 261121-154810-adbda9: whether the quest's CI goes green once
  group-heal lands its pin.

## Next

1. Read the jobs; record them in Evidence.
2. Read CI at the pushed head once (one `gh run view`): every platform should build;
   `placement_repair_group` fails at strata's pin b8de3b8 until group-heal moves it (Blocked on).
3. Correctness review and the When-done measurements (the lead's), after group-heal lands.

## Blocked on

- group-heal (the lead's) lands first: it moves strata's consensus pin to 8780e44, which holds
  the leaving-removal fix (consensus 9a2476d) `placement_repair_group` needs. Until then that test
  fails in CI at pin b8de3b8 (replace's fence), so build-test/asan/tsan cannot be green here.
- The new-root-voter install step of A3 (the outcome's last sentence) waits on group-heal's install
  binding, as placement--repair.md records; the tests add a root voter by log catch-up.

## Evidence

At the head strata:7c16dd86 (pending, Doing). At the task heads, passed: A1 261121-085245-cf7c4e,
261121-131722-6fe3d9; A2 261121-131722-ace692; A3 261121-131722-598daa, 261121-131723-0326d9,
261121-085245-d3bfc1, 261121-131723-5feffe, 261121-131723-350e25; A4 261121-131722-93347f.

| Criterion | Evidence |
|---|---|
| A1 | 261121-154751-efa9ae, 261121-154820-8b7277 (pending); fails without: the root mutants 261120-184058-c79fd0 .. 261120-184144-06d7d2 (`campaign brief`) |
| A2 | 261121-154754-383773 (pending); fails without: 261120-181211-11c0e5 |
| A3 | 261121-154756-1861fa, 261121-154800-1f9e95, 261121-154803-6be494, 261121-154805-7c3e3a, 261121-154807-24304f (pending); fails without: 261121-002719-b7c57d, 261121-002720-cd7f2d, 261121-002723-86ef33, 261121-002724-b25808, 261121-002725-07f209, 261121-002726-d6fee4, 261121-002727-19a20a, 261121-002752-d9cb1d, 261121-054427-5c7193 |
| A4 | 261121-154758-b0bc28 (pending); fails without: 261121-004909-30fad7, 261121-002729-8d49af |
| A5 | `docs/mechanisms/placement-root.md`, `docs/mechanisms/placement-repair.md` (both on the quest branch at 7c16dd86) |

## Result

Not yet: evidence at the merged head pending; group-heal lands first.

## Rulings I made

- The ASan/TSan fix in `root_group_test.cc` (the root task's file, merged) is made on the quest
  branch, not a task branch, since the quest's CI is what it turns red; repair merged it.
- Other CI failures on the repair branch (run 44886359089), outside placement's files and not on
  the quest branch's run: `manifest_log` (asan) and `catalog_stress` (macos-x86_64); reported to
  the lead, not fixed here.
- Task repair merged by `git merge --no-ff`, not `campaign task merge`: the tool asked to rerun
  six seed-1 diagnosis jobs (261121-000231-24e66d, -003512-50dacf, -003514-940aa0, -003517-dd0df9,
  -003522-a43d42, -004525-2c425b) that ran without the consensus fix and would fail again at the
  pin; the task's evidence superseded them at d30901ae. The tool's record of the merge (task
  status, its worktree) is the lead's.
- The faultlab port in four of placement's test files is a semantic merge fix inside the file set.
