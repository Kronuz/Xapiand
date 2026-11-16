# detent async-drivers-design review: design, from Xapiand's side (A8)

Commit: Detent:1ece2ff4ccb07408f9523b273a8f50f34e544aeb
Verdict: APPROVE
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

The Detent lead asked for this review (the journal's "Note for the Xapiand lead", A8). It covers
`docs/design/async-drivers.md` at the commit above, for Xapiand's Phase 7b: shards become groups,
with millions of indexes per host. Detent's own design review (F1-F15, N1-N9 in Detent
`.campaign/detent/reviews/async-drivers-design-design.md`) is not repeated here. Libraries were
read at the design's pins (consensus:ab05507, journal:e5f6ff1, task:bdafa5f), with
Xapiand:383b0876b, strata:3df7af4f, logger:f61d8df, scheduler:d57e8fc and reactor:ab69cf2. IDs
start with X so they do not collide with Detent's F and N.

The direction is right for Xapiand, and most of the design fits it as written. Five points block
adoption. Each is a small change to text or a quest criterion, not a re-plan.

## Verdicts on the asks

**1. API (D2): agree.** Xapiand's `src` has no `asio::awaitable`. Every database task already runs
as `task::task` over `scheduler_ref`, and `engine::AsioHost` wraps the loop's
`task::asio_scheduler` (`src/engine/host_asio.h:28-57`). Xapiand therefore takes D2 with no
bridge. A cross-loop call costs two posts, which is small next to a request. Two follow-ups:
Xapiand's apply moves off the group's loop and the November 2 turn line is superseded (X10), and
`Seams` should take its wheel from the scheduler (X14).

**2. Deadlines (D8): the shape is right; its lifetime rules have holes.** It matches #18 (H2 in
`docs/dev/github-triage.md:268`): one deadline per fan-out, children only shorten it, and
`cancel_reason::deadline` lets a search tell a timeout (a partial result with `timed_out`) from a
disconnect. On the paths the table names, the claim word allows exactly one resume:
- The loser of the CAS does nothing.
- Completion and the hop both run on the group's loop.
- `~cancel_callback` waits for a callback that is still running.

There is no double resume. Two cases are not covered: waiters still linked when a group closes,
and frames that teardown destroys without resuming them. Under Xapiand's topology either one
gives a lost wake or an unlink into freed memory (X2). Rows 1 and 2 both return `cancelled`, so to
the caller a cancelled proposal has an unknown outcome (X11).

