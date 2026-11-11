# stored-frame-bound: A stored frame's records end where the next frame starts

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status stored-frame-bound`.

## Outcome

<The behavior that exists when this quest lands, in one paragraph.>

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job that passed at the landed code, normally the suite; tag one `[artifact]` (a committed
`path`), `[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only
when a job cannot show it.

- A1 <criterion>: <evidence>

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
