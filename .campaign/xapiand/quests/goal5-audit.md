# goal5-audit: Goal 5 audit of completion, journal, consensus, task, wheel: every thread and reclamation path bounded

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status goal5-audit`.

## Outcome

`docs/GOAL5-AUDIT.md` in `consensus` lists, for `completion`, `journal`, `consensus`, `task` and `wheel`, every thread, deferred reclamation and retirement path with its bound, or names a finding filed as a quest of the owning library (Track C A8, from strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007)). `cluster-gate` lands after it (D123).

## Acceptance criteria

- A1 [artifact] `docs/GOAL5-AUDIT.md`: each library at a named commit, each site with its bound or a filed quest
- A2 A grep job listing thread creation and deferred-destruction sites at the audited commits, each matched to an audit row: a job

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
