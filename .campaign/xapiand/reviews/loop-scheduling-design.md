# loop-scheduling review: design (concurrency)

Commit: Xapiand:37d80c1d54cdaff3809f94c59ff450cfc0c7dc0b
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right. Two lanes keyed by attained service, the drainer's one place in the FIFO as the
heavy lane's progress guarantee, and the lane carried in the captured `scheduler_ref` are sound.
Every cross-thread path into the heavy lane goes through `asio_scheduler::post_demoted`, so D2 (e)'s
hop is complete. With `light_slices` 0 the library keeps today's order. One blocker: the design
says RESTORE stays flat, but the jobs it cites show a RESTORE stream under the T4/T5 mix running
about 0.7x on macOS. RESTORE is exactly the heavy-forever request that alternates CPU and I/O (Q1).
It is one text round: state the cost, give the owner options, and add the check.

## Evidence I relied on

- X1 off arm 261112-163009-f675a9: the per-request slice counts in D1's table. The job as a whole
  timed out, but the off arm's outputs are complete. The X1 table matches them.
- X2a 261112-170146-163a2c, X2b 261112-170149-32915c, X3 261112-170152-275ba3, X4
  261112-175900-eaaa36 and 261112-175902-15a8eb: the lane/off ratios, read from their
  `a1/out/*.json`. All ran prototype Xapiand:3c064618a, not this commit. That is right for a design
  review: the quest branch holds only the note.
- E1 261112-145754-4dcc08, E3 261112-141805-74aaf9, E2 261112-154306-4d90d5: the census, which D79
  and D80 already ruled on. I did not re-judge it.

## What I checked myself

- Probe job 261112-183821-695fc9 (script
  `Xapiand-wt/xapiand/loop-scheduling/.scratch/review-loop-scheduling-design/restore_under_load.py`).
  For every arm of X2a, X3 and X4 it reads RESTORE's server-time histogram from the last
  `LS_REQ_OUT` snapshot, plus the loaded phase's length and restore count. Rerun it on a fix with
  the same job ids.
- Code read: Kronuz/task at the pinned bdafa5f (`task.hh` scheduler_ref :169-200, running_on
  :282-299, final_awaiter :505-515, yield :797-805, budget :1148-1184, resume_on/event/lock_waiter
  :1187-1330; `task_asio.hh` :58-216). Also the prototype diff (`.scratch/merge/harness/experiments/task-lane`
  against bdafa5f), Kronuz/http at 51d153aa1 (`http_asio.h:565-579`, `http_body_stream.h`),
  Kronuz/reactor at ab69cf26e (spawn sites :414, :678, :684), and Xapiand `src/database/pool.cc:93-135`,
  `src/engine/host.h:70-100, :165-185, :315-335`, `src/engine/host_asio.h:46-190`,
  `src/server/search_views.cc:943, :1040, :1458, :3094, :3156-3167` and `src/opts.cc:95, :188, :282, :380`.
- Users of `asio_scheduler` in Detent, Prism, prospect, completion and consensus. Every construction
  is over an `io_context` executor. None calls `post_demoted` off the loop.

Answers to the lead's questions:

1. **Heavy forever (Q1).** The rule works for requests whose phases are other requests: each scroll
   page and each keep-alive request gets a fresh budget, and D2 (f)'s `enter_light` fixes the
   connection's scope. Aggregations are heavy in every arm (40 slices at the median), as they should
   be. RESTORE spends 64+ slices in every arm, so it is heavy for life. Today every body read or
   chore brings it back to the FIFO with a fresh slice. Under the lane it round-robins with the
   loop's heavy searches, one turn per pass. That costs about 0.7x RESTORE throughput under load
   (F1). For a light request misclassed heavy, each remaining turn waits behind the heavy lane: about
   p x (slice + unit), where p is the number of heavy handles queued, so a few ms at the measured
   p of about 4, and up to about 90 ms per turn at the 256 cap. It never recovers (F3). X1 puts
   that at 0.4% of light searches under the oversubscribed macOS mix.
2. **Starvation (Q2).** D3 holds in both extremes. Every pass with a non-empty heavy lane runs
   exactly one heavy turn, because the drainer holds one FIFO place and reposts itself. When every
   request is heavy, their Asio completions also route to the heavy lane, so the FIFO holds only
   the drainer, timers and the connection plumbing. Passes are then short, and the heavy lane is a
   plain round robin. Under a continuous light flood, a heavy turn waits one pass, which D3 caps at
   K slices per light request. Turns are tied to the slice only where the code checkpoints. A heavy
   resume after I/O runs to its next checkpoint (for example, response serialization), so that
   remains the stall quests' bound, not this one's (F7). Measured: heavy max latency 397-464 ms with
   the lane against 302-355 ms off at 16 loops (X2a), but 257-261 against 300-334 ms at D80's 8
   loops (X4). There is no starvation tail.
