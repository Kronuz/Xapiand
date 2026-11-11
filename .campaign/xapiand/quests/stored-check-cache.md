# stored-check-cache: strata's stored check decodes with one frame cache

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status stored-check-cache`.

## Outcome

strata's `stored_plugin::check` (include/strata/core/reader.hh:569-571) decodes each document's stored
fields through one `stored_frame` cache, as `records()` beside it does, instead of decoding every
document's whole frame into a fresh buffer. A segment check then allocates about one frame, not one
per document (postings-fuzz-memory: up to 189 MB per input against 4.5 MB live). Repository: Kronuz/strata.

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 `check` on a segment of many documents per frame decodes each frame once: a test in the `build-test` check counts the decodes (or bytes allocated) and fails without the fix
- A2 The rest of strata's suite passes: the `build-test` check

## Contracts

No format or API change: `check` keeps its signature and results.

## Decisions

- Lead: split from postings-fuzz-memory, which moves prospect's strata pin to this fix and runs its A3.

## May land early

None.
