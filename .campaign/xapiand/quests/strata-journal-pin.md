# strata-journal-pin: strata builds against journal main (Host::delete_group)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strata-journal-pin`.

## Outcome

strata main pins journal at a commit on journal's main that contains `Host::delete_group`
(journal:131eb2e, landed by journal-group-delete at journal:d8453f6), up from journal:9e11713, 70
commits behind it. strata's callers move to whatever journal's API changed in between: no
shims, no forwarding names. Every strata test passes on the new pin, so index-delete can call
`Host::delete_group` through strata.

## Acceptance criteria

- A1 strata's suite passes at the new pin, configured with `-G Ninja`: the `build-test` check, a job
  tagged `--for A1`
- A2 The group simulator rows (simulation_group_*) pass at the new pin: a job tagged `--for A2`
- A3 [ci] CI is green on every platform at the landed commit
- A4 [artifact] The journal's notable changes between the two pins that strata had to follow
  (renames, behavior) are listed in the journal's Result, with the strata commits that follow them

## Contracts

Consumes journal at its main. Changes no strata API. Quest branches pinned to the old journal
(group-heal, placement, turn-log-parity) take the new pin when they merge main.

## Decisions

- Lead, 2026-11-23: a pin bump, not a feature. index-delete (Xapiand) needs `Host::delete_group`
  through strata; journal-replaces-wal's contributor asked for it (its journal, Blocked on 2).
- Lead, 2026-11-23: if journal's API changed in ways that touch group-heal's open files,
  stop and report before adapting them; group-heal holds those files.

## May land early

Nothing: one change.

## Tasks

None: one contributor (maintenance, economical, medium; strong, high if the API moved).
