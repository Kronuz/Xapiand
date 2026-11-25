# consensus-group-last: consensus: Driver reports a group's recovered log end

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-group-last`.

## Outcome
`Driver::GroupState` reports `last`, the group's log end (its last index) once `recover` returns, so a
consumer can compare a recovered log with what it already applied before it touches its own state.
Nothing else changes: no new call, no behavior change.

## Acceptance criteria
- A1 A test recovers a group from a log with N entries (and from a closed record, and from a checkpoint
  plus a tail) and asserts `state(id).last` equals the worker's `last_index()` in each case, and that it
  advances with appends; a mutant that reports `committed` instead is killed (`--fails-without`).
- A2 The suite passes: the `build-test` check.
- A3 [artifact] `CHANGELOG.md` names the field.

## Contracts
Provides `consensus:group-last` (one field in `Driver::GroupState`). Consumed by
`strata-journal-behind`. Detent's lead is told before landing (consensus is shared).

## Decisions
- Lead, 2026-11-24: split from journal-replaces-wal's durability review round 3, finding S1 (a journal
  older than its index corrupts the index); the contributor's Blocked on 13 names this API.

## May land early
Nothing: one field.

## Tasks

<The steps that build the feature, one agent each: `T1 <what> (files; assignment, tier, effort;
after T0)`. Each merges into the quest branch on its own tests.>
