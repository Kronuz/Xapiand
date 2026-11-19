# task-lifecycle-followups: journal

State: working
Quest: .campaign/xapiand/quests/task-lifecycle-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at task:4c73dbb (branch head) and mutant branches `mutant/tlf-*` (local only,
evidence, never land).

## Next

1. Read the jobs; fill Evidence from `campaign brief`; set ready for review.

## Blocked on

Nothing. Q1-Q3 built as owner defaults, pending the owner's ruling (see Rulings). For the lead:
`docs/API.md:107` and `docs/design/service-shape.md:86` still show `debounce`'s old
`fire(key)` signature and timing; both are outside this quest's file set. `campaign brief` flags
test/{lifecycle,timers}_test.cc as outside the set: a glob-parsing quirk (the set names them). CI
has no run: the lead said not to push.

## Evidence

Jobs: suite 261119-140132-6f55be, tsan 261119-140132-d04ab6, asan+ubsan suite
261119-140207-206045, tsan x50 of the concurrency tests 261119-140209-c56112. Mutants
(`--fails-without`, each the named subtest at a branch reverting one behavior): A1 inbox
261119-140140-9aee97; A2 flush-inline 261119-140143-816688; A3 cause 261119-140145-8db73c; A4
abandoned 261119-140148-04f388; A5 flags 261119-140151-bc8368, throttle 261119-140153-e85702,
fire_pending's wait 261119-140154-3b9118, stop keeps pending 261119-140156-7d67fb, fired once
261119-140158-d2568f, touch after stop 261119-140159-a8816d, throwing fire hand-off
261119-140201-b19aab.

| Criterion | Evidence |
|---|---|
| A1 | 261119-140132-6f55be; fails without: 261119-140140-9aee97 |
| A2 | 261119-140132-6f55be; fails without: 261119-140143-816688 |
| A3 | 261119-140132-6f55be; fails without: 261119-140145-8db73c |
| A4 | 261119-140132-6f55be; fails without: 261119-140148-04f388 |
| A5 | 261119-140132-6f55be, 261119-140207-206045 (ASan+UBSan), 261119-140209-c56112 (TSan x50, 0 reports), tsan check 261119-140132-d04ab6; fails without: 261119-140151-bc8368, 261119-140153-e85702, 261119-140154-3b9118, 261119-140156-7d67fb, 261119-140158-d2568f, 261119-140159-a8816d, 261119-140201-b19aab |

Every mutant job fails on the CHECK of its own behavior (logs checked), not on a build error.

## Result

task commits: 4aecbae (F6 queued counts inbox), ea2674c (Q3 flushed_inline), 3e46835 (Q1
cause), 3e5ff12 (Q2 abandoned), dd2c7bd (debounce flags, throttle, fire_pending), 4c73dbb (docs).
How it works: `docs/mechanisms/lifecycle.md` (periodic and debounce; lifecycle).

## Rulings I made

- Q1 (owner default, pending): the fail() that caused the stop is listed first. Commit
  task:3e46835, revertable alone. `stop_source` records whether its first fail() came before any
  other stop request (`cause()`).
- Q2 (owner default, pending): `stopped.abandoned`. Commit task:3e5ff12, revertable alone.
- Q3 (owner default, pending): `stopped.flushed_inline` holds the thread start's error. Commit
  task:ea2674c, revertable alone.
- A stop keeps the debounce's pending keys for `fire_pending()` instead of dropping them: the
  lifecycle's stop precedes the flush and the third signal, so dropping them would leave
  `fire_pending()` nothing to fire (fsyncher-on-task A1). No current user of `task::debounce`.
- `fire_pending()` waits for a fire in progress on the loop before it fires its keys, so a touch
  before it has fired when it returns; it must not be called from a fire (documented).
- After a fire the throttle keeps the entry and its watch; target on a touch is
  `max(throttle end, min(now + quiet, force))`, today's debouncer's order (the throttle beats force).
- A throwing fire drops a throttled entry (the throttle is skipped) and hands a touched one to a
  new watch; the throw still reaches the group's `failed`.
- `stopped.flushed_inline` is an `exception_ptr` (the thread start's error) rather than a bool, so
  `main` can log why.
