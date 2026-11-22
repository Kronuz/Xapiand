# group-heal--install: journal

State: waiting on jobs 261121-173201-4aa79d 261121-173206-d6917f 261121-173210-076626 261121-173215-20e4f0 261121-173219-70b405 261121-173223-4bca51 261121-173227-05623f 261121-173231-b9c5e6 261121-173235-40775c 261121-173239-97acf3
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged into quest/xapiand/group-heal at strata:2b080579, main merged at c79cf24e, both pushed;
the task-head jobs 261121-172551-9db0a4 / 172613-7a078d were cancelled for the quest-head ones
(State; the quest journal lists them). Proof now lives at the quest head.

Build fix 2 (strata:167f77ed, f05b6672): 261121-133859-cf44a4 failed to compile at 95bdaa2e:
footprint_test's `row` still overrode the old one-argument `install_needed`, so it left the
binding's four pure notices (install_needed(peer, key), install_released, validate, activate)
unimplemented. Implemented as empty (a row opens without `snapshots`). 261121-133923-ba5a0d (the
install-silent `--fails-without` at mutant 89f2f507) "passed" on that same compile error, not on
a failed test: a false kill, and its regex `^group_voters_test$` names no ctest test (ctest's is
`group_voters`), so even a build would have run nothing of voters_test. Not evidence.
install_suite.sh now exits 127 on a failed configure or build and errors on a regex matching no
test; install_build.sh retired (group_install is registered), install_mutants.py rebuilds in a
configured tree. Jobs: plain 261121-172551-9db0a4 (A1, A2 at f05b6672, consensus 8780e44);
mutant install-silent 261121-172613-7a078d at strata:3ae7d70d
(refs/mutants/group-heal--install/install-silent), voters only, after the plain.

Step 2 (binding) written at strata:87bc0ebf, against consensus's driver-install branch
(origin/quest/detent/consensus-driver-install at 887a88b; contract DRIVER.md Install,
consensus:2c598fb): `group.hh` pins at SnapshotNeeded, serves, stages at ValidateImage, activates
at ActivateImage through recovery's `adopt`; loop gains `send_transfer` and `fetch`.
`voters_test.cc`: transfer frames and fetches on the seeded net; install_gap settles by install;
ungated seeds 8-27 + 101-140 settle (A2); `grow_and_heal` (A1 membership). First build:
261121-055544-08fada (`test/group/install_suite.sh`, CONSENSUS_DIR from `--ref`). Nothing has
compiled yet: expect fixes.

Step 1 (transfer, strata:0e67dcb7): pin-names-unchecked survived (261120-181838-e03e4e): `values`
only flipped a byte (CRC fails first). Fixed at strata:acca8f77: re-encoded pins naming `../x`,
`app.vol-1`, `ip` as strata files are refused. Rerun: 261121-054230-6f2ce0, mutant
261121-054233-ce72e0.

Build fix (strata:23f4dff4): 261121-055544-08fada failed to compile. `release_pins` passed
nullopt to `read_generation` (takes a generation): now `index_detail::read_state(dir).pins`, the
newest state. `same_digests` moved above `install_gap` (used before its definition). Jobs:
261121-084738-bee937 (install_suite.sh, A1/A2, consensus@887a88b); 261121-084747-cde37d
(install-silent, `--fails-without` at strata:6fa31367 on refs/mutants/group-heal--install/install-silent,
voters_test only, after the plain).

## Next

1. Jobs (State), at the quest head c79cf24e: the plain 261121-173201-4aa79d must pass; the
   install-silent mutant and the eight install_mutants must fail on their tests.
2. Mutant reruns only if the lead asks (finished results stand).
3. Rerun the install_mutants at the head (they ran at 0e67dcb7) with STRATA_CMAKE_ARGS for the
   consensus override, since install_test includes group.hh through oracle.hh.

## Blocked on

Nothing. 1 (the pin and test registration) done by the lead at strata:95bdaa2e: consensus pinned at
driver-install's landing (consensus:8780e44, no install API change since 887a88b), and
`group_install` registered in ctest beside `group_voters`.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 (transfer half) | 261120-181817-a0cc80 install_test all cases at 0e67dcb7; `--fails-without` mutants: skip-file-set 261120-181820-54034f, keep-unchecked 261120-181823-382ea2, write-past-failure 261120-181826-ed7951, marker-before-sync 261120-181829-f2050f, holder-unordered 261120-181832-5929d9, no-resume 261120-181835-53159e, serve-past-length 261120-181842-4fc87c; pin-names-unchecked survived (261120-181838-e03e4e), fixed at acca8f77: 261121-054230-6f2ce0, mutant 261121-054233-ce72e0 (pending) |
| A1 (learner installs, promoted, replace/remove across leader changes) | pending at c79cf24e: 261121-173201-4aa79d (voters_test `install_gap`, `grow_and_heal`; group_install); install_mutants 261121-173210-076626 (skip-file-set) .. 173239-97acf3; 261121-133859-cf44a4 failed to compile at 95bdaa2e (fixed f05b6672) |
| A2 | pending at c79cf24e: 261121-173201-4aa79d (ungated seeds 8, 12, 16, 18, 23, 24, 27, 101-140); mutant install-silent 261121-173206-d6917f; 261121-133923-ba5a0d is a false kill (compile error), not evidence |
| TSan (`threads`) | CI's linux-tsan runs ctest; group_install_test reaches it only once CMakeLists.txt registers it (Blocked on 1). No local sanitizer job, per the lead. |

## Result

Transfer built and proven in isolation; binding written, unproven (docs/mechanisms/group-install.md,
The binding). Pin bump and test registration: Blocked on 1.

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
- One pin per bundle position, shared by its flights ((peer, transfer) set): two pins at one
  P_b would name different generations, and the learner's chunk names must match the pin it
  fetched. Only the latest bundle is pinned; an older key pins nothing and the flight fails.
- The stage holds the chore (`installing_`, set by a chore item so it starts between entries):
  the chore is a work group in production, so a stage beside apply could race the set's files.
  DRIVER.md already holds checkpoints and repair behind an incoming image.
- Activation reuses recovery's step (`adopt`, split out of `recover`): strata stays open
  (`index::install` on the live index), so the group keeps no strata options past open (the
  footprint test refuses a kept config).
- `recovered()` now names the replica's latest bundle (restored, installed or published): the
  leader pins it, and lifetime_test reads it only right after a reopen.
- `dir_` is a shared_ptr: a pin carries it so a serve racing the teardown reads on.
- voters_test's oracle checks the log merged from every replica (agreeing where they overlap,
  no index unapplied) instead of node 1's: an installed replica applies only past its install.
- mutants.py: `checkpoint-ungated` removed (its premise, that an ungated checkpoint strands a
  replica, no longer holds); `install-silent` repointed; `publish-before-sync`,
  `commit-after-publish` repointed at the new unlink line; `destroy-keeps-member` repointed
  (it no longer applied on main: the destructor gained the hand-off cancel).
- footprint_test.cc is outside this task's globs but inside the quest's (test/group/**): the
  binding's pure notices break its `row`, so the fix goes with the binding. Empty overrides, not
  default no-ops in `member`: a member that forgets one should not compile.
- Jobs pin consensus at 8780e44 (CMakeLists.txt's pin, consensus main), not 887a88b: evidence
  then matches what CI fetches.
