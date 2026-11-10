# docs-site-refresh: Rewrite the public docs site: architecture, features, quick-start, usage, configuration, reference

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status docs-site-refresh`.

## Outcome

<The behavior that exists when this quest lands, in one paragraph.>

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 <criterion>: <evidence>

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Prefixes of the branch that are safe alone: refactors, tests, unused internal APIs, code behind
a disabled switch. "None" when everything waits for the review; `land --upto` refuses then.>
