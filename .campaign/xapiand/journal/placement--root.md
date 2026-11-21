# placement--root: journal

State: waiting on jobs 261120-181331-7b77ce 261120-181334-01bf69 and 19 mutant jobs
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at strata d339430c (quest/xapiand/placement--root): lead's ruling 1 applied
(namespace keys store `/` as byte 0: path order, subtree [key \0, key \1) after the path);
origin/quest/xapiand/placement merged (CMakeLists.txt is the lead's; tests register through
`test/placement/root.cmake` only). 261120-180504-a88f58 cancelled (old code).

## Next

1. Read 261120-181331-7b77ce (build + root tests, A1); fix failures with a job each.
2. Read the fuzz job 261120-181334-01bf69 and the 19 mutant jobs below.
3. Full suite (`build-test`) on the task branch once those pass.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: 261120-181331-7b77ce (records, placement + model, root group: lone voter and minority loss at 3 and 5 voters); fuzz 261120-181334-01bf69; mutants below |
| A5 | `docs/mechanisms/placement-root.md` (root half; repair's note is task repair's) |

Mutant jobs (`--fails-without`, after 261120-181331-7b77ce): slash-raw 261120-181342-515d5a,
trailing-bytes 261120-181345-8248fd, count-bound 261120-181348-298ca2, leader-voter
261120-181351-117f15, distinct-members 261120-181354-794b7f, distinct-groups 261120-181357-68cb78,
leaders-le-groups 261120-181400-90b926, erase-counter 261120-181404-f2c5db, put-unchecked
261120-181407-1e9c32, corrupt-accepted 261120-181409-900c76, no-expectations 261120-181412-056b51,
remove-busy-node 261120-181415-e9548a, create-existing 261120-181418-d927d6, no-pending
261120-181421-a01f3d, any-state 261120-181424-ef2a30, delete-keeps-load 261120-181427-015feb,
update-no-version 261120-181429-04e99f, leader-by-groups 261120-181432-cf7202, no-witness
261120-181434-ea762a.

Messages:

| Id | Answer |
|---|---|
| msg-1 (scan -> root, key order) | Done as asked (lead ruling 1, Xapiand:8ff0a0f5f): strata d339430c stores each `/` as byte 0, so keys sort in path order (`twitter` < `twitter/user` < `twitter-a`) and `subtree_begin(p)`/`subtree_end(p)` = [key \0, key \1) follow `p`'s key; `path_of` maps back and refuses a raw `/`. Property test in `root_records_test.cc`; note's "Order" paragraph updated. |

## Result

Not ready. Files: `include/strata/placement/{records,placement,root}.hh`,
`test/placement/root*` (`root.cmake` registers them), `docs/mechanisms/placement-root.md`.

## Rulings I made

- Header-only like `strata::group` (target `strata::placement`); no `src/placement/`.
- `CMakeLists.txt`'s two hunks (target, `test/placement/*.cmake` glob) are now the lead's
  (strata:4a42c30c); this task registers tests only in `test/placement/root.cmake`.
- Everything is a record, the application state is one format byte: counters (`D` next node id,
  `G` next group id) are records the leader reads and rewrites under expected versions; the
  directory version is `D`'s strata version (M5); loads (`L` + id) are records so placement reads
  nodes, not 10^6 namespaces. Commands are conditional record writes prepared at the leader
  (D106's writer); apply decodes, validates every value and writes one transaction.
- Placement: preferred leader = fewest leaderships, other data members = fewest groups; D7's
  witness (rf 2 with a third node) and learner (two nodes). Picking every member by groups first
  breaks "at most one leader above the mean" (model: 2 above at one rf, 100+ mixed), so the
  leader slot deviates from "fewest groups"; groups then drift up to 10 apart under mixed rf.
  Accepted by the lead (Xapiand:8ff0a0f5f).
- The leader is members[0] (a preferred leader, not a hint; D5's "no leader hints" holds).
- Node records keep id out of the value (it is the key); Xapiand's M1 fields go in `attributes`.
- Name uniqueness among unretired nodes (M1) is left to Xapiand's join command.
- Bench (`bench/placement/**`) not started: the 10^6-record measurements are the quest's
  "When done" jobs.
