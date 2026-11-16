# ci-quest-branches-prospect: CI runs on campaign quest branches (prospect)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status ci-quest-branches-prospect`.

## Outcome

prospect's `ci.yml` runs on pushes to `quest/**`, so a campaign landing finds CI at its exact commit
(the campaign has `ci=github`). Before this, quest branches got CI only by a manual `workflow_dispatch`.

## Acceptance criteria

- A1 `ci.yml`'s push branches include `quest/**`: the `yaml` check
- A2 [manual] A push of the quest branch starts CI: the lead names the run

## Contracts

None.

## Decisions

- The lead, 2026-11-08: found when e2e-harness had no CI run after its push.

## May land early

None: one line.
