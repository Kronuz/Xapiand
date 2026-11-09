# ci-quest-branches-strata: journal

State: ready for review
Quest: .campaign/xapiand/quests/ci-quest-branches-strata.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261109-003518-12bda2 (`yaml`: `'quest/**'` in ci.yml's push branches) |
| A2 | lead, 2026-11-09: pushing quest/xapiand/ci-quest-branches-strata at strata:446410f started GitHub Actions CI run 42608975510 (event push), with no manual dispatch |

## Result

`.github/workflows/ci.yml` in strata runs on pushes to `quest/**`, so a campaign quest branch gets CI by being pushed.

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
