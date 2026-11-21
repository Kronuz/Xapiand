# group-core: journal

State: waiting on jobs 261120-143826-fc1ff1 261120-143835-d4f803 261120-143838-68581e 261120-143842-94692c 261120-143852-9d288c (step 12, CI fix at strata:6421d4b3)
Quest: .campaign/xapiand/quests/group-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 12 (lead: CI run 44816782044 red at d1793ae0), strata:6421d4b3.

- All five named jobs fail only `standalone_turn_log`, aborting at journal host.h:534
  (`owned_ranges`: one thread per loop's range table, debug builds); linux-x86_64-gcc also fails
  `group_voters`. Local gate jobs build Release (NDEBUG), so they never saw the assert.
- Assertions-on repro at d1793ae0 (RelWithDebInfo -O2 -g, each case alone) 261120-143205-ce4513:
  only `t6w` aborts (exit 134); every other case and `group_voters` pass. Cause: the test, under
  journal 9e11713's check (journal:0fc8c14, arrived with the quest's pin move cc95ba6 -> 9e11713):
  t6w creates the index on the main thread and closes it on a second, so `turn_sealer::close` ->
  `Host::close_group` -> `publish` -> `owned_ranges` runs off the table's first thread. No
  strata::group code is on the path; the group tests pass with assertions in CI.
- Fix (6421d4b3): t6w closes on the main thread, a second thread writes the turns. Fails without:
  t6w at d1793ae0, assertions on, 261120-143842-94692c.