**3. Blocking entry (D5): the rules are right; the scheduler and the lone-voter sleep are not.**
The rules hold: drain before returning, `stalled` on a lost wake, `needs_peer`, and the owner tag.
In 7b a loop Driver serves every partition, so Xapiand's WorkPool chores never run the entry on a
live group. Its Xapiand users are offline tools (D28's recovery tooling), tests and the simulator.
Journal IO moves from flush chores (today's WAL append, `src/database/shard.cc:1133`) to the
loops' completion queues. Two problems remain:
- The entry is specified over `task::inline_scheduler`, which cannot carry D8's cancel hop (X3).
- It sleeps through a lone voter's 1-2 s election (X5).

**4. 7b fit: the arithmetic checks; a million groups per host fits only once groups follow the
catalog.**

The job records I read (below) give these sums:
- A three-voter member today: 6,262 + 164,640 + 511,947 = 682,849 B, with 3 + 5 descriptors.
- After D9 and D10: 6,262 + 1,600 + 48 + 16 = 7,926 B, with 3 descriptors.
- A million leaders: 10^6 x (10.6 + 15.5) us/s = 25.9 cores.
- The saving per 1,000 groups: (17.523 - 0.026) x 8,760 = 153,274 core-hours, $6,131 at $0.04.

"At least 26 cores" is a lower bound. In a host's real mix, a third of the groups lead and step on
twenty replies a second. Two thirds follow and each steps on ten heartbeats a second at 1,761 ns.
From the same per-step costs, that is about 30 cores before transport.

D9 and D10 are needed and right, but they do not make a million groups per host fit. An open idle
group stays at 6.5-7.9 KB and 3 descriptors. strata's working target for an idle open index is
under 1 KB, no descriptor, an open under 100 us, and a million per host
(`docs/design/small-indexes.md:25-27`). Xapiand's catalog keeps one descriptor per warm index and
closes idle indexes (`src/database/pool.cc:540-550`). So Xapiand's groups have to follow the
catalog: close when idle, reopen on demand. That needs three things:
- A lone voter leads at once (X5).
- Q4, with a closed state that a frame can reopen (X8).
- Q3 at zero descriptors per idle group (X9).

D10's placement also constrains Xapiand's loops: L is fixed for life and must be a power of two
(X4).

**5. D10 and Q7: Q7 (a) is right for the threat model, once its per-frame rule is fixed.** For a
trusted cluster with honest faults, one certificate per node pinned per loop is right, and so is
moving the hello's per-group checks into the group-open exchange. The per-frame rule is wrong: a
frame must be accepted from any node the cluster authenticates, not only from members of the
receiver's membership in force (X1). The number of connections grows with loops x peer hosts
(X12).

**Q3: (b), with conditions.** Descriptors run out first at millions of indexes, so one directory
per core is what is needed. It also suits nested index paths, because the journal never sees a
path. The conditions are in X9, and the partitions must not be bound to L for life (X4).

**Q6: (b), not (a).**

What moves are Xapiand's on-loop timers:
- `engine::AsioHost::arm` (`src/engine/host_asio.h:68-75`) creates an `asio::steady_timer` per arm
  and makes three allocations. It carries:
  - turn deadlines (`src/database/pool.cc:338`, `src/walk_gate.h:90`);
  - `engine::sleep_for` (`src/engine/host.h:163-188`);
  - the control plane's autocommit debounce, one task per pending key waiting on the host's timer
    (`src/control_plane.h:179-183`).

  It moves onto the loop's wheel through `asio_scheduler::start_timer` once quest 1 lands.
- The catalog's `DB-TIMER` thread (`Scheduler<Fire>`, `src/database/pool.cc:412-432`) only posts
  chores. It moves onto the control loop's wheel, with a `post_call` for each arm made off the
  loop.

The cost, estimated from the code: about 30 lines in `host_asio.h` and 20 in `pool.cc`, and one
thread fewer. A timer arm drops from 225-538 ns and three allocations to 14-18 ns and one. The
timer state of a pending index drops from about 240 B to a 32-byte entry.

What stays on Kronuz/scheduler:
- Kronuz/logger. Its deferred and cancellable lines are `ScheduledTask`s armed from any thread and
  run by the LOG thread (`logger/logger.h:50`, `:555-562`). There is no loop to own a wheel.
- The WAL's fsync debouncer (`src/xapiand_fsyncher.h:72`, `src/database/wal.cc:108,117`). It is
  deleted with the WAL in 7b (D13), not moved. `src/manager.h:40` includes `debouncer.h` for
  nothing.

This gives one mechanism per thread contract: an owner-thread wheel for loop timers, and the
any-thread scheduler for off-loop deferred work. These are not two timers for the same job.
Retiring the library is the owner's call: stash's lock-free wheel is his design. D4's
`SchedulerQueue` row measures the wheel's workload (one thread arming and re-arming), not the
scheduler's (adds from any thread, a runner thread), so on its own it cannot justify (a) (X6).

**Task-wheel diff (quest 1): not found.**
- Detent's campaign has no `task-wheel` quest. `async-drivers` (M1b) is planned and waits for this
  design to land.
- The task repository has no branch, worktree or commit for it. Its refs are main at task:bdafa5f,
  `design/cancellation`, `phase/cancellation-c1` to `c3`, `phase/mechanism-notes` and
  `phase7c/l1-task`.

The only code is the bench prototype (`async-drivers-bench/wheel.hh`, `loops.hh`
`wheel_deadline`), which Detent's review covered. I found no defect against the properties it
claims. Two notes:
- `next()` returns the end of a slot, so every timer fires up to 1 ms late (X7).
- The Debug owner check binds whichever thread touches the wheel first. Quest 1 should bind it
  when the loop starts.

## Evidence I relied on

Detent's job records, read under `~/.scratch/campaign/jobs/<id>/a1/`:
- 261110-000912-e9dff1 (`idle_cost`, exit 0). Rows at 1,000 groups:
  - journal shared store: 1,913 B and 2 descriptors;
  - single voter: 6,497 B and 3;
  - three-voter follower: 6,262 B without its channel and 170,902 B with it, 3 descriptors;
  - Detent's own group: 6,832 B and 4.
- 261110-000909-400d3c (`peers_cost`, exit 0): 511,947 B and 5 descriptors per three-voter
  member; 982,918 B and 9 per five-voter member.
- 261110-000911-f87d0f (`driver_cost`, exit 0): `run_one` takes 1,547.9 ns for a leader and
  1,760.8 ns for a follower. At 1,000 groups the wheel costs 10.613 us and 7.787 us per group per
  second at the 100 ms and 1.5 s cadences.

All three match the design. For the bench's protocol tests (`wheel_test`, `waiter_test`,
`blocking_entry`, `reader_test`) I relied on Detent's review, which read their logs and ran
mutants. My findings concern cases those tests do not model.

## What I checked myself

No probe jobs. Each finding is either a contradiction between the design and the code it builds
on, or a case the design and its bench leave out, so cited lines settle it. Rerunnable checks for
the fixes belong in the quest criteria each fix names.

I read:
- The design and its history (Detent:71742b7..f48f7cb), Detent's review at Detent:b11ab50, and
  the F15 fix (Detent:31152cd).
- The bench's `wheel.hh`, `waiter.hh`, `waiter_test.cc` (its model group), `loops.hh`,
  `blocking_entry.cc` and `idle_cost.cc`.
- consensus `core.h` (Receive, Start, drive), `types.h` and `ARCHITECTURE.md`.
- task's `scheduler_ref` and `inline_scheduler` (`task.hh`) and its teardown note.
- journal's `ARCHITECTURE.md`.
- strata's `track-c.md` (D5, D11, D12, D17) and `small-indexes.md`.
- In Xapiand: its timers and every Kronuz/scheduler user (a grep of `src` for scheduler,
  `io_context`, `asio::awaitable` and WorkPool), its catalog, chores and `opts`.
- logger's `logger.h`.
- Detent's campaign state and the task repository's refs.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| X1 | yes | defect | D10 and Q7 accept a frame "only for a group whose membership in force has that node as a member". This breaks two routine cases. A joiner's membership is empty until the log names it, so it refuses its leader's first append and install and never joins. A follower that lags an add-voter refuses the new leader's appends and never catches up. Core admits any authenticated incarnation for exactly this reason, and Track C D5 says the same. Xapiand's 7b moves replicas routinely (placement, replacement). | async-drivers.md:386-389, :533-541; consensus:ab05507 `core.h:756-759`, `ARCHITECTURE.md:31`; strata `docs/design/track-c.md:305-308` | Accept a frame when its sender is a node the loop's pins authenticate for the cluster and it names a group and destination incarnation the loop holds. Core then decides by term, incarnation and log. The group-open exchange keeps the hello's checks (cluster, group, epoch, profile), none of which is membership. Quest 5: a joiner installs and catches up over a shared connection, and a follower lagging an add-voter catches up from the new leader. |
| X2 | yes | gap | D8 frees a caller's frame only after its resume. That holds only while loops form a DAG ("callers' loops close after the group loops they wait on"), and Xapiand's loops do not. Xapiand runs one reactor per processor, D1 puts a Driver on each, and every loop's requests call groups on the others. Task's teardown destroys suspended frames without resuming them (phase 3). A waiter destroyed while still linked leaves its group's list pointing into freed memory. No destructor unlink is specified, and the bench's waiter has none. `close(Group&)` says nothing about other callers' waiters still linked when the group retires: that is a lost wake, or a later cancel hop unlinking from a freed list. Xapiand closes and drops indexes with requests in flight, and it relies on destroyed frames to release shard uses and turn leases. | async-drivers.md:294-306, :108, :292; bench `waiter.hh:16-78`, `waiter_test.cc` (no close or teardown case); task:bdafa5f `docs/mechanisms/asio-scheduler-teardown.md`; Xapiand `src/opts.cc:380`; Detent:4c09b5c `docs/SHARED_LIBRARY_DECISIONS.md:281` | (1) A waiter's destructor unlinks it, which is legal on its group's loop or in teardown phase 3, when every loop has joined. Groups outlive phase 3. (2) `close` unlinks every waiter before the group's memory goes, and resumes those whose claim it wins with a closed error. (3) Graceful shutdown is host-wide, in two steps: while all loops run, every Driver cancels its waiters and refuses new calls; then task's phases run. It is not an order between loops. Quest 4: `waiter_test` cases for close with linked waiters, a cancel racing close, and teardown with a linked waiter, under ASan. |
| X3 | yes | gap | D5 runs over `task::inline_scheduler`, which fails D8 three ways. It declares no `post_call`, so `scheduler_ref::post_call` returns false and D8's cancel hop is dropped: the cancelled caller never resumes, and step 5 throws `stalled`. Its queue is an unsynchronized deque, so a token requested on another thread races. Its only clock is manual, so a cancel cannot wake step 4's steady-clock sleep. The bench's entry models only deadlines on its own manual loop. Xapiand's chores and tools take request and shutdown tokens requested on other threads. | async-drivers.md:216-219, :230-235, :298-300; task:bdafa5f `task.hh:184-188`, `:311-349`; bench `blocking_entry.cc:78-85` | The steady-clock entry runs on a blocking scheduler: thread-safe `post` and `post_call` that wake its wait, and timers on a wheel. Xapiand's `engine::BlockingScheduler` (`src/engine/host.h:215-363`: a mutex, a condition variable, `post_call`, timers) already has that shape. Move it into task as the one blocking scheduler and make Xapiand's into it. `inline_scheduler` stays for the manual-clock simulator, with a same-thread `post_call`. `blocking_entry`: under TSan, a cancel from another thread returns `cancelled` and wakes a sleeping entry. |
| X4 | yes | gap | D10 fixes L, a power of two, for the host's life. Storage per core (D2's `TurnDriver`, Q3(b)) turns that into a bytes-on-disk rule: changing L means draining the host and re-creating it. Xapiand runs one reactor per processor, from the hardware's count, which is rarely a power of two. After 7b every node keeps its journal this way, rf = 1 included, because the journal replaces the WAL. A resized host, a moved data directory or a changed `--processors` would then refuse to start. An rf = 1 node has no replica to re-create from, since a group moves between cores only as a new incarnation. | async-drivers.md:337-339, :351-357, :520-523; Xapiand `src/opts.cc:95`, `:282`, `:380`; `.campaign/xapiand/PLAN.md:56-58`; strata `docs/design/track-c.md:622` | Options. (a) Recommended: L may change at restart. Open re-partitions the turn logs, copying each group's live records into its new loop's log the way R20 migrates per-group journals. The 16 GiB retained-log budget bounds it, about one recovery's read plus a write. The hello already carries L. (b) Key storage by a fixed power-of-two partition count chosen at creation, with loops serving partitions p mod L. That costs P/L barriers per loop per turn. (c) Keep L fixed, and ship an offline re-partition tool before 7b writes bytes; this is the minimum for rf = 1. Whichever is chosen, say where Drivers go on a host whose loop count is not a power of two. |
| X5 | yes | gap | A lone voter leads only after its randomized election timeout. Start resets the election deadline, and a quorum of one campaigns only once that deadline passes, at least 1,000 ms later. D5 step 4 adopts this, and the bench waits 2,000 ms for it. Xapiand's catalog closes idle indexes and reopens them while a request waits. At rf = 1, every reopen would hold its first write and read for 1-2 s, against strata's working target of an open under 100 us. | consensus:ab05507 `core.h:430-433`, `:1001-1004`, `types.h:113-116`; async-drivers.md:233-235; bench `idle_cost.cc:153-156`; Xapiand `src/database/pool.cc:540-550`, `pool.h:384-391`; strata `docs/design/small-indexes.md:25-27` | Core campaigns at Start when this node is the group's only voter and there is no witness. No split vote or disruption is then possible. `create` and `recover` return with a leader after one hard-state barrier, and D5 step 4 counts no election. Quest 4: a lone voter's first proposal after `recover` succeeds with no clock advance. |
| X6 | no | gap | Q6's recommendation (a) cannot hold. The logger arms deferred lines from any thread with no loop. D4's `SchedulerQueue` row measures the wheel's workload, not the scheduler's. Xapiand's answer is (b): its on-loop timers move to the wheel and Kronuz/scheduler stays for off-loop work. Retiring the library is the owner's call. | Q6 verdict above; logger:f61d8df `logger/logger.h:50`, `:555-562` | Restate Q6 as (b) with the list of what moves. Measure the logger's workload before proposing retirement. |
| X7 | no | gap | D4's "one timer mechanism" holds only for journal, consensus and `task::deadline`. reactor's `LagProbe` and event timers, http's timers and Xapiand's `AsioHost::arm` stay `asio::steady_timer`s until something moves them. `LagProbe` must stay off the wheel: `next()` wakes at the end of a slot, which adds up to 1 ms to the lag that Phase 6's 1 ms p99 stall row reads. | async-drivers.md:38-40, :143-153; bench `wheel.hh` `next()`; reactor:ab69cf2 `reactor_lag.h:103-200`, `reactor_events.h:83` | Say which timers quest 1 moves and which stay. |
| X8 | no | gap | 7b needs groups to follow Xapiand's catalog, which demotes an idle index at 30 s and closes it at 10 min. Q4 must land before 7b, with a closed state that a frame or a request reopens. D10's receive path then has to open a stored group that is closed, not refuse it. | async-drivers.md:471-476, :524-526; Xapiand `src/database/pool.h:384-391` | Put it in Q4's quest scope, and add a D10 rule for a frame naming a stored group that is not open. |
| X9 | no | gap | Q3(b) needs five conditions. (1) Group ids are opaque, assigned at creation and never derived from the index path. `slot(group)` then stays stable, and nesting (`twitter`, `twitter/user`), renames and NAME_MAX never reach the journal. (2) Reclamation and admission work per group, by name. They must not scan a directory of 62,500+ groups' files per core and read each file's header. (3) A per-core group table lets a closed group reopen without scanning the turn log. (4) A strata index stays in its own nested directory, so a checkpoint names that state (a strata commit and checksum, pinned by strata) instead of copying the index into artifacts. (5) An idle group holds zero descriptors: all three go (the `PosixIO` directory and the Store's two). | journal:e5f6ff1 `ARCHITECTURE.md` (`reclaim_step`, `Inventory`); strata `docs/design/track-c.md` D11 (checkpoints stay the group's files); bench `idle_cost.cc:146`; Xapiand `src/database/pool.cc:540` | Make each condition a quest 3 criterion. |
| X10 | no | gap | Xapiand's apply must leave the group's loop. `Committed` reaches the Sink inside a driver step, which D7's budget does not bound. Xapiand hands the range to a chore under the index's turn and calls `applied()` later; only one range waits for it. This supersedes the November 2 line "the per-index turn holds the journal append and the apply; journal carries no turn log for it". The log orders appends, the turn orders applies, and millions of indexes need the per-core turn log. | consensus:ab05507 `ARCHITECTURE.md:21-25`; Detent:4c09b5c `docs/SHARED_LIBRARY_DECISIONS.md:261` | Record it in SHARED_LIBRARY_DECISIONS. Xapiand's 7b design follows it. |
| X11 | no | nit | A cancelled `propose` returns `cancelled` both when it was never submitted (row 1) and when it may still commit (row 2). To the caller, the outcome is unknown. | D8 table | Say so in D8. Xapiand answers a cancelled write as an unknown outcome, idempotent by version. |
| X12 | no | gap | D10's connections grow with loops x peer hosts. With 16 loops and Track D3's bound of 64 peers, a host holds 1,024 connections (42 MB), against 128 in Track C D12 (two lanes per peer). With random placement over 100,000 nodes (1,325 peers), it holds 21,200 connections and 863 MB at rest. D10 therefore depends on the copyset bound. The keepalive should run once per node pair, not once per loop pair. 7b's shard RPC should use these connections rather than open a second set. | async-drivers.md:367-371; strata `docs/design/track-c.md:648-665` | State it in D10's Cost. Quest 5 measures keepalives per node pair. |
| X13 | no | gap | D6 allows one primitive per queue, so every turn of a loop's groups waits behind a large index's snapshot chunks. A 10 GB index is about 160,000 chunks of 64 KiB. | async-drivers.md:261-266 | Quest 2: turn primitives take priority over artifact and snapshot IO, measured. |
| X14 | no | nit | `Seams` carries the wheel separately from the scheduler, so a Driver can pair one loop's scheduler with another loop's wheel. | async-drivers.md:97 | Take the wheel from the scheduler; `asio_scheduler` owns it after quest 1. |
| X15 | no | nit | A cancelled `create` or `recover` leaves its group open with no caller. Xapiand's catalog must own opens (its single flight), and requests wait on an open under their own tokens. | D8 table, `create` and `recover` row | Handle it in Xapiand's 7b design. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Not run

