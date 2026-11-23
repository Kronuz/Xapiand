# placement: journal

State: waiting on jobs 261123-033240-38613f 261123-033244-fcdb6d 261123-033248-aed250 261123-033252-6cab6f 261123-033256-4f8bcc 261123-033300-366693 261123-033304-7f8260 261123-033307-d5ae57 261123-033311-0f4e28 261123-033402-76ca9b; CI run 45166135463 at strata:ae70ff95
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Resumed 11-23. Head strata:ae70ff95, pushed; CI run 45166135463 there.

- strata:fe2787fb merges group-heal's head strata:a1e786f4, which carries main strata:00eb018d
  (group-catalog) through group-heal's own merge strata:11672618. A direct merge of main
  conflicted only in group-heal's files (group.hh, test/group/mutants.py, voters_test.cc), which
  group-heal already resolved; taking its merge keeps every out-of-set byte group-heal's.
- strata:ae70ff95 migrates `root_world.hh` and `repair_world.hh` to group-catalog's borrowed
  index (D169): `group_test::borrowed` holds the directory and index, the uuid moves from the
  config to `open_index`, as group-heal's `test/group/world.hh` does.
- CI run 44989985435 at 63472c6a: red on every platform (not only the three gates) on
  `group_voters` (all), plus `group_footprint` (linux gcc x86_64/arm64, clang: idle heap 1054 >
  1024 B). Pre-existing, group-heal's: group-heal's own head cd2a4a40 (run 44987690334) fails the
  same two tests on the same platforms. Every placement test passed on every platform. group-heal
  commits since (strata:82ea744c idle-group pointer, strata:9ccddbf7 voters_test) target both;
  this head carries them; their CI is group-heal's run 45161055901 (queued at the time).
- The macos-arm64 `manifest_log` timing bound seen at 7c16dd86 did not recur at 63472c6a.
- Jobs at 63472c6a all passed (exit 0): build 261121-213312-91071a; A1 -213324-a6a5d4, fuzz
  -213405-46539a; A2 -213329-3a65fc; A3 -213332-6f2684, -213340-9bf04b, -213344-66bf80,
  -213347-0d93c0, -213351-f4e1ca, -213355-f37e3f; A4 -213336-391a2c.
- Voided by the merge (they compile group.hh): build, A1 root tests, A3, A4: rerun at ae70ff95
  after build 261123-033240-38613f: A1 -033244-fcdb6d; A3 plan -033248-aed250, replace
  -033256-4f8bcc, drain -033300-366693, replicas -033304-7f8260, waits -033307-d5ae57, ctest
  -033311-0f4e28; A4 defer -033252-6cab6f. A2 -033402-76ca9b (now tagged --for A2 with its
  inputs; the scan's closure is unchanged since 63472c6a). Not voided: fuzz 261121-213405-46539a
  (records.hh, format.hh and the corpus unchanged).

## Next

1. Read the jobs and CI at ae70ff95; fill Evidence. Placement red: fix with a test that fails
   without it. Only group-heal's tests red: they are group-heal's; once its fixed head is green,
   merge it again. All green: State "ready for review".
2. Correctness review and When-done measurements (the lead's).

## Blocked on

- CI green: `group_voters` and `group_footprint` are group-heal's (red at its own head cd2a4a40);
  this branch's CI goes green when group-heal's does and this branch carries that head.
- Files outside the set (34 vs main: group docs, `include/strata/group/*`, `sim/**`,
  `test/group/*`): all byte-identical to group-heal's head a1e786f4, carried by the merge;
  placement edits none. Nothing to revert; they leave the quest's diff when group-heal lands.
- group-heal (the lead's) lands first; this branch now carries its head, so the quest's diff
  against main shrinks to placement's own once it does.
- The new-root-voter install step of A3 waits on wiring group-heal's install binding into the
  repair tests, as placement--repair.md records; the tests add a root voter by log catch-up.

## Evidence

At strata:ae70ff95 (pending, Doing); all passed at 63472c6a.

| Criterion | Evidence |
|---|---|
| A1 | 261123-033244-fcdb6d (pending), 261121-213405-46539a; fails without: the root mutants 261120-184058-c79fd0 .. 261120-184144-06d7d2 (`campaign brief`) |
| A2 | 261123-033402-76ca9b (pending); fails without: 261120-181211-11c0e5 |
| A3 | 261123-033248-aed250, 261123-033256-4f8bcc, 261123-033300-366693, 261123-033304-7f8260, 261123-033307-d5ae57, 261123-033311-0f4e28 (pending); fails without: 261121-002719-b7c57d, 261121-002720-cd7f2d, 261121-002723-86ef33, 261121-002724-b25808, 261121-002725-07f209, 261121-002726-d6fee4, 261121-002727-19a20a, 261121-002752-d9cb1d, 261121-054427-5c7193 |
| A4 | 261123-033252-6cab6f (pending); fails without: 261121-004909-30fad7, 261121-002729-8d49af |
| A5 | `docs/mechanisms/placement-root.md`, `docs/mechanisms/placement-repair.md` (on the quest branch at ae70ff95) |

## Result

Not yet: evidence and CI at ae70ff95 pending; group-heal lands first.

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
- Main taken through group-heal's merge of it (strata:fe2787fb), not `git merge origin/main`: the
  direct merge conflicts only in group-heal's files, and resolving them here would write outside
  the set; group-heal's head carries main 00eb018d with its own resolution.
