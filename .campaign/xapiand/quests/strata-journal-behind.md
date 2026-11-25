# strata-journal-behind: strata: a group whose journal is behind its index refuses at open

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strata-journal-behind`.

## Outcome
A strata group whose recovered log ends before what its index already applied (P_H > E: the journal put
back from an older copy, a copy of a running data root, a plain-mode power cut on macOS) refuses at open
before anything on disk changes: `group::open` reads E from `Driver::state(id).last` after `recover`, and
`adopt` on recovery's path (not an install's activation) throws `group_fenced` naming P_H and E before
`resume` and the file set's recover. Every file stays byte-equal. Sound pairs (P_b <= P_H <= E) open as
today.

## Acceptance criteria
- A1 Value tests: a group whose log is cut back below its applied head (clean stop, and after a crash)
  refuses with both positions in the message and leaves every index and volume file byte-equal; a sound
  group after a crash replays (P_H, E] and serves every acknowledged write. A mutant without the check
  fails the first test (`--fails-without`).
- A2 The suite passes: the `build-test` check.
- A3 [artifact] `docs/mechanisms/` states the open's three refusals (index older, journal older, journal
  missing) and what each leaves on disk; `CHANGELOG.md` names it.

## Contracts
Consumes `consensus:group-last`; provides `strata:journal-behind`. journal-replaces-wal bumps its strata
and consensus pins to these and confirms with the reviewer's stalejournal probe (261124-220619-fb40f3).

## Decisions
- Lead, 2026-11-24: journal-replaces-wal's durability review round 3, S1, blocking. The check lives in
  strata (one place), not in Xapiand decoding consensus's and strata's formats (the contributor's Blocked
  on 13 rules that out).

## May land early
Nothing: the refusal and its tests land together.

## Tasks

<The steps that build the feature, one agent each: `T1 <what> (files; assignment, tier, effort;
after T0)`. Each merges into the quest branch on its own tests.>

## Review

One `durability` review, strong tier at effort max: is every recovery path (clean stop, crash, closed
record, checkpoint plus tail, an install's activation excluded on purpose) checked before the first write;
can a sound group ever be refused (E read before the log's tail is recovered)?
