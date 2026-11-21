# placement--root: journal

State: waiting on jobs 261120-180504-a88f58
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Root task built on strata quest/xapiand/placement--root (records, placement, root application,
tests, mutants, fuzz target, mechanism note); first build-and-test job 261120-180504-a88f58 queued.

## Next

1. Wait on 261120-180504-a88f58; fix compile or test failures (none built yet: jobs only).
2. Mutant jobs, one per name in `test/placement/root_mutants.py --list`, each `--fails-without`.
3. Fuzz job: Homebrew clang (`/usr/local/opt/llvm/bin/clang++`) with `-DSTRATA_FUZZ=ON`, run
   `fuzz_placement_root` from `test/placement/root_corpus` for a bounded time.
4. Full suite (`build-test`) on the task branch.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: 261120-180504-a88f58 |
| A5 | `docs/mechanisms/placement-root.md` (root half; repair's note is task repair's) |

## Result

Not ready. Files: `include/strata/placement/{records,placement,root}.hh`,
`test/placement/root*` (`root.cmake` is included by a glob added to `CMakeLists.txt`),
`docs/mechanisms/placement-root.md` (key layout for scan, records, commands, placement, proof).

## Rulings I made

- Header-only like `strata::group` (target `strata::placement`); no `src/placement/`.
- `CMakeLists.txt` (outside the file set): the `strata::placement` target and a glob including
  `test/placement/*.cmake`, so each task adds its own cmake file without editing it again.
- Everything is a record, the application state is one format byte: counters (`D` next node id,
  `G` next group id) are records the leader reads and rewrites under expected versions; the
  directory version is `D`'s strata version (M5); loads (`L` + id) are records so placement reads
  nodes, not 10^6 namespaces. Commands are conditional record writes prepared at the leader
  (D106's writer); apply decodes, validates every value and writes one transaction.
- Placement: preferred leader = fewest leaderships, other data members = fewest groups; D7's
  witness (rf 2 with a third node) and learner (two nodes). Picking every member by groups first
  breaks "at most one leader above the mean" (model: 2 above at one rf, 100+ mixed), so the
  leader slot deviates from "fewest groups"; groups then drift up to 10 apart under mixed rf.
  Stated in the note; the lead may prefer the other trade.
- The leader is members[0] (a preferred leader, not a hint; D5's "no leader hints" holds).
- Node records keep id out of the value (it is the key); Xapiand's M1 fields go in `attributes`.
- Name uniqueness among unretired nodes (M1) is left to Xapiand's join command.
- Bench (`bench/placement/**`) not started: the 10^6-record measurements are the quest's
  "When done" jobs.