3. **Cross-thread (Q3).** Complete. Chores (`pool.cc:127-133`), `event::set`, `async_mutex` wakes,
   `asio_park` cancels from another thread, `SleepFor` and timer expiries, and a child finishing on
   another scheduler (`final_awaiter`'s `continuation_scheduler.post`) all end in
   `scheduler_ref::post`. That routes a heavy reference to `asio_scheduler::post_demoted`, where the
   thread check lives. Teardown is safe because the hop handler re-enters `post_demoted`, which
   checks `closed()`, and a hop the closed loop drops unrun owns nothing. The lane bit cannot race:
   it is a value inside each captured `scheduler_ref`, and the only mutable copy is thread-local.
   Spawns never inherit it, since Host, reactor and control plane all spawn with a fresh
   `scheduler_ref::of(...)`. It can leak in two benign ways (F4): back to light when a child
   completes on a ref of another identity (`final_awaiter` compares `self_`), and across a request's
   separate script budgets.
4. **Library split (Q4).** Yes, provided three things hold. `light_slices` 0 must demote without
   entering the heavy lane, as D5 says; test 5 needs that as its mutant (F6). `operator==` must ignore
   the lane (F4). The extra member grows every coroutine frame and awaiter by 8 bytes unless the
   bit is packed (F5). The one Kronuz/http line (`http_asio.h:578`) is the only inline resume in
   http and reactor at their pins. `BodyStream` goes through `await_asio`, and reactor only spawns.
5. **Physical cores (Q5).** macOS `hw.physicalcpu` is correct, and macOS has no affinity masks. On
   Linux, D6's `(physical_package_id, core_id)` key is the fragile part (F8). Affinity is honoured,
   but cgroup CPU quotas are not, today or after. FreeBSD's `kern.smp.cores` ignores a jail's cpuset
   (F8). A 1-core/2-thread host gets 1 loop and a 2-core host without SMT keeps 2, both from the
   `max(1, ...)` at `opts.cc:380`. That is fine, but `--processors` can no longer raise the loop
   count above the cores (F9).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | The design says "RESTORE stays flat (0.99x)", from X2b's T12 loadtest, where RESTORE runs alone. The same jobs show a RESTORE stream under the T4/T5 mix slowing with the lane on macOS. In all 4 lane arms (X2a at 16 loops, X4 at D80's 8 loops), both full loaded RESTOREs of 20,000 docs take 8.4-16.8 s of server time; in all 4 off arms, every loaded RESTORE takes 4.2-8.4 s. Phase lengths (3 restores in 21.1-21.3 s off, 22.7-23.0 s lane, the third finishing after the heavy clients stop) put it at about 7 s against 10 s, or about 0.7x ingest throughput under query load. The VM's buckets cannot tell the arms apart. Cause: RESTORE is heavy for life (64+ slices), so its body reads and chores round-robin with the heavy searches instead of returning to the FIFO. | probe 261112-183821-695fc9; design :25, :275 | State the cost in the answer and under "What the numbers say", and drop "RESTORE stays flat" except for T12 alone. Offer the owner options: (a) accept it, so ingest shares the loop fairly with heavy search; (b) class a body-streaming request by its slices between body reads; (c) give the heavy lane two turns per pass while a writer is queued. Add RESTORE-under-mix to A3's check: stall_load records each RESTORE's duration, and a lane/off ratio is reported from X2a's procedure. |
