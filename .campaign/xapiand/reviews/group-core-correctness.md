# group-core review: correctness

Commit: strata:6a23026488fa11237cfd7538a7758ac0c90568ea
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: the lead's five questions at strata:6a230264. A7 (a reopen's first write) waits on
detent:consensus-quiescence and is not counted. Two defects block, both small: a replica elected
while stalled on `no_space` never hands leadership off (C1), and a checkpoint started after
`close()` began runs against the index the close destroyed (C2).

## Evidence I relied on

- Suite at head, build-test 261120-105301-a74923 (84 of 84, Release), and crash
  261120-105301-679f4f (`group_crash_lone_voter`, every crash point and a second crash at every
  point of recovery). These cover A1-A6 and A10's cases as written.
- `--fails-without` jobs at c2b656db for A3 and the failed-open row (no-hand-off, superseded-ignored,
  resubmit-superseded, forward-twice, failed-open-keeps-row): group.hh and submit.hh are the same
  at 6a230264, whose only change is index.hh's sealer and t6.
- A1's crash mutants (killed at 1e88eb3f and c99372d4) and B10's append-on-retry and sync-retry
  (killed at 33230b18): each was run once, which the proof standard asks. I did not rerun them.
- seal-repeats killed by `standalone_turn_log`'s `lag` (261120-105335-a6926e).

## What I checked myself

Scripts are in `Xapiand/.scratch/xapiand/review-group-core-correctness/`. Each one patches its
test in the job's checkout and then restores it.

- **TSan, Q2.** Job 261120-111836-ce9ade at head ran `group_lifetime_test` (held, points, reopen,
  failed-open), `group_lone_voter_test 0 3` and `group_voters_test 4` with the named cases. It
  exited 0 with no report. The last green TSan run had been at 47e7f140, before step 9's hand-off
  timer and failed-open close.
- **Q1, a close against what it leaves.** Job 261120-111858-4234e2 (`close_table_probe.py`)
  exited 0. After every `points` close (46 and 41 events, both orders), the journal group is
  `closed` in the host's table and the turn log holds no live record of it. Consensus retires an
  admitted proposal at close without delivering a `Committed` (worker.h `retirement_turn`), so
  no entry is applied after the close's last bundle.
- **Q3, the hand-off.** Job 261120-113145-69776e (`stalled_elected_probe.py`) exited 1, which is
  C1.
- **Q2, calls after close.** Job 261120-112430-782b7a exited 1, and job 261120-113516-c15d90
  (`close_race_probe.py`, libc++ hardening extensive) narrowed it down. Its control, `points`,
  passes. A `read_applied` started after the close answers `consensus group closed` (54 runs) or
  a value (282 runs) and never crashes. A `checkpoint()` traps, which is C2.
- **Q4, the seal change.** It cannot stall a turn or starve a group. `turn_sealer::turn`
  (turn_seal.hh:163-195) runs each group's watcher (`durable_through`, which moves `synced`) after
  `commit()` returns and before the next turn, both under `turn_mu_`. So the first call of the
  next turn always sees `synced >= handed`, and only the repeat calls inside one turn get nothing.
  Each group still gets one record per turn in the driver's rotation, D4's `turn_records = 1`.
  This matches journal 9e11713's CHANGELOG (repeat calls at :40-47, at most one record per turn
  at :72). `strata::group`'s index has no sealer at all, because its options set no `log`.
- **Q5, the pins.** journal:7b64376 (journal-idle-groups: create and reopen durable at the first
  turn) is not an ancestor of 9e11713, so the pinned journal still makes them durable at return.
  `open()` does not depend on either behavior. A crash before the first turn finds no group, or
  still finds it `closed`, and recreates or re-recovers it: `create_deferred` writes nothing
  (group.hh:800-817), and a closed group recovers from the same bundle. consensus 1c08c9f's
  DRIVER.md:138 ("admitted: completes when its record is written") makes a `closed` answer
  in doubt, which matches the mechanism note's "not acknowledged".
- **Read by hand.** I read `superseded`: group.hh:740-746 compares the applied entry's term at
  the placement's index, so a superseded entry can never commit. I read forward-once
  (submit.hh:91-96: one forward carrying the count, then 503) and the waiter bookkeeping
  (`pending`, `recent_`, `outstanding_`).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | defect | The hand-off timer is armed only where `drain()` returns `stall` (group.hh:551). When it fires on a replica that is not the leader, `hand_off()` returns without re-arming (group.hh:886-889), and `RoleChanged` is ignored (group.hh:72). A replica that stalls as a follower, or hands off and is later re-elected, therefore keeps leadership for as long as its stall lasts, and every `quorum` write to the group goes unanswered. Raft elects a stalled follower, because its log is current. This breaks A3's and D3/N15's "a leader stalled on `no_space` hands leadership to a follower applied past it" | Probe 261120-113145-69776e, 3 and 5 voters: the stalled follower takes leadership by transfer, then after 2,000 ms (16 election timeouts) it is still leader and stalled, and a `quorum` write to it is unanswered. The same flow with the stall after the election passes (`stalled_leader`) | Keep the timer armed while stalled, whatever the role: `hand_off()` re-arms whenever `stalled()`, and transfers only when leading. Or re-arm on `RoleChanged` to Leader while stalled. Test: voters_test `stalled_follower_elected` (3, 5), as in the probe. Mutant: a timer that is not re-armed off-leader |
