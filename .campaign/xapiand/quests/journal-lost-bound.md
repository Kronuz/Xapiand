# journal-lost-bound: damage bounds a group's loss instead of losing the group

Risk: high (durability). The lead writes this quest; contributors read it and never edit it. Its
machine state (status, branch, checks, reviews) is in `state.json`: `campaign status journal-lost-bound`.

## Outcome

Recovery of a damaged block no longer loses every group its manifest names. `TurnLog::recover`
(turn_log.h:605-611 at journal:178030b) reports, per group, a bound: the highest sequence the damage
could hold. It keeps that group's records after the bound. `Host::open` stores the bound durably in
the group's `GroupEntry`, removes no artifacts, and reports it through a group status. A reader
whose checkpoint covers the bound replays the records after its head; one whose checkpoint does not
is lost, loudly. strata's turn-log-parity A2 (tolerant open, `log_lost`) builds on this; it is
rocksdb-parity D2's items 1-2. Detent's lead reviews the diff before it lands (turn-log-parity's
quest file, Detent's constraints of 2026-11-20): journal's I/O surface and completion ordering stay.

## Acceptance criteria

Gate: CI (`build-test`, `asan`, `tsan`).

- A1 After damage to one block, a group whose bound is at or below a reader's covered sequence keeps
  every record after the bound, and one above it reports the loss: a test with values over several
  groups per block; a mutant that reports `lost` for every named group fails it (`--fails-without`)
- A2 The bound survives a crash between storing it and the next open, and a second damage keeps the
  lower bound: a test with values, plus the existing power-loss and double-damage suites green
- A3 [artifact] `docs/mechanisms/lost-bound.md` explains the bound, where it is stored and who may
  replay past it

## Contracts

journal's recovery result and group status gain the bound (`TurnRecovery`, `GroupStatus`); strata
consumes it in turn-log-parity's tolerant-open task.

## Decisions

- Lead, 2026-11-21: option 1 of the tolerant-open task's three (bound per group); option 2 costs a
  durable table write per checkpoint per index, option 3 contradicts turn-log-parity's outcome.

## May land early

None.
