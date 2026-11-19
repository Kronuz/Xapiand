# task-lifecycle-followups: journal

State: working
Quest: .campaign/xapiand/quests/task-lifecycle-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A5: debounce flags, throttle, fire_pending in task_timers.hh (task worktree).

## Next

1. A5 debounce: touch(key, flags), timing::throttle, fire_pending(); hook tests.
2. Docs (docs/mechanisms/lifecycle.md); mutant branches; verify + TSan/ASan jobs.

## Blocked on

Nothing. Q1-Q3 built as owner defaults, pending the owner's ruling (see Rulings).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |

## Result

Pending.

## Rulings I made

- Q1 (owner default, pending): the fail() that caused the stop is listed first. Commit
  task:3e46835, revertable alone. `stop_source` records whether its first fail() came before any
  other stop request (`cause()`).
- Q2 (owner default, pending): `stopped.abandoned`. Commit task:3e5ff12, revertable alone.
- Q3 (owner default, pending): `stopped.flushed_inline` holds the thread start's error. Commit
  task:ea2674c, revertable alone.