| C2 | yes | defect | `close()`'s last chore item resets `ix_`, closes the file set (`io_ = nullptr`) and drops the directory (group.hh:317-322). `push()` refuses items only once fenced (group.hh:494). A `checkpoint()` started while the close runs queues its capture and `sync_and_commit` after that teardown and dereferences the destroyed index (`ix_->applied()`, group.hh:777) and a null `io_` (`files_.sync` calls `io_->sync_directory()` unconditionally, file_set.hh:91): a crash in Release. A periodic checkpoint racing a catalog close reaches it. `read_applied`'s `fn(*ix_)` (group.hh:255-258) has the same exposure for a read whose `Driver::read` returned before the close; the probe's reads, started after it, were refused by the Driver | Probe 261120-113516-c15d90: with the close at event 0 and the checkpoint 8 events later, `assertion this->has_value() failed: optional operator-> called on a disengaged value`, frame `capture_work` at group.hh:777. The control `points` under the same hardening passes | The teardown marks the group closed under `mu_`. `push()` then aborts each item with `cs::closed`, as it does for a fence, and `checkpoint()` and `read_applied()` answer `closed` without touching `ix_`. Test: lifetime `points` gains a checkpoint and a `read_applied` started at each event after a close; each answers a value or `closed` |
| N1 | no | gap | A4's second mutant (the applied position published before strata's write is visible) was not built. The ruling says it cannot fail `read_applied`, which runs between two entries on the chore. That holds for reads, but `applied()` is public and the hand-off's `applied_past` and hosts read it. A4 is marked met without it | Journal, Rulings ("not built"); quest A4 | Lead: either narrow A4 to the read path or have a host-side observer test |
| N2 | no | gap | A `quorum` wait cannot be cancelled. `propose`'s token covers only the placement (group.hh:232-244). A client of a partitioned old proposer, or of a stalled replica (C1's), waits until this replica applies, fences or closes | group.hh:239-243 | Xapiand's write path needs its own deadline, or propose takes the token through the wait. Owner's call |
| N3 | no | nit | The replacement-pool check (`length_error`) runs after `sync_and_commit`, so a group sized too small syncs and commits strata on every attempt before refusing | group.hh:296-298 | Check before the chore step |
| N4 | no | gap | Recovery checks a named file's length but not an immutable file's CRC32C, which the bundle carries, so a damaged rotated volume recovers silently | file_set.hh:119-128 | Verify sealed files' CRC32C at recovery, or say why not (format-freeze or the durability lens) |
| N5 | no | nit | turn_seal.hh:107 says "Each is durable when it returns" for create and reopen. That holds at journal 9e11713 and stops holding past journal:7b64376. group-core's paths do not rely on it (Q5), but strata standalone's D7 join does | turn_seal.hh:107-115 | Recheck when the journal pin passes 7b64376 (turn-log-parity's file) |

## Not run

- ASan and the Linux sanitizers: CI's jobs at 6a230264 were still running.
- I did not rerun A1's crash mutants or B10's at head, and I did not do a crash run with crash
  points inside one chore step (the harness cuts power between events).
- A2's 40-schedule evidence run, A6's 10^4 footprint and the timing jobs: I relied on the
  contributor's records.
- I did not test a `read_applied` whose `Driver::read` returned before `close()` and whose chore
  item lands after the teardown. C2's fix covers it.
- A7, which is blocked on detent:consensus-quiescence.

## Re-check (ffab229e)

Commit: strata:ffab229eeb897ae0631113cd002a00e701f0fed2
Verdict: APPROVE
Date: 2026-11-20

Scope: the fix round only, `git diff 6a230264..ffab229e` (9 files, +375/-130). Both blockers are
closed and the new mechanisms (N4's CRC32C, A7's closed record and open's hop) hold. Three notes,
none blocking.

### Evidence I relied on (all at ffab229e, exit 0)

Build-test 261120-121525-807eb8, crash 261120-121525-6eba6d, voters named cases with
`stalled_follower_elected` 261120-121535-6b10fe, points with `late` 261120-121536-1b02bc, held
261120-121538-1d9cc7, file set 261120-121539-279b80, reopen-turn 261120-121540-67ed09,
close_race_probe rerun 261120-121553-724c76. Mutants, each killed: handoff-leader-only -121542,
no-hand-off -121544, no-closed-mark -121545, checkpoints-overlap -121547, applied-before-write
-121549, sealed-unchecked -121551. My C1 probe passed at 274fc243 (261120-114759-d6b42f), and
group.hh's hand-off is the same at ffab229e.

### What I checked myself

- **C1, the timer re-arms whatever the role** (group.hh:919-930). It cannot hand off spuriously.
  It transfers only when `state()` says Leader and `applied_past` names a follower past
  `applied() + 1`, which is never the stalled replica, so leadership cannot ping-pong. It cannot
  loop. It returns without re-arming once the stall clears or the group fences (:920), and the
  destructor cancels it (:174). It cannot fight an election. A follower or candidate only
  re-arms, and a transfer the Driver refuses while one is in flight (Busy) is dropped at
  `transfer_on_loop`. The cost is one loop timer per election timeout per stalled group.
- **C2, the deadlock and the window.** Only `checkpoint()` (:275) and `close()` (:286) take
  `checkpoints_`. No chore item and no Driver path awaits it. `Driver::close` (consensus b8de3b8)
  waits on no strata checkpoint: the close's own covering checkpoint has completed, and a racing
  one is parked on the lock before it reaches the Driver. So holding the lock through the
  teardown cannot deadlock. There is no window before the mark. `closed_` is set under `mu_` in
  the same chore item that resets `ix_` (:295-304), and the chore is serial. An item queued
  before the teardown runs against a live index, and one queued after it is refused, at `push`
  (:476-492) or by `drain` taking the queue (:520-521). This holds for front pushes too. A
  `read_applied` waiter linked after `fail_waiters` sees `refused_` (applied_waiter). One
  pre-existing behavior stays as it was: a close of a stalled group waits for space while it
  holds the lock.
- **N4, the CRC32C at recovery** (file_set.hh:129, 149-159). A corrupt sealed file throws
  `file_set_damage` after its truncate and before the unnamed names are removed (:131-133). The
  open then fences and closes its row (group.hh:221-233), and the journal's records past the
  bundle stay. Nothing acknowledged is deleted, but the replica stays down until repaired, which
  needs group-install. A torn final file is unchanged: an unsealed file carries no CRC
  (bundle.hh:94), is truncated to its named length, and fails closed only when short.
  `Crc32c::update`/`value` is the same CRC as the one-shot `crc32c()` the seal uses
  (crc32c.hh:245-264). The test's intact case covers one chunk. The 64 KiB chunk loop is plain.
- **A7, open's hop and the closed record.** `schedule_on` always posts (task.hh:1036-1041). The
  hop now runs only when `current()` is not the loop, so an off-loop start behaves as before.
  Every Driver wake-up posts (driver.h:422-491), and `Driver::open` is synchronous and on-loop
  (driver.h:182-192), so an inline call adds no re-entrancy. A stale record does not leak:
  consensus uses a record only on an exact match of the hard state, the last index and term, and
  the membership (core.h `reopen`), and binds it to the group's incarnation. Probe
  261120-124706-38cbe4 (`stale_record_probe.py`) exited 0 and checked four cases:
  1. A reopen started from the chore scheduler (off the loop) leads in its closed term.
  2. After a restart, the first close's (stale) record opens, elects term 2 past the closed
     term 1, and reads a, b and c.
  3. A damaged record fails the open ("closed state checksum").
  4. The intact record, handed to a new group on the same Driver, then reopens in its closed
     term and reads a, b and c.
- **N3's reorder** (group.hh:807 before :811). It does not change crash behavior. Neither the
  old nor the new order publishes a bundle on the refusal, so recovery starts from the same
  P_b. The only change is that a refused checkpoint no longer commits strata first. `encode(b)`
  reads only the capture, and `sync_and_commit` takes it `const&`.

### Original findings

| ID | Status | Where |
|---|---|---|
| C1 | closed | group.hh:919-930, voters_test.cc:1046, mutant 261120-121542-01253e |
| C2 | closed | group.hh:284-305, 476-500, 520-521; points `late` 261120-121536-1b02bc; mutant 261120-121545-005bf5; probe 261120-121553-724c76 |
| N1 | closed | blob_app `before_write`, lifetime `held` 261120-121538-1d9cc7, mutant 261120-121549-49be3a |
| N2 | open (owner's call, recorded in the journal's Rulings) | group.hh:239-251 |
| N3 | closed | group.hh:807-815 |
| N4 | closed | file_set.hh:129, 149-159; file_set_test 261120-121539-279b80; mutant 261120-121551-0fb5d1 |
| N5 | open (recorded; recheck at journal past 7b64376) | turn_seal.hh:107-115 |

### Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | nit | The mechanism note says a reopen without the record elects. A damaged record, or one from another group or member incarnation, makes the open fail (`Corruption`) instead. The note does not tell the host to drop that record before it opens again | Probe 261120-124706-38cbe4 (damaged record: "closed state checksum"); consensus quiescence.md "Decoding refuses"; replication-group.md's close bullet | Add one sentence to the note: a record that does not decode fails the open, so the host reopens without it |
| R2 | no | gap | N4 covers sealed files only. An unsealed named file is checked for length, not bytes, so damage inside the live volume below its named length recovers silently | file_set.hh:121-129, bundle.hh:94 | Record it with N4's ruling (the bundle carries no CRC for a growing file), or leave it to the durability lens |
| R3 | no | nit | `closed_record_` is empty whenever the Driver is not at rest at close: a call or frame is waiting, or Core's applied lags the checkpoint. The reopen then elects. The result is correct but costs A7's barrier saving | group.hh:293; consensus driver.h:245-252 | None needed; a host test could assert the record is present after a quiet close, as `reopen` already does for a lone voter |

### Not run

- TSan at ffab229e: no new cross-thread state outside `mu_` (`closed_` is read and written only
  under it), so I did not run it. CI's linux-asan-ubsan was still running at ffab229e.
- Multi-voter reopen with a closed record (follower resumes its leader): consensus's own
  `closing` simulation cases cover it. group-core has no such test.

## Re-check (CI fix)

Commit: strata:6421d4b3852390e3062a31d0f9427f685142af36
Verdict: APPROVE
Date: 2026-11-20

Scope: `git diff ffab229e..6421d4b3` (5 files, tests, one CMake test target, docs). No library
header or source changes, so no mechanism the earlier rounds relied on moved. Answer to the
question: nothing is weakened, and t6w keeps everything but the thread it closes on.

| ID | Finding | Blocking | Kind |
|----|---------|----------|------|
| R1 | None blocking. | no | none |
| R2 | The placement-strategies.md "Scripted placement (parked)" section arrived through the main merge (strata:a2619696), not this fix; it is outside group-core and I did not review it. | no | note |

### What I checked

- **t6w (standalone_turn_log_test.cc:1310-1337).** Before: main turned the sealer, a thread
  `owner` ran `ix.close()`. Now: a thread `loop` turns the sealer until `closed`, and main runs
  `ix.close()` then sets `closed`. The roles swap and the rest is the same: the waiter thread still
  parks in `await_durable` first (`waiting() == 1` wait, unchanged), `served(true)` still holds the
  barrier open, the close still races the turns, and the `outcome == 1 || outcome == 2` check, the
  reopen's `index_closed`, the restaged sequence `base + 2` and the new-host recovery are
  untouched. The new `CHECK(turned)` is stricter: the old `until` result was checked inline, and now
  a deadline miss on the loop thread still fails the test. The turn and the close still run
  concurrently, joined before the main thread's own `turn()` calls resume, so there is no overlap
  between two writers of the host's table. Main created the index and ran the first turns, so it
  remains the owner thread the debug assertion names.
- **Pre-fix failure is the assertion, not noise.** 261120-143842-94692c at d1793ae exits 134 on
  host.h:534; the assertions-on full ctest at 6421d4b (261120-143835-d4f803) passes. I added a
  repeat probe, 40 runs of t6w in the assertions-on build at 6421d4b: job 261120-162244-cded19,
  exit 0 (script `.scratch/xapiand/review-group-core-correctness-ci/t6w_repeat.sh`). No
  flake at the changed timing.
- **voters_test case 0 (:440-445).** The draw order is now text then count, which is the order
  clang already used on the recorded seeds, so macOS schedules are unchanged and gcc now matches
  them. The other `gen()` uses in a `+` chain (:460, :465, :822) have one draw each, so nothing
  else depends on operand order. The mutant killed at 6421d4b (261120-143852-9d288c, exit 1) shows
  the test still bites.
- **group_footprint.** In an unsanitized build the counting allocator and the test body are
  byte-identical to before (the `#ifndef` arm is the old code, with `traces` hoisted). The
  `#else` arm only defines the names the body references, and `main` returns 77 first, mapped to
  SKIP by `SKIP_RETURN_CODE`. A real failure cannot return 77 (the test returns 0/1). The cost
  is that the A6 heap counts are not exercised under ASan/TSan, which they never could be (the
  sanitizer owns `operator new`); the Release and assertions builds still run them (143826,
  143835). Backtrace is linked through `find_package(Backtrace)`, optional, and `execinfo.h` is
  guarded.

### Not done

- I did not run FreeBSD or gcc myself; the jobs above are macOS. The gcc fix and the FreeBSD link
  are the contributor's claim, to be confirmed by CI.
