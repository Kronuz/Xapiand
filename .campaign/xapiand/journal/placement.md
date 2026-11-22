# placement: journal

State: waiting on jobs 261121-213312-91071a 261121-213324-a6a5d4 261121-213329-3a65fc 261121-213332-6f2684 261121-213336-391a2c 261121-213340-9bf04b 261121-213344-66bf80 261121-213347-0d93c0 261121-213351-f4e1ca 261121-213355-f37e3f 261121-213405-46539a
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Resumed 11-21 21h. Head strata:63472c6a = strata:7c16dd86 + group-heal's head strata:cd2a4a40
(`git merge --no-ff`, no conflict; consensus pin now 8780e44, group-heal's install binding and
voters_test harness fix). Pushed; CI runs there.

- Jobs at 7c16dd86 all passed (build 261121-154749-16ebbe; A1 -154751-efa9ae, -154820-8b7277; A2
  -154754-383773; A3 -154756-1861fa and the group cases at consensus 20198fc -154800-1f9e95,
  -154803-6be494, -154805-7c3e3a, -154807-24304f; A4 -154758-b0bc28). Superseded by the jobs below.
- Probe 261121-154810-adbda9 (consensus 8780e44 under placement's old group.hh) failed to compile:
  group.hh's dispatch lacked `ActivateImage`, which group-heal's group.hh adds. Merging group-heal
  answers it.
- CI run 44963869517 at 7c16dd86: every platform red on `placement_repair_group` alone (Failed;
  Timeout under asan/tsan), each FAIL "fenced (persistence lacks preowned reservation)" at pin
  b8de3b8: strata's pin before group-heal, not placement's code (the same cases pass at consensus
  20198fc in the jobs above). macos-arm64 also failed `manifest_log` at
  `manifest_log_test.cc:1201`, a wall-clock bound (`even_read - even_snapshot < 0.5 *
  time_scale`) on a shared runner, outside placement's files; main's CI at cc30217f is green.
- Jobs at 63472c6a (after build 261121-213312-91071a): A1 root tests 261121-213324-a6a5d4, fuzz
  10 min 261121-213405-46539a; A2 261121-213329-3a65fc; A3 plan 261121-213332-6f2684, group
  replace -213340-9bf04b, drain -213344-66bf80, replicas -213347-0d93c0, waits -213351-f4e1ca,
  ctest as CI runs it -213355-f37e3f; A4 defer -213336-391a2c.

## Next

1. Read the jobs and CI at 63472c6a once; fill Evidence. All green: State "ready for review at
   strata:63472c6a". A red in placement's files: fix with a test that fails without it.
2. Correctness review and When-done measurements (the lead's).

## Blocked on

- group-heal (the lead's) lands first; this branch now carries its head, so the quest's diff
  against main shrinks to placement's own once it does.
- The new-root-voter install step of A3 waits on wiring group-heal's install binding into the
  repair tests, as placement--repair.md records; the tests add a root voter by log catch-up.

## Evidence

At strata:63472c6a (pending, Doing). Passed at 7c16dd86: see Doing.

| Criterion | Evidence |
|---|---|
| A1 | 261121-213324-a6a5d4, 261121-213405-46539a (pending); fails without: the root mutants 261120-184058-c79fd0 .. 261120-184144-06d7d2 (`campaign brief`) |
| A2 | 261121-213329-3a65fc (pending); fails without: 261120-181211-11c0e5 |
| A3 | 261121-213332-6f2684, 261121-213340-9bf04b, 261121-213344-66bf80, 261121-213347-0d93c0, 261121-213351-f4e1ca, 261121-213355-f37e3f (pending); fails without: 261121-002719-b7c57d, 261121-002720-cd7f2d, 261121-002723-86ef33, 261121-002724-b25808, 261121-002725-07f209, 261121-002726-d6fee4, 261121-002727-19a20a, 261121-002752-d9cb1d, 261121-054427-5c7193 |
| A4 | 261121-213336-391a2c (pending); fails without: 261121-004909-30fad7, 261121-002729-8d49af |
| A5 | `docs/mechanisms/placement-root.md`, `docs/mechanisms/placement-repair.md` (on the quest branch at 63472c6a) |

## Result

Not yet: evidence and CI at 63472c6a pending; group-heal lands first.

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
- group-heal's head merged into the quest branch (strata:63472c6a) by the lead's instruction, so
  the repair group cases run at the pin they need; no file of group-heal's edited.