| F2 | no | gap | Note for the lead, not a relitigation. D80 records the VM result as "0.43-0.57x on the VM (T5 passes there)", which was measured at 4 loops. The strata VM has no SMT, so D6 gives it 8 loops, which is X3's configuration: T5 misses by 1.5-1.7x. The design's Q3 says this correctly ("on hosts without SMT it is the lane alone"); PLAN.md's D80 row does not. A2 should record the VM's T5 as a miss too. | design :364-365; PLAN.md D80 | Correct the D80 row's VM clause, or tell the owner. |
| F3 | no | nit | D1 calls K = 8 "1.6 ms of CPU", and Q1 says it "is attained CPU". `budget` measures wall time inside a resume (`cycles::read()` against `slice_start`, `task.hh:1158-1160`), so a loop descheduled mid-turn spends slices it did not compute. Misclassing therefore grows with oversubscription, which is the condition E1 measures (loops ready 32% of runnable time). X1's 0.4% already includes that, and D6's fewer loops lowers it, so K stands, but Q1's reasoning is wrong. | task.hh:1158-1160; design :16, :357 | Say "1.6 ms of loop time while resumed". State the misclass cost (each remaining turn waits behind p heavy handles) and that heavy is sticky. |
| F4 | no | gap | The design does not state the lane's exact semantics. (1) `scheduler_ref::operator==` must ignore the lane, or `final_awaiter` (`task.hh:511`) stops transferring when a child turned heavy, and the parent resumes light through its captured ref. The prototype compares `self_` only, which is right; write that into D5. (2) Heavy also leaks back to light when a continuation was captured on a ref of another identity (Debug's `Counted`, a Host ref against the asio_scheduler ref). The budget's count restores heavy at the next spent slice, so this is bounded at one slice. (3) The three script budgets (`search_views.cc:943, :1040, :1458`) count apart from the request's, so a request with a script can spend up to K x 2 - 2 slices while light. | task.hh:511; prototype task.hh `post`/`with_heavy`; search_views.cc:943 | In D5: equality ignores the lane; checkpoint goes heavy when `spent >= light_slices` or the scope is heavy (as in the prototype); script budgets share the request's count, or the design states the looser bound. |
| F5 | no | nit | `bool heavy_` grows `scheduler_ref` from 16 to 24 bytes, in every promise (`continuation_scheduler`), event awaiter, lock_waiter, asio_park and chore. That is a memory ratchet cost in every frame, for every user of Kronuz/task. | task.hh:197-203 | Pack the bit into `vt_`'s low bit (vtables are aligned), or state the cost. |
| F6 | no | gap | A4's library test 5 (the default is unchanged) has no mutant. The likely bug is `light_slices` 0 read as K = 0, which makes every default demotion sticky-heavy for Detent and Prism. | design :347-348 | Mutant: `light_slices` 0 enters the heavy lane. Also assert that an off-thread `post_demoted` on a non-`io_context` executor (`io_` null) is unsupported, because the prototype's check (`io_ &&`) would push from the foreign thread. |
| F7 | no | nit | D3's "one slice plus one unit of work" holds only for a turn that ends at a checkpoint. A heavy resume after I/O, a chore or a body read runs until its next checkpoint or suspension, so un-checkpointed sections bound the light wait as they do today. | design D3 :155-156 | Say so in D3 and name the stall quests that own those sections. |
| F8 | no | gap | Core detection. Linux's `(physical_package_id, core_id)` relies on `core_id` being unique within a package, but `core_id` is platform-defined (multi-die x86 and arm64 PPTT layouts differ). The sibling sets `topology/core_cpus_list` (older kernels: `thread_siblings_list`), intersected with `sched_getaffinity`, count cores with no ID assumptions. FreeBSD's `kern.smp.cores` ignores the process's cpuset (a jail with 4 CPUs on a 32-core host gets 32 loops); `cpuset_getaffinity` divided by `kern.smp.threads_per_core` would match Linux. Pre-existing and unchanged: cgroup `cpu.max` quotas are ignored, and readers, WAL and fsync threads still follow the host's logical count while loops follow affinity on Linux only. | design D6 :219-226; opts.cc:95, :282 | Use sibling sets on Linux; add a multi-die tree and a cpuset-split tree to the topology unit test; handle the FreeBSD cpuset or state it. |
| F9 | no | nit | Owner's call, not blocking. Under D6, `--processors` only caps the loop count, so no option gives one loop per logical CPU (option (a)). Today `--processors N` gives N loops. | design :219, :324; opts.cc:380 | Options: (a) keep it, (b) let an explicit `--processors` set the loop count, (c) add a loop-count option. |
| F10 | no | nit | D4 recounts `demoted()` as every queued heavy resume, not only spent slices, but keeps the 256 cap. Under heavy overload the 503 now triggers at fewer runnable heavy requests than today. The mixes measured have 0 503s, so nothing shows the new threshold. A heavy request that fans out child tasks on its loop also takes one heavy turn per child per pass. | design D4 :177-178; search_views.cc:3094 | State the threshold change, and that fairness is per handle, not per request. |
| F11 | no | nit | D2 (f) marks the scope light "on entry and on exit". If the exit marking is not a scope guard, a request that throws (cancelled, deadline) leaves the connection's next header read in the heavy lane for one wait. Entry marking bounds the cost. | design :138-140 | Make the exit marking RAII. |

## Not run

- No build or run of the prototype or this commit, no TSan run of the cross-thread hop, and no new
  measurement. All numbers come from the existing X/E jobs' outputs.
- FreeBSD and Linux multi-die topology were not checked on hardware. F8 rests on the sysfs ABI's
  stated platform dependence, not on a failing host.
- RESTORE under the mix on the VM: the log2 buckets cannot tell its arms apart, so F1 rests on
  macOS (8 of 8 arms consistent).
- I did not review x7-loop-fix-c or the census method (E1-E3), which D79 and D80 already ruled on.