- No build and no job of my own. No Linux numbers.
- Not reviewed: Detent's owner-loop migration (the "How Detent's owner loop moves" table, quests 6
  and 7), Q1, Q5, and Q2 beyond whether the Sink fits Xapiand.
- D9 and D10's bench code (`conn_cost`, `reader_test`, `peers_cost`): only the job rows were read;
  Detent's review covers the code.
- 7b's application checkpoint for a strata index has no design anywhere: nothing says how a
  group's checkpoint names an index without copying it. X9 states only the constraint.
- A deadline across nodes is 7b's shard RPC, not this design: the remote leg makes its own
  deadline from the remaining budget.

## Re-review

Date: 2026-11-10. Scope: the fold of X1-X15 at Detent:1ece2ff (design `docs/design/async-drivers.md`,
bench under `docs/design/async-drivers-bench/`). The first review above was at Detent:f48f7cb.
Verdict: APPROVE. All five blocking findings are closed; X10 is closed in the design and its
decisions record is still open (non-blocking, with the lead).

Do the jobs still hold at 1ece2ff? `git diff 9e7237c..1ece2ff` touches only the journal and four
job ids in the design's table (`async-drivers.md:651`), so the three X2 jobs at 9e7237c apply to
head. `git diff c351fc1..1ece2ff` in the bench touches only `waiter_test.cc` (the TSan fix at
9e7237c), so the X3, X4 and X5 jobs at c351fc1 (`blocking_entry`, `placement_test`, `lone_voter`)
run code unchanged at head. Every job was read in the job store (`campaign job show`, `a1/log`).

