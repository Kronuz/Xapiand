# group-heal--repair: journal

State: blocked on lead: the task's file set (`src/group/repair*`, `test/group/repair*`) cannot hold A3
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

## Doing

Nothing. Stopped before writing code: every A3 path needs files outside the set (below).

## Next

1. Once the lead widens the file set (option (a) below): build A3 as designed below, against
   consensus:59ae07f through the quest's override (`--ref consensus=59ae07f`,
   `CONSENSUS_DIR=$JOB_REF_CONSENSUS test/group/install_suite.sh`), no pin change.

## Blocked on

The lead: the file set. strata::group is header-only: `src/group/` does not exist (the witness task
had `include/strata/group/witness*`). Beyond the glob, A3 cannot be built outside group.hh:

- A damaged strata segment or manifest throws `corrupt_index` from `index::open` in
  `group::open_index` (group.hh:194), so no index exists to install onto. It must catch that, put
  strata's names aside, open a deferred index and mark the group damaged.
- Recovery on a damaged replica fails in `adopt` (group.hh:1105, P_b > P_H) and in
  `files_.recover` (`file_set_damage`, a sealed set file's CRC32C). A damaged recovery must skip
  both, hold apply, refuse reads, and ask for the image.
- Fencing today is `application_failed` (group.hh:1341), which fences consensus's Core for good;
  `Driver::reinstall` needs an unfenced follower with no delivered range outstanding
  (core.h@59ae07f:885: `delivered_through_` must be 0), so the damaged group must acknowledge each
  delivered range unapplied (`applied`) and call `reinstall` on the loop: new group state.
- A damage found at a read (`read_applied`, a segment block's checksum) must fence the same way.
- install.hh's `activate` moves staged `ia.` files in without a CRC32C check (the durability
  review's Not-run note, which it assigns to A3's fencing).
- The test target needs a line in CMakeLists.txt (`group_repair_test`, `add_test`).

Options:
- (a) Widen the task to `include/strata/group/repair*`, `include/strata/group/group.hh`,
  `include/strata/group/install.hh`, `test/group/repair*`, `CMakeLists.txt` (the test target
  only) and `docs/mechanisms/group-install.md` (a Repair section). group.hh is also placement's,
  which lands after this quest. Recommended.
- (b) Keep the set: impossible. A host-side helper in `repair.hh` that wipes the replica and
  reopens still fails `adopt` (consensus restores the base from its journal), and the runtime
  read path is group.hh's.

Design for (a), in two lines: damage (strata `corrupt_index` at open or read, `file_set_damage`)
fences the group locally only (reads and apply refused, delivered ranges acknowledged unapplied),
and the group calls `Driver::reinstall`, which installs the leader's base (at or below commit)
through install's own stage and activate, CRC32C-matching files kept, damaged ones fetched whole;
`RepairCompleted` unfences it, readable once applied reaches P_H. A damaged leader transfers
leadership first (`LeaderMustTransfer`); `Current` (one image per base) or `NotLeader` retry on the
next leader or leader checkpoint.

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | none: blocked on the file set |

## Result

None yet.

## Rulings I made

None.
