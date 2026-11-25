# consensus-recover-gate: consensus: recover refuses, before Start, a log that retains no accepted boundary

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-recover-gate`.

## Outcome

`Driver::recover` takes `accept`, the log boundaries the host's open accepts. Once the log is
recovered (and a closed record applied) and before Start, a group whose log retains none of them
(`Core::retains_suffix`) fences with `recover_refused` (a `fenced` carrying the log's base boundary,
last index and membership) and the call fails; nothing that changes consensus state ran (no
election, vote, append or checkpoint, no close record), so the next recover finds the same log. An
empty `accept` is no gate. The exact signature and tests are strata docs/design/journal-binding.md,
Contracts, at the strata-journal-behind branch.

## Acceptance criteria

- A1 Value tests: a lone voter refused at the gate persists no term and appends no NoOp, and the next
  recover finds the same base, last and terms (three retries); the turn log and the host's table stay
  byte-equal for a group recovered from the turn log without pressure; a follower refused at the gate
  grants no vote; an accepted boundary at the base, one inside the log, and an empty `accept` each
  start. A mutant that runs the gate after Start is killed (`--fails-without`).
- A2 The suite passes: the `build-test` check, and CI on every platform.
- A3 [artifact] `docs/DRIVER.md` states the gate and the reason's text; `CHANGELOG.md` names it.

## Contracts

Provides `consensus:recover-gate`; consumed by strata-journal-behind. Detent's lead gets the exact
shape before the contributor starts (consensus is shared; detent:consensus-inline-sync-loop is open
on the driver).

## Decisions

- Owner, 2026-11-25 (PLAN D182, D183): the stale-journal fix is strata's journal binding; this is its
  consensus half. The gate takes a value, not a callback (design review D-8).

## May land early

Nothing: the gate and its tests land together.

## Tasks

<The steps that build the feature, one agent each: `T1 <what> (files; assignment, tier, effort;
after T0)`. Each merges into the quest branch on its own tests.>

## Review

One `durability` review, strong tier at high effort: is anything persisted before the gate on any
path (closed record reopen, bootstrap-only typed init, pre-Start reclaim: design review D-7), and can
a sound log be refused?
