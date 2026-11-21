# group-heal--install: journal

State: waiting on jobs 261120-181817-a0cc80 261120-181820-54034f 261120-181823-382ea2 261120-181826-ed7951 261120-181829-f2050f 261120-181832-5929d9 261120-181835-53159e 261120-181838-e03e4e 261120-181842-4fc87c
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1 done at strata:0e67dcb7 (pushed, quest/xapiand/group-heal--install): the install transfer,
transport-agnostic, `include/strata/group/install.hh`; tests `test/group/install_test.cc`
(values, retry, held, damaged, threads, crash); `test/group/install_build.sh` builds it in a
wrapper project (CMakeLists.txt is not mine); `test/group/install_mutants.py` (8 guards);
`docs/mechanisms/group-install.md`. Local Release run: all cases ok; crash 720 runs over 71
operations (565 old, 155 new) plus the activating recovery crashed at each of its 30.

Found and fixed while building: activation removed the staged manifest log and its segments in
one directory epoch; a power loss kept the log and lost a segment, and the repeated activation
failed ("missing file: seg-1.sx", crash at 56 and activation crash at 15). The log now goes,
synced, first. Mutant holder-unordered.

## Next

1. Jobs (State). Then the binding, once Blocked on clears.

## Blocked on

1. consensus (Detent's lead): the Driver has no install surface. A Driver group runs no
   SnapshotChannel and its Worker is private (driver.h: `Group::worker`, no accessor; transport's
   `PeerHost` needs `worker()` and `channel()`, which only a bare Worker host has). Missing, at
   consensus b8de3b8 and main 8801b99 alike:
   - the leader's flight: `SnapshotNeeded` -> source ready -> `SnapshotTransmit` reach the Sink,
     but nothing reads `Worker::source_chunk` or frames the transfer (RFTXFR01) for a Driver
     group; the host transport carries no transfer frames to or from a Driver group;
   - the receiver's frames (`reserve_snapshot`, `offer_snapshot_chunk`) and, needed here, the
     application's semantic validation (`validation_chunk`, `consume_validation`,
     `validation_succeeded`, `reject_snapshot_validation`): install stages its whole transfer
     there, before consensus persists the install, which is what makes a crash leave the old
     replica or the complete new one;
   - the image at activation: `ActivateInstall` carries a token and a boundary, not the bundle.
   driver_simulation.cc says it outright: "the simulation sends no installs".
   Asked: a Driver install surface (frames in/out per group, a validation notice with
   validated/validation_failed calls, the image readable at ActivateInstall), or a ruling that
   strata binds a SnapshotChannel itself (then the Driver must expose the group's Worker).
   Until then A1's add-learner/promote/replace/remove after compaction and A2 cannot run.
2. Lead: files outside this task's set, for the binding: `include/strata/group/group.hh`
   (dispatch's ActivateInstall and SnapshotNeeded, `resume` first in `recover`, the leader's pin
   on the chore with `release_pins` at open, `unlink_retired` held below a live pin's P_b, the
   membership steps), `CMakeLists.txt` (register `group_install_test`; until then
   `test/group/install_build.sh` builds it in a wrapper project), `test/group/voters_test.cc` and
   `test/group/mutants.py` (A2's schedules settling by install, the drop-SnapshotNeeded mutant).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 (transfer half) | 261120-181817-a0cc80 install_test all cases at 0e67dcb7; `--fails-without` mutants: skip-file-set 261120-181820-54034f, keep-unchecked 261120-181823-382ea2, write-past-failure 261120-181826-ed7951, marker-before-sync 261120-181829-f2050f, holder-unordered 261120-181832-5929d9, no-resume 261120-181835-53159e, pin-names-unchecked 261120-181838-e03e4e, serve-past-length 261120-181842-4fc87c |
| A1 (learner promoted, replace/remove across leader changes) | blocked: Blocked on 1, 2 |
| A2 | blocked: Blocked on 1, 2 |
| TSan (`threads`) | CI's linux-tsan runs ctest; group_install_test reaches it only once CMakeLists.txt registers it (Blocked on 2). No local sanitizer job, per the lead. |

## Result

Transfer built and proven in isolation (docs/mechanisms/group-install.md). Not bound to the
group or consensus: see Blocked on.

## Rulings I made

- consensus pin stays at b8de3b8 (consensus-quiescence, which group-core needs for
  closed_state/reopen); its snapshot headers (snapshot_channel.h, snapshot_transfer.h,
  snapshot.h, membership.h) are byte-identical to consensus main 8801b99, so "build on main"
  holds for the install surface. CMakeLists.txt is not in this task's files either.
- Bytes: the pin carries a CRC32C per file (the leader reads each live file and segment once at
  the pin); one check for held prefixes, torn staged bytes and another leader's same-named file.
- Staged strata files are written twice (staged, then strata's install copy): strata's
  `install(source, generation)` reads from an IO, so staging is a prefixed view of the same
  directory. Stated in the doc's costs.
- Staging names `is.`/`ia.`/`ip`: a set name (max 251) plus `ia.` stays within NAME_MAX.
- The window writes answers in order and none after a failed one, so a retry fetches exactly the
  lacking bytes (checked to the byte in `retry`).
- msg-175: fetch errors (timed_out, io_error, any other) and not-found all end `install_failed`
  carrying the code; the retry keeps the staged bytes. `retry` drives timed_out and io_error.
