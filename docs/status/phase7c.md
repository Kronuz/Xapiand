> Status: CURRENT (2026-10-29) the Phase 7c design ledger.

# phase7c (design)

Branch `design/phase7c` from Xapiand master `3b0dc433f`, worktree `Xapiand-wt/phase7c-design` (local
only, never pushed). Design: `docs/design/phase7c.md`. Review: `docs/reviews/phase7c-design.md`.
Scope: strata `docs/PLAN.md` Phase 7, 7c (`:651-674`, owner, 2026-10-29).

## State

designing (revision 2, ready for the architect's re-check). Revision 2 closes R1 to R7 with
decisions, folds C1 to C4 into S1's done criteria and C5 to C10 into the decisions and rows they
name, splits the Xapian deletion into X1 with its own done criteria, and records the owner's
answers to Q1 to Q6. No build, probe or measurement was run for revision 2.

## Done

- Revision 1 of the design and this ledger: `Xapiand:0a8d235b8`, `Xapiand:fdcc91862`.
- Architect's preliminary review, APPROVE WITH CONDITIONS for S1 only: `Xapiand:e30a844ff`.
- Revision 2 (see the log for how each finding closed): `Xapiand:2d8c2daab`.

## Next

- Lead: the architect's re-check of R1 to R7 against revision 2 (the review's probes are kept in
  `.scratch/review-7c/`).
- Startable now, needing no open question: S1 (with C1 to C4), X1 (the Xapian deletion), and L1 to
  L6 on their own branches.
- X2 onward wait for the re-check and for L1 to L6.

## Blocked on

Nothing. Q1 to Q6 are answered (strata `docs/status/xapiand-on-strata.md`, "Owner answers on the 7c
design"). Detent's move off http's offload path is coordinated separately; Detent keeps its http
pin until then, and nothing in 7c waits on it.

## Log

### 2026-10-29

- Revision 1 read: strata PLAN, ARCHITECTURE "Concurrency and async", `xapiand-on-strata.md`,
  `write-buffer.md`; Xapiand AGENTS, ARCHITECTURE, `offload-bridge.md`, `engine-offload-architect.md`,
  `phase7a.md`, `src/engine/`, the pool, handler, shard, schemas, resolver, Lua, manager and HTTP
  service code; the reactor, http, task, completion, journal, residency, threadpool, scheduler,
  logger and atomic-shared-ptr libraries; Detent's use of http's offload.
- Revision 2 read: the review (R1 to R7, C1 to C10, probes); the owner's answers; task `aa447e7`
  (`budget`, `running_on`, `cancel_source`, `asio_scheduler`, `as_awaitable`); reactor `0c04d0d`
  (`OffloadGate`, `track`, `abort_all`, `wait_sessions`); http `6a07f99` and `dc3be77`
  (`ConnectionInput::monitor`, the streamed and offload paths, `BodySink`, `stream`); threadpool
  `feature/scheduler` (`WorkItem`, class lanes, reserve, `merge_cap`); strata `d93bf7d` (include
  unchanged at `5b0e6ed`: `apply`, `check_out`, `check_in`, `writer_scope`, `run_chores`, `drain`,
  `reader_handle`, `pool_executor`, `hazard.hh`, `owner.hh`); storage `671eaac` (`write`, chunked
  `read`); Xapiand `processor.cc`, `shard.cc` storage paths, `search_service.cc`, `opts.cc`, and the
  sites F8 added.
- How revision 2 closes each blocking item:
  - R1: each Lua call runs `func(env)` with its own `_ENV` table over a shared base; the hook yields
    only when yieldable; a non-yieldable bound, an instruction cap, a live-byte cap and a matcher
    step-budget patch end runaways (D10, T10).
  - R2: a slice is a continuous run, stamped by `running_on` at every resume; the policy is called
    two-level; T5's light mix has write-then-GET pairs (D2).
  - R3: `OffloadGate` stays as `AdmissionGate`, counting in-flight requests per reactor beside the
    demoted cap, each with its own 503; T8 runs light and heavy overloads (D2).
  - R4: every lease operation is a turn, timers included; the registry has `closing` and a retry
    rule; one turn per task (schema writes first), and `lock_until` deadlines answer 503 (D5).
  - R5: Xapiand never calls `check_out`, so no chores run inline; strata's `defer_io` returns
    `flush_owed` or `drain_owed` instead of doing I/O; faults stay Phase 6's (D6).
  - R6: `task::await_asio` bridges Asio to tasks; the disconnect monitor requests a cancel source
    for reads; the handler await is not cancellable and frames end only by completion; shutdown
    order is D16, tested in T7 (D3, D16).
  - R7: D9 is "blob I/O in chores over `DataStorage`"; `storage_commit` is named in D7; uploads above
    the body cap stream into their own volume through a new storage streamed write (L6); X1 keeps
    `DataStorage`.
- Not opened: `src/xapian/`, `src/sortable_serialise.{cc,h}`, `src/server/remote_protocol_views.cc`.
  Their deletion in X1 is by `git rm`. `sortable_serialise` users were found by grepping other files.
