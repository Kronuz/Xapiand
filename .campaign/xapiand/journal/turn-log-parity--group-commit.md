# turn-log-parity, task group-commit: journal

State: waiting on jobs 261121-151521-37deb9 261121-151523-00cd16 261121-151525-9704d9 and CI at strata:80f91fd0
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)
Branch: quest/xapiand/turn-log-parity--group-commit (strata), worktree
strata-wt/xapiand/turn-log-parity--group-commit.

## Doing

strata:80f91fd0 (pushed): `barrier_cohort` checks the cohort wait on turns the sealer records,
not on wall-clock. CI run 44934094130 at strata:2f236c94 failed it on freebsd-x86_64 (K=4 I=4 L=1)
and macos-arm64 (K=4 I=2 L=2): its "every closed-loop commit rides exactly the next barrier" check
breaks when a runner stalls a writer past the cohort bound (1/16 barrier, 4 ms).

The property now checked, per log: the turns released writers missed, summed, are at most the
log's `late_arrivals()`. `turn_sealer` records, under its mutex:

- `last_ack()` (thread-local): the turn that made each `await()` durable, and whether that turn
  released the writer from a wait (so it owed the next turn an arrival).
- `late_arrivals()`: at each written turn's start, the released writers not back, counted only for
  the last turn's cohort once its wait ran out at the bound (`expired_`), plus any an earlier turn
  released (`owing_`, decremented on each owed join).

A turn that starts before its released writers are back, its bound not run out (the group-count
mutant: the second writer of a group is not waited for), records no late arrival for them, so
their missed turns exceed the allowance whatever the scheduler does.

## Next

1. Read 261121-151521-37deb9 (A4 test), 261121-151523-00cd16 (mutant, must fail on the check, not
   the build), 261121-151525-9704d9 (suite), and CI at strata:80f91fd0 (every platform).
2. Lead: merge into the quest branch once CI is green.

## Evidence

| Criterion | Evidence |
|---|---|
| A4 | 261121-151521-37deb9 (pending); fails without: 261121-151523-00cd16 (pending, mutant strata:191551bf at refs/mutants/group-commit-cohort-groups) |
| suite | 261121-151525-9704d9 (pending) |
| CI | strata:80f91fd0 (pending) |

Superseded at strata:2f236c94: A4 261121-101429-f8726f, mutant 261121-101433-65f5c5 (strata:017bb849,
the old group-count code, which lacks the new accessors), suite 261121-114248-d19804.

## Rulings I made

- The mutant is rebuilt on the new head (strata:191551bf): the cohort the wait uses counts
  released groups while `released_` still counts writers, the old behavior expressed in the new
  code. The old mutant strata:017bb849 reverts the accessors and would fail to compile, which
  proves nothing.
- Round 0 is not checked (no previous acknowledgement). A write after an acknowledgement that
  released nothing (the turn was written before the writer parked, a stall over a whole barrier)
  owed no arrival and is printed with `*`, not counted.
- The check is an upper bound, not equality: a writer that joins after a turn's snapshot but
  before its seal rides it yet counts late. That leniency never covers an early start, which
  records nothing.

## Blocked on

Nothing.
