> Status: CURRENT (2026-10-29) the Phase 7c design ledger.

# phase7c (design)

Branch `design/phase7c` from Xapiand master `3b0dc433f`, worktree `Xapiand-wt/phase7c-design` (local
only, never pushed). Design: `docs/design/phase7c.md`. Scope: strata `docs/PLAN.md` Phase 7, 7c
(`:651-674`, owner, 2026-10-29).

## State

designing (ready for preliminary review). Revision 1 is complete: inventory (169 call sites, 5 kinds of
unbounded CPU, 11 blocking-work mechanisms, 3 destructors), D1 to D15, T1 to T13, steps with step 1's
done criteria, Q1 to Q6. No build, probe or measurement was run.

## Done

- Revision 1 of `docs/design/phase7c.md` and this ledger (this commit).

## Next

- Lead: preliminary review (architect; a phase's first design and a concurrency shape). Owner: Q1 to Q6.
- Step 1 (S1, tooling and the pre-7c baseline) and L1 to L5 can start now; no open question blocks them.

## Blocked on

Nothing for the draft. X1 and X2's scope wait on Q1; X1's `DataStorage` deletion on Q4; L3's scope on
Q3.

## Log

### 2026-10-29

- Read: strata PLAN (goals, ratchets, every gate, Phases 0, 6 to 8), ARCHITECTURE "Concurrency and
  async", `xapiand-on-strata.md`, `write-buffer.md` (`design/write-modes` `ae2cf70`); Xapiand AGENTS,
  ARCHITECTURE, `offload-bridge.md`, `engine-offload-architect.md`, `phase7a.md`, `src/engine/`, the
  pool, handler, shard, schemas, resolver, Lua, manager and HTTP service code; the reactor, http, task,
  completion, journal, residency, threadpool (`feature/scheduler`, research note), scheduler, logger and
  atomic-shared-ptr libraries; Detent's use of http's offload.
- Not opened: `src/xapian/`, `src/sortable_serialise.{cc,h}`, `src/server/remote_protocol_views.cc`.
- Inventory counts come from greps on `3b0dc433f`; step 1's `harness/async_check.sh` (K5) turns them
  into a rerunnable check.