| ID | Status | Design / code | Jobs |
|---|---|---|---|
| X1 | closed | D10 "Identity (Q7)" `async-drivers.md:466-474`: accept a frame from a node the loop's pins authenticate for the cluster naming a group and destination incarnation the loop holds; Core decides; no membership check. Q7 (a) :634-640; quest 5 criterion (joiner installs, lagging follower catches up), :597 | none; text only, as the journal says. Quest 5 owns the proof. |
| X2 | closed | D8 "Close, teardown and shutdown" :354-375 (destructor unlinks; `close` refuses, unlinks, resumes with `closed`, drains staged, frees once nothing is queued; host-wide two-step shutdown); calls by group id :118-124; table row :328; quest 4 criteria. The model's group counters stay on the group's loop (`waiter_test.cc`, `in_flight`, 9e7237c). | 261110-042826-5c2da9 plain, 261110-042827-fb0e1d ASan+UBSan, 261110-042829-5f86fb TSan: all exit 0, "waiter_test: ok". No sanitizer report in the logs. `close` 1,796 and 1,014,239 schedules, `teardown` 319 and 133,406, 0 violations. Controls: `close-naive` breaks 977/1,824, `teardown-naive` 27/319, matching :371-375. The earlier TSan failure (76a5a7) was the model's race, fixed at 9e7237c. |
| X3 | closed | D5 :237-266 over `task::blocking_scheduler` (Xapiand's `engine::BlockingScheduler` moved into task with the wheel); `inline_scheduler` stays the simulator's; step 5 waits while a linked waiter's token can still be requested. Quest 1 criterion :593. | 261110-031128-58310d (all) and 261110-031130-32585e (`threads`, TSan): exit 0; a cross-thread cancel returns `cancelled` in 51 and 60 ms, 300 rounds. Before-job 261110-031132-189172 (`inline`) exit 1: "how=stalled dropped_post_calls=1 want=cancelled". |
| X4 | closed | D10 :389-420: placement `(slot * L) >> 12` for any L from 1 to 4,096; L may change at restart by re-partitioning at open (recover, write to the next generation, sync, commit by the layout record); hello and `misrouted` kept. Quest 3 criterion :595. | 261110-031134-9f199a: exit 0; `remap` 8->6, 6->12, 16->3, 3->1 keep every group's live bytes (copied = live) across the crash points; `share` bound holds for L pairs 6/8, 24/32, 8/6. Before-job 261110-031136-e8b259 (`placement_test mod`) exit 1, "117629 over the bound". |
| X5 | closed | D5 step 4 :260-263 counts no election; Core campaigns at `Start` for a lone voter with no witness; quest 4 criterion :596. | 261110-031128-58310d: `rule=d5 case=first-election advances=0`, and the `core=today` control fails with 1,500 ms as expected. Before-job 261110-031138-999c25 (`lone_voter` on consensus:ab05507) exit 1, first leader at 1,501 ms. This proves the rule in the model; the real Core change is quest 4's and `lone_voter` stays red until it lands. |
| X6 | closed | Q6 restated as (b), with the list of what moves, :626-633. | text |
| X7 | closed | D4 :207-213 says what moves and what stays, and that `LagProbe` stays off the wheel; owner check binds when the loop starts (quest 1, :593); Limits :581-582 say up to 1 ms late. | text |
| X8 | closed | Q4 :620-623 and the build plan :603-605 put the closed state before 7b; D10 :476-479 opens a stored closed group on a frame. | text |
| X9 | closed | Q3 (b) :615-619, and the five conditions as quest 3 criteria :595. Per host rather than per core, which follows from X4. | text |
| X10 | partially closed | D2 :130-135: the Sink applies nothing inline and `applied` comes later by `post_call`; it supersedes the November 2 line. The Detent decisions record still carries the old line: `docs/SHARED_LIBRARY_DECISIONS.md:261` (Detent:4c09b5c, unchanged). | n/a. The journal lists it for the lead (Next 3); it is outside the quest's file set. |
| X11 | closed | D8 table row 2, :323. | text |
| X12 | closed | D10 Cost :442-452; quest 5 measures keepalives per node pair, :597. | text |
| X13 | closed | D6 :297-300; quest 2 criterion :594. | text |
| X14 | closed | D2 :122-124 (`scheduler_ref::wheel()`); quest 1 criterion :593. | text |
| X15 | closed | D8 table, `create`/`recover` row, :327 (the host owns opens, single flight). | text |

New notes, non-blocking:
- The X2, X3 and X5 jobs run the bench's models of the protocols (`waiter.hh`, `blocking.hh`, the
  `Core` rule), not the production types. That is the right level for a design. Each quest's
  criteria say the same tests are re-run against the real types.
- X10's remaining step is a one-line edit to `SHARED_LIBRARY_DECISIONS.md:261`, which the lead
  makes. Xapiand's 7b design follows D2's Sink text in the meantime.