- group_voters on gcc x86_64 (seeds 8, 12, 16, 23 named nobody behind, both runs 44797677012 and
  44816782044; arm64 gcc and clang pass): the C command's `"C|" + ... + text(gen, ...) + "|" +
  to_string(gen() % 3)` draws in unspecified order, and gcc x86_64 draws the right one first, so
  its schedules differ from the ones D109's seeds were picked on. Fix: draw the body first (the
  order clang uses). Fails without: the reversed order on clang, 261120-143852-9d288c (expect
  seeds 8, 12, 16, 23 FAIL, as on CI).
- Jobs at 6421d4b3: gate build-test 261120-143826-fc1ff1, crash 261120-143827-809109 (green); full suite
  assertions on 261120-143835-d4f803; ASan+UBSan on standalone_turn_log and group_* 261120-143838-68581e.
- Touches two files the correctness review (accepted at ffab229e) saw, both tests:
  test/standalone_turn_log_test.cc (t6w itself is main's, unchanged by the quest) and
  test/group/voters_test.cc (one draw's order). No include/ change.

Step 11 (lead: correctness review CHANGES at 6a230264, `reviews/group-core-correctness.md`;
rulings 1-7), strata:274fc243 e7b9acd2 429b4976 ffab229e. Jobs: Evidence, all at ffab229e
unless named.

- C1 (274fc243, e7b9acd2): `hand_off()` re-arms while stalled whatever the role and transfers
  only when leading (chosen over re-arming on `RoleChanged`: no new Sink path, and it covers a
  stall that starts as follower and every later election; cost one loop timer per election
  timeout while stalled). voters_test `stalled_follower_elected` (3, 5). Mutant
  handoff-leader-only (the old arming). The reviewer's probe at 274fc243 (before the test
  existed, since the probe adds a case of that name): 261120-114759-d6b42f, exit 0.
- C2 (ffab229e): the teardown sets `closed_` under `mu_`; `push()` and `drain()` refuse later
  items, a call with `cs::closed`. `close()` holds the checkpoint serializer from its first
  checkpoint through the teardown (new: a checkpoint racing the close used to run between the
  close's checkpoint and its `Driver::close`), so a late checkpoint waits and is refused at its
  capture. lifetime `points` adds `late`: close at every event, a checkpoint and a read at each
  event after it until one past the close's end (1,849 + 1,318 runs, 4.8 s locally): each a value
  or `closed`, `closed` once the close finished. Local: late checkpoint closed 3,167 of 3,167,
  late read closed 261 (the rest values, replay-checked). Mutant no-closed-mark (local: killed,
  the late checkpoint answers a value in Release). close_race_probe rerun 261120-121553-724c76.
- N1 (ffab229e): blob_app `before_write` (in `transaction()`); lifetime `held` holds A's apply
  at the strata write and checks from the host thread `applied() < ` the held entry. Mutant
  applied-before-write (store before the strata phase; local: killed, "2 at entry 2").
- N3 (ffab229e): the pool check moves before `sync_and_commit` (bundle encoded from the capture,
  which the sync does not change).
- N4 (429b4976): `file_set::recover` reads each sealed file through its length (64 KiB chunks,
  incremental CRC32C) and fails closed on a mismatch; file_set_test flips each of six bytes.
  Mutant sealed-unchecked.
- A7 (ffab229e): consensus pin 1c08c9f -> b8de3b8 (journal 9e11713, completion d8b538c, task
  95798a0 are b8de3b8's own pins already, unchanged). `close()` keeps `Driver::closed_state()` as
  `closed_record()`; `config.closed` hands it to `Driver::recover`. `open()` hops to the loop
  only when started off it: with the hop, a write started after it reached the Driver before
  `Driver::open` ("consensus group closed"), and one issued after the recover's turn was
  submitted cost 3 (consensus's "a request issued after the recover"). `reopen` now checks the
  reopened voter leads in its closed term (in-process and after a restart); `reopen-turn` issues
  the reopen and the write together: 2 barriers, an open group's 2 (local). New ctest
  `group_lifetime_reopen_turn`. The pin returns to consensus main when quiescence lands there.
- Green at ffab229e: crash 261120-121525-6eba6d, voters named 261120-121535-6b10fe, points
  261120-121536-1b02bc, held 261120-121538-1d9cc7, file set 261120-121539-279b80, A7 reopen-turn
  261120-121540-67ed09. Still queued: State.
- Mutant anchors moved: no-hand-off (indent), checkpoints-overlap (now in `checkpoint()`);
  rerun as jobs. Local: all six new or moved mutants killed.
- N2, N5: recorded only (Rulings), per ruling 6.
- The reviewer's failed probe jobs 261120-112100-73067d, 261120-112430-782b7a,
  261120-113145-69776e, 261120-113516-c15d90 are C1's and C2's findings at 6a230264 (expected
  red), superseded by the reruns above.

Step 10 (lead: build-test 261120-092528-6f13a3 red on `standalone_turn_log`), strata:6a230264.

- Introduced by group-core's journal pin move (cc95ba6 -> 9e11713), not pre-existing: the test
  x3 at strata main d956c0f4 (main's pins) passes, 261120-104308-ba5900 (126 records, 126 turns,
  worst lag 2); x3 at c2b656db fails all three, 261120-104206-ce0c3a (t6 at 1199/1200, `lag`
  worst 3, 130 records over 126 turns). turn_seal.hh:103 is not the cause.
- Two journal changes in the range: (a) journal:6187a32 calls a sealer again while it returns
  records in a turn, so the index's seal() handed the checkpoint and the open batch in one turn
  (`lag`); (b) retired() is answered from the host's table, so a new host retires a closed group
  through its close's sequence + 1 and t6's reopen restages at base + 2 (as t6w already says).
- Fix (6a230264): index.hh seal() hands nothing while its last handed record is not durable (one
  record per turn, turn_records); t6 asserts retired == base + 1, sequence base + 2, apply base + 3.
  Mutant seal-repeats (test/group/mutants.py) kills `lag` (local: worst 3). Local full
  standalone_turn_log_test ok.
- Jobs: build-test 261120-105301-a74923, crash 261120-105301-679f4f, seal-repeats kill check
  261120-105335-a6926e (plain job: passes when the test fails; `--fails-without` needs a
  criterion and this is the suite's, not A1-A10's).

Step 9 (lead: A3's unbuilt cases and the failed-open row close), strata:c2b656db. Jobs: Evidence.

- No_space hand-off (D3, N15): a stalled leader arms a loop timer (`wheel::timer_entry` in the
  group, armed only while stalled) for one election timeout (`timing.election_max`); at it, if
  still stalled and leading, it asks `loop::applied_past(id, applied + 1)` (new host hook: the
  host's view of its followers, health's later) and `transfer_leadership`s to the answer, then
  re-arms while the stall lasts. voters_test `stalled_leader` (3, 5): leader's budget spent, its
  write stalls, a follower applied past it takes over and answers a write, the old leader's
  `quorum` answer waits, then answers `accepted` once space frees; digests equal. Mutant
  no-hand-off.
- `superseded` required: voters_test `superseded_entry` (3, 5): the leader is cut off inside its
  request's prepare (past the ready point), places it alone, the others elect and commit; healed,
  the request answers `superseded`, 0 forwards, its command in no node's log. Mutants
  superseded-ignored (waiter takes another term's verdict), resubmit-superseded (writer loops).
- Forward once: voters_test `forward_once` (3, 5): a follower's merge forwarded to the leader is
  answered `accepted`, 1 forward; forwarded to another follower it answers 503, 0 placements, 1
  forward. Mutant forward-twice (`if (forwarded)` -> `if (false)`).
- Failed open closes its row: `open()` catches a create/recover or chore-recovery failure, fences
  (inside the chore before any range applies, then on the loop), `Driver::close`s the row and
  rethrows. lifetime_test `failed-open` (in ctest's default run): a reopen over a directory that
  lost strata's index fails `P_b > P_H`, `groups() == 0`, then the same process reopens over the
  real directory and reads a and b. Mutant failed-open-keeps-row.
- Pre-existing at 33230b18, fixed (Rulings): voters_test `default_limits` fails at consensus
  1c08c9f ("worker batch exceeds the host's turn room", D132): it set all of `cs::Limits{}`.
  Now only the default log (log_bytes, log_entries), the test's wire limits kept. So
  build-test 261120-090009-f2e837 at 33230b18 is expected red on `group_voters`.
- Local (light holds): `group_voters_test 0` ok (write path: 1-2 superseded per 8 seeds, as
  before), `group_lifetime_test failed-open` ok. Not run locally: crash, full ctest (jobs).
- Lead's contract note (journal-idle-groups, journal:7b64376: create and reopen durable at the
  first turn, not at return): no group-core case reads durability at `open()`'s return. Every
  case writes (a turn) before a restart or crash it checks; A1's crash points between create and
  the first turn recover to "no group" and create again, nothing acknowledged. `reopen-turn`
  (A7) counts the reopen's barriers with its first write, which this contract matches. Nothing
  to fix before the pin moves; recheck A1's crash run then.

Step 8 (lead's resume under agent-campaign:21248b4: merge main, move pins, fix the design review's
blocking findings, A7 and A8), strata:33230b18. Jobs: Evidence.

- Merged strata origin/main (0c44798a; merge, no rebase). Conflicts: CMakeLists.txt takes main's
  completion block (COMPLETION_TASK beside completion, Asio off); index.hh keeps create_deferred
  public (D112) and takes main's `ix.attach_log(true)`. group-core's diff to main in index.hh is
  only that move: create_deferred leaves `private:` (main 5704-5725) for `public:` after
  `create` (840-865), with two comment lines on D112.
- Pins (57ca3198): consensus 57745e8 -> 1c08c9f (main: close-fenced, wave-open, turn-room
  refusal), journal cc95ba6 -> 9e11713 and completion 7731315 -> d8b538c (consensus's own pins).
  Two follow-ons: journal's `Host::entry` returns an optional, so `turn_seal.hh:103` holds it by
  value (one line, turn-log-parity's file; Rulings); consensus refuses a worker batch past one
  turn-log segment (D132), so the group tests' `rpc_bytes` drop 16384 -> 8192 (16 KiB segments
  kept, their rotation coverage with them). Full build green locally; footprint ok (closed 2,945 B,
  destroyed 194 B, 0 descriptors).
- Design review (round 2 re-check, CHANGES): B10 is the only blocking finding. Its fix is built
  (file effects positional, `file_set::write` at an offset, never appending; a failed checkpoint
  sync fences): mutants append-on-retry (A1, and A2's file-set ENOSPC once) and sync-retry,
  requeued at 33230b18. N16 and N17 are nits; N17 is A3's open `superseded` case (Next).
- A5 and A7 (33230b18): `group_lifetime_test reopen` closes a lone voter, checks the host's table
  holds it closed and the turn log no live record, reopens it in the same process (consensus now
  erases the closed row), closes, restarts and reads a, b and c. A7's equality is `reopen-turn`
  only: at consensus 1c08c9f the reopen spends 0 barriers in `open()` and its first write 5,
  against an open group's 2 (the reopened voter elects, then a NoOp, then the write).
  detent:consensus-quiescence (reopen without election) is still `planned` in Detent's state, so
  A7 stays blocked; the gap job records it.
- Home files on quest/xapiand/group-core beyond the journal, against master (nothing edited):
  `.campaign/xapiand/reviews/group-core-design.md` identical. `docs/design/cluster-release.md`
  differs: the branch has the three design revisions (faf31a506, 904f8c6a7, 2fea83c16; +320 -46
  against the merge base), master has the D113 answer line and D141's multicast answer (6 lines)
  the branch lacks. `docs/design/cluster-release/bundle_model.py` and `verdict_model.py` exist
  only on the branch. Round 3's revision (2fea83c16, B10's text) was never re-reviewed.
- A8: decided, D113 (owner 2026-11-17 took cluster-release Q2 (a)).
- `read-skips-probe`'s anchor had gone stale (`config_.id` -> `id_` at the keeps-config cut);
  repointed, job requeued. Every other mutant's anchor applies at 33230b18.

Step 7 (lead's resume step: build-test red at 47e7f140, 261117-174709-007299), strata:88d72629.

- The 64 B "a destroyed group leaves of ours" at 64 groups is the test's chore scheduler, not
  the group. Counter (`GROUP_FOOTPRINT_SIZES=1`, job 261117-205530-eb24be, live blocks by
  requested size, groups' window before->destroyed against the rows' rows-before->before plus
  destroyed->rows-closed): 4096 B blocks groups +2, rows +1 (+4,096 B = 64 B x 64); hash bucket
  arrays 776 -> 1576 B (97 -> 197 buckets, the loop's and the Driver's tables) +48 B net; the
  rest under 40 B. Stacks (`GROUP_FOOTPRINT_TRACE=4096`, job 261117-213444-278a4b): both blocks
  of the groups' window are `std::deque<task::detail::queued_call>::__add_back_capacity` under
  `group::kick` -> inline_scheduler: task's inline_scheduler queues in a deque that keeps its
  4 KiB blocks once grown, and no bare row runs on the chore, so the chore queue's first blocks
  fell in the groups' window. One-off per scheduler (4 B per group at 1,000 closed, hence 10^4
  passing). Fix (test only): both schedulers' queues are warmed (1,024 no-op tasks each) before
  any window; mutant cold-schedulers (`--fails-without` 261117-220123-40de8b); threshold
  `gone_slack` 16 B unchanged. Breakdown with the fix: 261117-220126-1624db.
- malloc_history (`footprint_owners.py`, jobs 261117-201244-d391d7, 261117-202428-f2baa9) was
  not usable for this: its phases are separate lldb runs under MallocStackLogging, and their
  Core/Worker counts disagree with the test's own counter (a closed row's Core read as live).
  The in-process counter and stack tracer replace it for window attribution; the script keeps
  `--terms` and a `rows-before` phase.
- "group, destroyed open: +6,787 B; its row stays open in the Driver": not a group-core leak,
  and the measurement broke the group's lifetime rule (docs/mechanisms/replication-group.md,
  Lifetime: destroyed after `close()`, or once fenced and drained). The 6,787 B is consensus's
  open row (bare row idle 6,880 B at 10^4): `Driver::groups_` holds a row until
  `Driver::close(id)` and frees it at `~Driver`; even a closed row stays (921-971 B) until a
  reopen or `~Driver` (driver.h `open`, `close_step`, `~Driver`). That is consensus's contract.
  The group cannot close its row in a destructor (the close is async: checkpoint, then
  `Driver::close`), so destroy-while-open stays a precondition, not a refusal. The footprint
  now fences its open groups before destroying them, as a stopped process does (strata:
  c0622adf), and labels the row consensus's. Two in-process gaps follow; see Blocked on.
- `campaign brief` outside the file set: strata `include/strata/core/index.hh` (c99372d4, D112:
  `create_deferred` public, the first checkpoint commits generation 1; the group's index is
  created through it). Home, on quest/xapiand/group-core: `.campaign/xapiand/reviews/group-core-
  design.md` (design reviewer's rounds 1-3, f27c26cb6 edb610078 4f0c74bdb); `docs/design/
  cluster-release.md`, `docs/design/cluster-release/bundle_model.py`, `verdict_model.py` (the
  design rounds' revisions answering that review: faf31a506, 904f8c6a7, 2fea83c16). Nothing
  moved or reverted; the lead decides.

Step 6 (A6 closed and destroyed, lead's step after c99372d4's jobs), strata:47e7f140; jobs
queued (Evidence). Step 5's jobs at c99372d4: 11 mutants killed, TSan, build-test, crash green.

- Closed was a measurement bug, not the close. The 10^4 job (261117-151206-b86dca) closed 100
  of 10,000 and divided the "closed (all)" row by all 10,000, so 9,900 open groups' index
  descriptor and frame read as 0.99 per group (0.80 = 400/500 at 500, 3cedb9). Its checks
  subtracted the open ones and passed. From its own numbers a closed group held 0 descriptors,
  0 frames and 3,559 B ((12,370 x 10^4 - 12,459 x 9,900) / 100; 3,565 B at 500); "destroyed
  6,742 B" mixed 9,900 groups destroyed open (their rows stay open in the Driver, about 6.8 KB)
  with 100 closed ones (about 1.2 KB). Fixed in the test (strata:47e7f140): closed and destroyed
  rows are per closed group, the closed ones destroyed first.
- Second measurement bug: "bare index, closed 1,928 B" was the test's own vector of bare
  indexes, cleared but not freed (malloc_history at phase bare-closed: 2,048 B per index under
  that vector, 3 B of strata's). Freed now. The frame pool is off in the footprint build
  (`TASK_FRAME_POOL=0`, one TU, ODR-safe: every dependency is header-only).
- Split: bare rows, as many as groups closed, checkpoint once and close as a group does
  (`close_row`); "consensus row, closed" is consensus's and the journal's part; the group's
  own closed = closed - row - index closed; ours after destroy = destroyed - row - index closed.
  Checks: closed ours <= sizeof(group) + own_heap; destroyed ours <= 16 B (the loop's bucket
  array); every destroyed group leaves its loop's table (mutant destroy-keeps-member).
  Local, before the row checkpoint and the vector fix (64 groups, 32 closed): closed 3,628 B,
  0.02 descriptors (host turn log), 0 frames; closed and destroyed 1,045 B; a row closed without
  a checkpoint 1,417 B. Numbers at 47e7f140 come from the jobs; the host's light slots were
  held behind an exclusive, so nothing ran locally at the fix.
- `footprint_owners.py --at PHASE [--base PHASE] [--closes N]`: owners at any phase
  (bare-closed, before, idle, closed, destroyed, rows-closed), or past another.
- Timing: strict at 500 and 1,000, 10 closes each (1,500 hit the 10-minute cap: about 311 ms
  per open under load). Flat in N: compare the idle minute's CPU (total, not per group: 0.149
  and 0.155 ms at 10^4 and 500 at c99372d4) and the open's ms per group.

Step 5 (D111, A6 as written; D112, deferred create), strata:1b1601be, ba578486, c99372d4.

- A6, one directory per group: the file set lives in the index's directory under `app.`
  (`bundle_detail::prefix`; bundle names cap at 251 so the on-disk name fits NAME_MAX); strata
  parses only `manifest-*`/`seg-*` and its open, clear_uncreated and retention touch nothing else
  (index.hh 141, 5234, 5284, 5311); the set's recovery deletes only prefixed names. The group
  owns its directory (`std::unique_ptr<jn::IO>`) and drops it at close. Tests run one faultfs
  disk per group (A1's ENOSPC and sync-EIO cases unchanged in effect: no strata IO in those
  windows). `test/group/file_set_test.cc` (ctest `group_file_set`), mutant no-prefix.
- A6 heap, by owner (`test/group/footprint_owners.py`, malloc_history at 100 idle groups, malloc
  blocks, b98362b7): group 8.7 KB (chore deque's 4 KiB block 4.6 KB, the object 3.6 KB, file map
  0.1), consensus 5.2 KB (Core 2 KB, Worker 2 KB, Group row 0.4, log 0.2), journal 4.7 KB
  (host table 1.3 at 100 groups, falls with count; Journal 0.85; turn-log notes 0.9; Admission
  0.3; owner session 0.2; PrefixIO 0.2), index 4.0 KB. Cut: the deque (an intrusive queue), the
  config kept after open (800 B in the object plus its heap; `opening_` freed at open's end), the
  file set's directory. Now (64 groups, requested bytes): idle 12,720 B = index 5,090 + consensus
  row and journal group 6,984 + the group's own 645 (sizeof group 2,504, of it the index 1,856);
  2.00 descriptors; closed 3.7 KB and 0 descriptors (was 9.2 KB, 2); destroyed 1.1 KB. The test
  checks own <= 1,024 B (mutant keeps-config) and closed 0 (mutant close-keeps-directory).
- A6 open cost (sample(1), 200 groups, waves of 8, PosixIO-seam syncs skipped: 289 ms per open
  and first write on a loaded host; 1,117 ms with syncs issued, 12.4 seam syncs): journal's host
  table commit at create_group (`Host::commit` -> `replace_file`: create, write, direct
  F_FULLFSYNC, rename, directory sync) 38%; journal's turn flush (`execute_primitive` sync_data
  through InlineCompletion) 15%; strata's `create` committing generation 1 (manifest write,
  rename) 20%; the first write's blob (file-set open, pwrite, close) 17%; the host's mkdir 5%.
  Direct flushes bypass the seam (`PosixFile::sync`), so NOSYNC skips none of them. Ours:
  strata's create commit, removed by D112 (`create_deferred`, public in index.hh; the first
  checkpoint commits generation 1); the rest is journal's (Detent, forwarded by the lead) or
  the host's.
- Scale (lead): counts at 10^4 as a light job (60 min); idle CPU strict at 500 and 1,500, per
  group and flat in N (the two jobs compare).
- A10 and A5, `test/group/lifetime_test.cc` (ctest `group_lifetime`): `held`, threads: A's apply
  blocked inside an entry, B on the same loop answers 5 `quorum` writes, a read and a checkpoint;
  then 60 writes, 9 reads, 3 checkpoints against B's apply, for TSan. `points`: checkpoint,
  close and read_applied started at every event of a run (apply first: 46; commits first, ranges
  queued: 41); oracle in the file's head; crossings counted, each at least 1 (capture past its
  start 34 runs, close with entries applied after it started 86, read waiting on apply 86).
  Mutants apply-on-loop, close-uncovered (killed locally); capture-on-loop and read-on-loop
  survive without TSan (capture-on-loop fails only the crossing count), so their jobs run
  `held` under TSan. read-on-loop may survive there too: strata's index takes concurrent reads,
  so a loop-side read is not a race on the index; if it survives, drop it (Rulings).

## Next

0000. Read step 11's jobs (State); fold any red in; then the lead's re-review.
000. Read step 10's jobs (Doing): suite and crash at strata:6a230264, the seal-repeats kill
    check; then step 9's below.
00. Read step 9's jobs, all queued at its end: build-test 261120-092528-6f13a3, crash
    261120-092528-a14183, named cases 261120-092530-8e1d75, failed-open 261120-092537-7db5a0,
    mutants 261120-092539-50196a 261120-092541-cd604b 261120-092544-0e1fd2 261120-092546-475273
    261120-092548-bda1ed; fold any red in. Step 8's at 33230b18: crash 261120-090009-2638c9 green;
    B10 append-on-retry (lone voter) 261120-090021-8a66ab killed (exit 1, green); build-test
    261120-090009-f2e837 (expected red: default_limits, fixed at c2b656db), append-on-retry
    (voters) 261120-090029-d472af, sync-retry 261120-090036-a032b9, read-skips-probe
    261120-090043-0b4a03 and the A7 gap 261120-090052-9c8c69 still queued or running. Then the lead's correctness review and CI push.
0. Read step 8's jobs (Evidence); then the lead's correctness review and CI push.
1. (done) Read step 7's jobs: build-test green at 88d72629, cold-schedulers killed, the size
   breakdown's 4096 B row gone; then lead's push for CI. Step 6's reading, for reference: closed (0 descriptors, 0 frames, heap by owner), destroyed split
   (ours, consensus's row: Detent's consensus-quiescence heap criterion), timing at 500 and
   1,000 flat in N; fix failures (if `gone_slack` 16 B is off, read the row's mirror first:
   `footprint_owners.py --at destroyed --base before --closes N`). Push for CI (lead).
2. (done at c2b656db, step 9) A3's remainder. Built at strata:a4e7cf80 (`submit.hh`, `blob::app::prepare`, voters_test
   `write_paths`, mutants ack-at-commit, prepare-before-ready, pending-ignored, lift-the-bound,
   no-forward, resubmit-invalid). Left: a leader stalled on `no_space` hands leadership to a
   follower applied past it (`transfer_leadership`); assert an election replacing an uncommitted
   entry answers `superseded` (seen: 1 and 2 per 8 seeds, not yet required); forward-twice
   mutant. Original plan, for reference. Lead's list:
   quorum answer carries apply's verdict (built: `propose` at `ack::quorum`); refused merge
   resubmitted from the original request up to 3 placements, then a retryable 503; a deposed
   proposer forwards once carrying its placement count; `superseded`. Plan:
   - `include/strata/group/answer.hh` holds the rule (`after`, `max_placements`); add the loop
     there or beside it: `submit(request)`, where the request prepares a command from the
     leader's applied state (a merge chooses `expected_version`: `leader_chose = true`), proposes
     at `quorum`, and on `after(...) == resubmit` prepares again from the original request;
     `unavailable` answers a retryable 503; `rejected{NotLeader}` at a deposed proposer forwards
     once through a host hook on `loop` (like `send`), carrying `placements`; `superseded`
     answers as is, never resubmitted. Keep it isolated so a different ruling edits one file.
   - blob app: add a merge command (`M|key|value`, prepared to `C|key|value|<version>` at the
     leader) and an invalid one (strata `invalid`, answered 400, never resubmitted); a test hook
     that makes the predictor miss the fields mark (stale version) so the bound is reached.
   - voters_test cases: two merges to one id, a conditional write and an invalid document with
     an election between, every schedule: no acknowledged success refused, no merge answered
     409, the conditional's answer matches its effect, invalid answered 400 once, no request past
     3 placements (503 after the third), a deposed proposer's merge forwarded once and answered
     by the leader; a leader stalled on `no_space` hands leadership to a follower applied past
     it (`transfer_leadership`); an election replacing an uncommitted entry answers `superseded`.
   - Mutants (`mutants.py`, `--target=group_voters_test`, each a `--fails-without` job):
     acknowledge-at-commit, prepare-before-ready, lift-the-bound (`max_placements` unbounded),
     forward-twice or no-forward, resubmit-invalid.
3. A4's applied-before-write mutant (see Rulings).
4. After detent:consensus-quiescence lands: A5's `TurnLog::read` finds none after close and the
   in-process reopen (msg-1), A6's quiescent groups, A7. A6's catalog part (strata's catalog owns
   the group's and index's lifetime; a catalog close of an idle group publishes a bundle covering
   applied) is not built: the catalog has no group hook yet.
5. A8: the owner's decision (cluster-release Q2); nothing to build.

## Messages

- msg-2 (lead, D109): (a). Built: A2's gated schedules checkpoint only when every node is up,
  none stalled or short of space, and the checkpointing node's commit is at or below every
  node's; its loop and chore then run alone (IO held, no message in) so the capture lands at or
  below that commit (`world::leaves_none_behind`, `world::capture`); the settle check is
  unchanged. Seeds 8, 12, 16, 18, 23, 24, 27 (3 voters) run ungated as fixed schedules that
  assert the cluster does not settle and every replica behind is named in `installs_needed()`.
  Root cause (for group-install): a replica cut off while another checkpoints past its log end
  gets `SnapshotNeeded` (consensus core.h:1164-1168) and, with no install, stays behind; 7 of 80
  schedules, all failing, none failing without it (`~/.scratch/group-core/census-40-seeds.txt`).
- msg-1 (lead, journal-host-guards): done in strata:1068c00d. journal pinned at cc95ba6 (landed);
  `jn::group_closed` from the Driver's propose maps to `cs::closed` (`group::place`); consensus's
  own `durable()` watch (driver.h:928) swallows it, so the group's wait is where it lands: a
  close (and a fence) refuses every `quorum` waiter, linked or not yet (sticky `refused_`).
  Tests: lone voter's script proposes then closes, the write answered (verdict or `closed`), the
  group reopened from its bundle at every crash point; voters_test `close_with_uncommitted`: a
  leader cut off from its followers places a write and closes, the waiter answers `closed`
  (mutant close-keeps-waiters, job 261117-085705-587d4d), the group reopens and the cluster
  settles to equal digests. Not testable yet: an in-process reopen and recovery over a retired
  sequence gap. At consensus 57745e8, `Driver::close` drains the worker but never calls
  `Host::close_group`, so the journal group stays `stored` and an in-process reopen is refused
  ("the group was recovered on this host"); the gap appears only once
  detent:consensus-quiescence closes the journal group. The tests restart the process instead
  and say so; add the in-process case with A7.
- Lead, mid-step: keep consensus at 57745e8 (main e4e394b red). Kept; only journal moved.

## Blocked on

- Lead (finding, not blocking): strata core's `turn_sealer` (turn_seal.hh) makes its turn mutex
  the loop and lets any thread call the table (create, close, reopen, attach) under it, while
  journal 9e11713 debug-asserts one thread per loop's range table. No race (strata never calls
  `group_io`; every `publish` is under `turn_mu_`), but any standalone user closing an index on
  a thread other than the first to touch the table aborts in debug builds. Options: (a) journal's
  check accepts an externally serialized loop (an owner handoff or a lock-held check; Detent's
  library); (b) turn_sealer funnels table calls to one thread; (c) document it in turn_seal.hh.
  Recommend (a).
- Lead (not blocking step 7): consensus keeps a row open until `Driver::close(id)`, so two
  in-process paths leave an open row behind and make a reopen throw ("the group is open on this
  Driver"): (1) a fenced group destroyed in a live process (a fence never closes the row);
  (2) `open()` failing after `Driver::open`. Options: (a) the group closes its row on those
  paths (`Driver::close` after the fence or the failed open; needs consensus to close a fenced
  row, a Detent question); (b) the catalog (A6's lifetime owner) closes the row of a fenced or
  failed group before dropping it; (c) only a process stop ends them, as today. Recommend (a)
  for the failed open (group-core's own) and (b) for fences once the catalog hook exists; Detent's
  answer: consensus-close-fenced (landed, consensus:9b218a0) closes fenced and half-opened rows,
  so (a) is buildable in group-core. (2) built at c2b656db (step 9: a failed open closes its
  row). (1) stays: a fence never closes the row; the catalog's (option b) once it has a hook.
- Detent (forward): (1) 64 groups opened at once on one host fail with "storage fenced outside
  worker"; bare consensus rows reproduce it, no strata::group involved:
  `GROUP_FOOTPRINT_NOSYNC=1 GROUP_FOOTPRINT_WAVE=64 build/group_footprint_test 64 0` at
  strata:ba578486 (rows: `Driver::open`, `create`, one `propose`, `applied`; 8 at once pass).
  (2) The host table's `replace_file` and the turn flush call `PosixFile::sync` directly, past
  PosixIO's sync seam, and are 53% of an open's time with the seam's syncs skipped.
- A2's settle failures: ruled (a), PLAN.md D109 (lead, msg-2); group-install takes the seven
  seeds as its A8. Root cause and proof under Messages, msg-2.
- detent:consensus-quiescence: A7 built against its Driver part at consensus:b8de3b8 (step 11);
  the pin returns to consensus main when quiescence lands. Before: The
  in-process reopen after close now works at consensus 1c08c9f (closed row erased, journal group
  closed into the host's table) and `reopen` tests it; a reopen still elects before its first
  write (5 barriers against 2), which is quiescence's A5.
- Q10: decided (b), D106. Built on it, kept in one place
  (strata `include/strata/group/answer.hh`: `ack`, `answer`, `max_placements = 3`, `after()`).
- `strata-nospace-position` (not built): the group relies on its contract (a `no_space` answer
  leaves strata's position unmoved). At 491a1bdc the reservation path keeps it and the allowance
  path moves it; the group fences on the `skipped` that follows (`group.hh`, `write()`), and A1's
  `no_space` case uses the reservation path. When it lands, add the allowance-path case.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | at strata:ffab229e: build-test 261120-121525-807eb8, crash 261120-121525-6eba6d; file_set_test (N4) 261120-121539-279b80; `--fails-without` sealed-unchecked 261120-121551-0fb5d1. Before: green at strata:6a230264: build-test 261120-105301-a74923, crash 261120-105301-679f4f. Before: queued at strata:c2b656db: build-test 261120-092528-6f13a3, crash 261120-092528-a14183. At strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837 queued, crash 261120-090009-2638c9 green; `--fails-without` B10 append-on-retry 261120-090021-8a66ab killed, B10 sync-retry 261120-090036-a032b9. Before: queued at strata:c99372d4 (one directory per group, deferred create): crash 261117-151220-79527f, TSan 261117-151216-02b5d8, `--fails-without` unlink-at-apply 261117-151147-f41da5, append-on-retry 261117-151150-e05468, sync-retry 261117-151154-9b1fbc, first-commit-unsynced 261117-151125-7ccde6. Before: green at strata:1e88eb3f: build-test 261117-112713-54e643, crash 261117-112713-a33261, ASan+LSan 261117-112719-032b88; `--fails-without` (strata:e7bb6c70's mutants, same anchors): replay-from-ph 261117-041329-e493ed, unlink-at-apply 261117-041335-5055d0, publish-before-sync 261117-041341-82b222, capture-mid-entry 261117-041346-d08891, effects-gated 261117-041351-01ad6c, appstate-gated 261117-041356-b72595, no-mark 261117-041401-a4bc5e, commit-after-publish 261117-041406-4aaf54, append-on-retry 261117-041412-e13f97, sync-retry 261117-041417-101200. 174 script events, 175 crash points, 2,527 second crashes |
| A2 | queued at strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837, crash 261120-090009-2638c9; `--fails-without` B10 append-on-retry (voters, file-set ENOSPC once) 261120-090029-d472af. Before: queued at strata:c99372d4: append-on-retry (voters) 261117-151158-264d61, admission-waits 261117-151202-2b5482. Before: green at strata:1e88eb3f: build-test 261117-112713-54e643, crash 261117-112713-a33261, 40 gated schedules per voter count 261117-112714-70e657, TSan 261117-112716-5eea20; `--fails-without` append-on-retry 261117-085651-94cdb1, admission-waits 261117-085659-d135d4, install-silent 261117-112707-8d20d3, checkpoint-ungated 261117-112710-55af69. Seeds 8, 12, 16, 18, 23, 24, 27 (3 voters) fixed ungated: named in `installs_needed()` (D109) |
| A3 | at strata:ffab229e: build-test 261120-121525-807eb8, named cases with `stalled_follower_elected` (C1) 261120-121535-6b10fe; `--fails-without` handoff-leader-only 261120-121542-01253e, no-hand-off (anchor moved) 261120-121544-aeb1f7; reviewer's probe rerun at 274fc243 261120-114759-d6b42f (exit 0). Before: green at strata:6a230264: build-test 261120-105301-a74923, crash 261120-105301-679f4f. Before: queued at strata:c2b656db: build-test 261120-092528-6f13a3, crash 261120-092528-a14183, named cases (`group_voters_test 0`: superseded_entry, forward_once, stalled_leader) 261120-092530-8e1d75, failed-open 261120-092537-7db5a0; `--fails-without` no-hand-off 261120-092539-50196a, superseded-ignored 261120-092541-cd604b, resubmit-superseded 261120-092544-0e1fd2, forward-twice 261120-092546-475273, failed-open-keeps-row 261120-092548-bda1ed. Before: queued at strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837, crash 261120-090009-2638c9. Before: green at strata:1e88eb3f: build-test 261117-112713-54e643, crash 261117-112713-a33261, named cases (`group_voters_test 0`) 261117-112639-0e3525; `--fails-without` close-keeps-waiters 261117-085705-587d4d (vacuous, superseded by 261117-112702-5fc7d8), quorum-at-placement 261117-085711-59939a (vacuous, superseded by 261117-112659-a0922c), ack-at-commit 261117-112642-eee4a9, prepare-before-ready 261117-112645-377e63, pending-ignored 261117-112648-9a2926, lift-the-bound 261117-112650-2e5ea4, no-forward 261117-112653-d75991, resubmit-invalid 261117-112656-b5bed0. |
| A4 | at strata:ffab229e: build-test 261120-121525-807eb8, `held` (host sees applied below the held strata write, N1) 261120-121538-1d9cc7; `--fails-without` applied-before-write 261120-121549-49be3a. Before: queued at strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837, crash 261120-090009-2638c9; `--fails-without` read-skips-probe (anchor repointed) 261120-090043-0b4a03. Before: green at strata:1e88eb3f: build-test 261117-112713-54e643, crash 261117-112713-a33261, named cases 261117-112639-0e3525; `--fails-without` read-skips-probe 261117-112704-eaa423 (261117-085718-358fc4 was vacuous). Left: the publish-applied-before-write mutant (Rulings: no loop-side read to reach it) |
| A5 | at strata:ffab229e: build-test 261120-121525-807eb8 (ctest `group_lifetime`: points with late calls after a close, C2; reopen in its closed term), points 261120-121536-1b02bc; `--fails-without` no-closed-mark 261120-121545-005bf5, checkpoints-overlap (anchor moved) 261120-121547-534c19; close_race_probe rerun 261120-121553-724c76. Before: queued at strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837, crash 261120-090009-2638c9 (ctest `group_lifetime` runs `reopen`: closed in the host's table, no live turn-log record, in-process reopen, restart). Before: queued at strata:b98362b7: build-test 261117-134414-aaeddb, crash 261117-134414-ef6998, TSan 261117-134416-c8e692; `--fails-without` close-uncovered 261117-134421-e77835, checkpoints-overlap 261117-134428-8708d8. Left for detent:consensus-quiescence: `TurnLog::read` finds none, in-process reopen |
| A6 (part) | queued at strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837, crash 261120-090009-2638c9 (footprint ok locally: closed 2,945 B, destroyed 194 B). Before: Queued at strata:88d72629 (schedulers warmed; open groups fenced before destroy): build-test 261117-220121-0d17ad, crash 261117-220122-3683cd, `--fails-without` cold-schedulers 261117-220123-40de8b, size breakdown 261117-220126-1624db. At 47e7f140: build-test red 261117-174709-007299 (64 B, the chore queue: Doing, step 7); 10^4 counts 261117-174721-76cffc green. Before that, queued at strata:47e7f140 (closed and destroyed per closed group, consensus's closed row): build-test 261117-174709-007299, crash 261117-174709-c75fb1, counts at 10^4 with 1,000 closed (light, 60 min) 261117-174721-76cffc, strict 500 261117-174725-f04c3b and 1,000 261117-174730-8a0b8c (10 closes each), `--fails-without` destroy-keeps-member 261117-174734-de0aa7; TSan 261117-174738-0c9f4a (A1-A3, A5, A10). Before: green at c99372d4 (step 5 mutants, 151206 counts, 3cedb9 timing at 500; 6093d5 at 1,500 stopped at its cap, invalid). Budget: idle group 12,720 B = index 5,090 + the group's own 645 (5.7 KB, "about 5.4 KB") plus the stated overhead, consensus's row and journal group 6,984 (Detent's, outside the criterion); 2.00 descriptors, closed 0 (64 groups, local, strata:1b1601be). Queued at strata:c99372d4: build-test 261117-151220-00850f (ctest `group_footprint`, `group_file_set`), counts at 10^4 groups (light, 60 min) 261117-151206-b86dca, idle CPU per group strict at 500 261117-151209-3cedb9 and 1,500 261117-151213-6093d5 (per group, flat in N: compare the two); `--fails-without` no-prefix 261117-151128-1d419f, keeps-config 261117-151132-fc01a3, close-keeps-directory 261117-151136-c0239c, close-keeps-index 261117-151139-b815a7, files-keep-handle 261117-151143-991952, D112 first-commit-unsynced 261117-151125-7ccde6. Left: quiescent groups (consensus-quiescence), the catalog close |
| A7 | at strata:ffab229e (consensus b8de3b8, quiescence's Driver part): `group_lifetime_test reopen-turn` 261120-121540-67ed09 (local: 2 barriers, an open group's 2), ctest `group_lifetime_reopen_turn` in build-test 261120-121525-807eb8. Before: gap job at 33230b18 261120-090052-9c8c69 (failed as expected: 5 against 2) |
| A8 | [decision] D113 (owner, 2026-11-17): cluster-release Q2 (a), document mode with `quorum` and `none` |
| A9 | strata `docs/mechanisms/replication-group.md` at strata:466dbece: lone voter, close and lifetime, the admission bound, the voters proof |
| A10 | at strata:ffab229e: build-test 261120-121525-807eb8, points 261120-121536-1b02bc, held 261120-121538-1d9cc7. Before: TSan at strata:6a230264 261120-111836-ce9ade (reviewer's). Before: queued at strata:33230b18 (merge, pins 1c08c9f/9e11713/d8b538c): build-test 261120-090009-f2e837, crash 261120-090009-2638c9. Before: queued at strata:b98362b7: build-test 261117-134414-aaeddb (ctest `group_lifetime`), TSan 261117-134416-c8e692; `--fails-without` apply-on-loop 261117-134435-94dd84, capture-on-loop (TSan) 261117-134456-196914, read-on-loop (TSan) 261117-134505-949822 |
| Models, rounds 1-3 | `bundle_model.py` jobs 261117-024523-dd3b4d, 261117-025944-a43484, 261117-030454-a1f4da; `verdict_model.py` 261117-024526-a92827, 261117-025949-8a87ce |

## Result

Step 1 (strata): `include/strata/group/{group,bundle,file_set,answer}.hh`, target `strata::group`;
`test/group/{lone_voter_test.cc,blob_app.hh,host_io.hh,mutants.py}`, ctest `group_lone_voter`;
`docs/mechanisms/replication-group.md`; `docs/REPLICATION.md` "Checkpoints and repair" takes the
proposed text. strata pins consensus 57745e8, journal 555b69f, task 95798a0, completion 7731315.

## Rulings I made

- Step 12: t6w's fix is the test's thread layout, not turn_sealer (the contract question is
  under Blocked on); voters' draw order follows clang, so D109's seeds keep their schedules.
- footprint_test CI failure (run 44797677012): TSan duplicates operator new/delete at link (runtime defines them); ASan's aligned/nothrow forms bypass the replacement, so plain delete read a header off foreign memory. backtrace link errors are FreeBSD (libexecinfo). Ruling: under __SANITIZE_*/__has_feature the test exits 77 (SKIP_RETURN_CODE 77) with a message, every other build measures; backtrace links via find_package(Backtrace). strata:25960848.

- Step 11, C1: re-arm whatever the role, not on `RoleChanged` (reasons in Doing).
- C2: `close()` holds the checkpoint serializer through `Driver::close` and the teardown, so no
  checkpoint runs between the close's covering checkpoint and its teardown.
- A7: the closed record lives in the closed group (`closed_record()`, about 200 B, inside A6's
  own budget for a closed group) and reaches the reopen through `config.closed`; keeping it
  across a restart is the host's (catalog's) choice, and without it a reopen elects. A durable
  home beside the bundle is a format question for the catalog (A6's catalog part), not built.
- `open()` skips its loop hop when already on the loop; its contract ("on the loop") is unchanged.
- N2 (review, recorded only): a `quorum` wait cannot be cancelled; `propose`'s token covers only
  the placement. Xapiand's write path needs its own deadline, or propose takes the token through
  the wait: the owner's call.
- N5 (review, recorded only): turn_seal.hh:107's "durable when it returns" holds at journal
  9e11713 only; recheck when the journal pin passes 7b64376 (turn-log-parity's file).
- N4's mutant and test are cited under A1 (recovery); N1's under A4.

- Step 10: test/standalone_turn_log_test.cc (outside the file set, standalone-turn-log's) changes
  t6's reopen sequences, the pin move's consequence, and nothing else; index.hh's seal() keeps
  D4's one record per turn rather than taking journal's repeat calls (that would move D4's
  window and `lag`'s bound, a design change for the turn-log owner). turn_seal.hh unchanged.
- Step 9: the stalled leader's target comes from a host hook on `loop` (`applied_past`, beside
  `send`): consensus's leader knows followers' matched index, not their applied position, so the
  group cannot pick "a follower applied past it" alone. Unset, no hand-off (a lone voter, or a
  host without health). The voters world sets it only in `stalled_leader` (`world(..., true)`),
  so A2's schedules and the D109 seeds run as their evidence did.
- A refused or aborted transfer (Busy, InvalidTarget) is dropped and tried again at the next
  timeout while the stall lasts; a failed open's `Driver::close` error is dropped so the open's
  own error is the answer (a left row is then loud: a reopen throws "open on this Driver").
- voters_test `default_limits` (pre-existing red at 33230b18 under consensus 1c08c9f's D132):
  keeps consensus's default log (log_bytes, log_entries), which is what it tests, and the test's
  wire limits, so its batch fits the 16 KiB turn-log segment.
- sizeof(group) grows by the hand-off timer and its delay (about 48 B); A6's own budget (1,024 B,
  measured 539-645) absorbs it. The footprint job (build-test) checks it.
- Step 8: `include/strata/core/turn_seal.hh:103` (turn-log-parity's file set) changed one line,
  `const auto* e` -> `const auto e`, so it compiles against journal 9e11713's optional
  `Host::entry`; the pin move required it and the line reads the same for a pointer.
- The group tests' `rpc_bytes` 16384 -> 8192 rather than larger segments: the 16 KiB turn-log
  segments keep their rotation in the crash schedules.
- A7's equality runs only as `reopen-turn`, outside ctest, so build-test stays a true gate; its
  gap job records the measured 5 against 2.
- Step 7: the 64 B was the footprint's own chore scheduler (deque blocks kept once grown);
  fixed in the test by warming both schedulers, threshold unchanged. Destroy-while-open stays a
  documented precondition (the close is async; a destructor cannot run it); the footprint now
  fences before destroying, like the other tests.
- D111's directory: the group owns its one directory (`std::unique_ptr<jn::IO>`) and lets it go
  at close; a reopen is a new group object over the same path, as the tests already did.
- `own_heap` = 1,024 B is the group's own budget past its index and consensus row (measured 539-
  645 B); it holds the object past its index and the file set's entries, which grow with the
  application's live files. `recovered()` keeps the recovered bundle (one map entry per file):
  kept, the lifetime test reads it.
- Bare index and row costs come from 256 of each: per unit they do not change with count.
- The bare index closed still holds 1.9 KB per index (pooled task frames and an executor's
  cancel_source), strata core's and task's, not the group's.

- Checkpoints run one at a time (`task::async_mutex` in `group`, strata:b98362b7): consensus
  rejects a checkpoint while one is pending (`Busy`, core.h:600), so a close racing a periodic
  checkpoint failed with "consensus call rejected". `points` starts a checkpoint and a close
  together at every event; mutant checkpoints-overlap fails it (killed locally).

- A6: "of its own" is measured against a bare index holding the same document (2 descriptors,
  5.1 KB, 1 pooled task block: the index holds one), so the group's own is the difference; the
  journal host may open a turn-log file meanwhile (at most 2 descriptors, never per group).
  Timers are the loop's armed entries, at most consensus's rows' (0 for an idle lone voter).
- A6 closed and destroyed rows are per closed group; consensus's part is a bare row that
  checkpoints once and closes as a group does, so the group's own is the difference. The
  footprint runs with task's frame pool off: the pool caches up to 1,024 frames per size class
  per thread, bounded and not per group, and with it on a freed frame stays in the heap count.
- Timing jobs at 500 and 1,000 close 10 groups (the close's cost comes from the 10^4 job's
  1,000) and close as many bare rows, so each fits the strict 10-minute cap.
- A6 at scale skips the syncs PosixIO issues through its seam (counted, not issued): they change
  no count, only the time. strata's and the journal's own flushes still run.
- Found (hunt row candidate, Detent's): 64 lone voters opened at once on one host all fail
  `open` with "storage fenced outside worker" and fence (`GROUP_FOOTPRINT_WAVE=64
  group_footprint_test 64`); 8 at once open fine. Not chased; the footprint opens 8 at a time.
- A10's `points` counts its crossings (capture past its start, close with entries applied after
  it started, read waiting on apply) and fails if any is zero: with apply first, entries commit
  one per turn and are applied at once, so none queued; commits first makes ranges queue.

- A3 (D106) split: the group gives the ready point (prepare runs on the chore once applied
  reaches `Driver::read`'s index) and the leader's pending commands (its term's, prepared and
  not applied, in prepare order; a placed one drops out once applied passes it); the predictor
  is the application's (`blob::app::prepare`, Xapiand's later). `submit.hh` holds the loop.
- A read refused `NotReady` or `Busy` (a leader before its term's first entry commits) answers a
  retryable 503 with nothing placed: preparing there could miss the prior term's entries.
- A client's own condition (`C|key|value|v`) is checked by apply, its 409 apply's verdict; the
  model's 409-at-prepare is an option the application may take (`prepared::refused`).
- `propose` takes a `placed` hook (the writer learns the placement before apply answers); the
  `quorum-at-placement` mutant's anchor moved with it.
- The test's forward goes to the leader of the highest term (a cut-off leader still believes it
  leads).

- Step 2: the CI leak was the test destroying a group with its chore and waiters suspended.
  Rule: a group is destroyed with its chore idle (after `close()`, or fenced and drained); a
  fence drops the item `drain()` kept; `applied(through)` posts hold loop and id, not the group.
- A checkpoint with nothing applied past the last bundle answers that bundle (`published_`):
  consensus refuses a capture at or below its base (`InvalidCheckpoint`), which made `close()`
  right after a checkpoint fail.
- Hunt row "checkpoint admission at default limits": the group fails such a checkpoint with
  `length_error` when `maximum_size(limits) + bundle > replacement_pool.logical_bytes` (necessary,
  not sufficient: journal's plan peak is private). Sizing the pool stays quest 10's.
- Hunt row "faultfs lacks lock_owner": the fix is in `strata/core/faultfs.hh`, outside this file
  set; the voters test reuses `test/group/host_io.hh` per node. Left on the hunt list.
- `read_applied` runs its function on the chore between two entries once applied reaches the
  Driver's read index, so A4's "publish applied before strata's write" mutant cannot make it read
  early: the read never runs mid-entry. That mutant needs a loop-side read; not built.
- A2's budget refusal uses the reservation path (as A1): the allowance path moves strata's
  position until strata-nospace-position lands, and the group fences on the `skipped` after.
- The voters test's ctest runs 12 schedules per voter count; the evidence job runs 40.

- strata pins consensus at 57745e8 and moves journal to 555b69f, task to 95798a0 and completion to
  7731315 (consensus's and journal's own pins, each a descendant of strata's old one; framing.h
  unchanged), and turns on `COMPLETION_TASK`, which journal now links.
- Strata position of entry i is `(i, 1)`; the capture's mark is an empty transaction at `(P_b, 1)`.
- One queue per group on a host-supplied chore scheduler, one item per resume, so every entry,
  capture, sync-and-commit, unlink and recovery is a crash point. Recovery goes to the queue's
  front and the queue holds until then, so ranges Core delivers during `recover` wait behind it.
- strata is created after the journal group's create (open-or-create on the chore), so a strata
  index never holds entries its journal lacks.
- Apply keeps a phase: a stall after the file effects retries only the strata write; a stall in
  the effects (ENOSPC) re-runs them from the entry's start, positionally.
- A `quorum` proposal not yet registered may be any entry applied meanwhile: apply keeps outcomes
  above the oldest unregistered proposal's starting applied index, pruned as they register.
- The journal host's faultfs goes through `test/group/host_io.hh`, a forwarding IO that adds
  `lock_owner` (faultfs::io implements only the legacy lock calls; core is outside this file set).
- Oracle: the test application records every entry it applies (index, term, kind, command); the
  replay runs those through the application alone on a fresh strata index and an in-memory file
  set, sharing no code with the group's apply, checkpoint or recovery.
- The consensus limits in the test are small (log 64 KiB): at the defaults a checkpoint's
  reservation exceeds the admission's 64 MiB replacement pool and waits forever. Xapiand's group
  options must size the pool for `maximum_size(limits)`; noted for quest 10.
- "Capture inside an entry's apply" is reached through the test application's `after_effects`
  hook, which starts a checkpoint while an upload's final entry applies.

## Design result (rounds 1-3)

Round 3 (B10, N16, N17): file effects are positional from the entry's starting application state
(the current volume's length is in it), so a failed push (ENOSPC) stalls and rewrites the same
bytes; a failed checkpoint sync fences and reopens through recovery; a forwarded re-submission
carries its placement count. D3, D16 step 2, quest 5 A1-A3. `bundle_model.py`, 11 scripts:
`fixed` 0 of 1,746, `sync-late` 0 of 1,752, `retry-whole` 0 of 1,758 (now safe); `append-on-retry`
fails 34, `sync-retry` 6, the other twelve broken variants fail as before.


Round 2 (re-check B7, B8, N10-N15), on top of round 1's revision 2 (B1-B6):

- B7: retryable answers (`no_space`, owed verdicts, a stage-1 throw) keep strata's position
  (`strata-nospace-position`, which quest 5 lands after) and retry the strata write alone; the
  file effects ran once and the application state updates after the definite answer; a third
  throw at one entry fences the replica. Definite, position-moving answers: `accepted`,
  `version_conflict`, `schema_conflict`, `fields_conflict`, `invalid` (N12: replicas share
  strata's caps). D3, D18.
- B8: re-submission from the original request for leader-chosen expectations only, at most three
  placements, a deposed proposer forwards once to the leader, then a retryable 503. D3, Q10.
- N11: the mark retries like an entry, so a capture and a close wait out `no_space`. N13: the
  application state is verdict-independent. N15: a stalled leader holds its group's `quorum`
  answers and hands leadership to a follower applied past the entry after one election timeout.

Models (`docs/design/cluster-release/`), jobs below:

- `bundle_model.py`: 9 scripts; `fixed` and `sync-late` fail 0 of 1,416 crash points each; every
  broken variant fails some: a1-literal 80, eager-delete 13, counter-from-dir 26, publish-first
  34, commit-after-publish 30, capture-mid 212, verdict-gated 8, read-early 74, no-mark 86,
  nospace-moves 36, retry-whole 40, appstate-gated 74.
- `verdict_model.py`, two voters with their own applied state: (a) acknowledges a refused write
  and fences in 2,442 / 1,092 / 0 / 1,813 of 2,520 schedules (applied, pending, ready point, the
  fields-mark gap); (b) over 20,160 schedules under all four rules: 0 false acknowledgements, 0
  fences, 0 merges answered 409, 0 unanswered, 0 divergent verdicts, `invalid` never
  re-submitted, no request past three placements; re-submissions 35,296 / 6,328 / 0 / 24,040,
  deposed-proposer forwards 9,128 / 0 / 0 / 9,296, merges answered 503 2,152 / 112 / 0 / 1,680.

## Design rulings

- Criteria in the design's section 5 follow the quest file's numbering (A1-A9, plus A10) so the two
  stop drifting.
- Models live in `docs/design/cluster-release/` (as `sortable-encoding/sizes.py` does), not in the
  reviewer's scratch, so the review reruns them at this commit.
- Added the `docs/design` paths to this worktree's sparse checkout to edit the design.
- Three placements per request: the third covers a leader change plus one predictor miss; with
  the ready point and pending versions the model never needs a second.
- A stage-1 throw retries twice, the third fences: a transient allocation failure clears, a
  damaged segment repairs by install.
