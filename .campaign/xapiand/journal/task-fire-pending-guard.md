# task-fire-pending-guard: journal

State: ready for review
Quest: .campaign/xapiand/quests/task-fire-pending-guard.md (the lead's; never edit it)

## Doing

Nothing: every criterion has evidence at task:f937bc2.

## Next

1. Review; the lead pushes for CI (I was told not to push).

## Blocked on

Nothing. CI: the lead said not to push, so A3's "CI green" has no run from this quest.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261119-145232-a31610 (Debug suite, debounce-reentry aborts); fails without: 261119-145237-42104f (task:f123b5c, no assert: SIGALRM, not SIGABRT) |
| A2 | N2 fixed: 261119-145232-a31610, 261119-145234-48e3e7; fails without: 261119-145239-811bb9 (task:46ea792, waits for every fire: debounce-overlap starves). N3 stated as the contract: docs/mechanisms/lifecycle.md, docs/API.md, header comment |
| A3 | 261119-145232-a31610 (suite), 261119-145249-853290 (TSan suite), 261119-145234-48e3e7 (TSan x50 mid-fire, overlap, after-stop, reentry; Debug). CI: not run, branch unpushed by instruction |

## Result

task:f937bc2. `debounce::fire_pending()` asserts (Debug) when called from the loop's fire of the
same debounce (thread-local chain of loop fires); it waits only for fires in progress at the
call (intrusive ticket list in place of the `firing_` count), so pre-stop back-to-back fires no
longer starve it. N3 is the documented contract. debounce_mid_fire is a handshake (hook or
return), no 300 ms window. New tests: debounce-overlap, debounce-reentry (test/timers_test.cc).
How it works: docs/mechanisms/lifecycle.md "periodic and debounce". Mutant commits are on local
branches mutant/fpg-no-assert and mutant/fpg-wait-all in the task worktree.

## Rulings I made

- N1: the marker is a thread-local chain of the loop fires on this thread (a node in each watch
  frame, owner pointer), not a bare "in a fire" flag, so another debounce's fire may still call
  `fire_pending()` (tested) and a nested A-fire -> B.fire_pending() -> A.fire_pending() is caught.
  Maintained in every build (no `NDEBUG` member difference, no ODR split); only the assert is Debug.
- N2 fixed, not documented: `firing_` count replaced by an intrusive list of fires in progress
  with tickets; `fire_pending()` waits until the oldest ticket is past the last issued at the
  call. Exact under concurrent fires, no allocation.
- N3 documented as the contract (API.md, mechanisms/lifecycle.md, the header comment): a fix
  would need a held entry the loop's watch must wait on, for callers that do not exist.
- N8: debounce_mid_fire's fire waits for the hook or for fire_pending() to return (a mutant
  that skips the wait returns, so it fails, never hangs); no deadline.
