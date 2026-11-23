# hunt-soundex-one-definition: soundex tables defined once, not per including TU

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-soundex-one-definition`.

## Outcome

Hunt row (hunt-header-tables, Xapiand:073d466 docs/dev/header-tables.md, "Library rows"):
german_soundex.h, french_soundex.h and spanish_soundex.h define 7 `static const` maps and vectors in
the headers. Each of Xapiand's 4 including TUs carries a ~12.5 KB static initializer and a heap copy
of every table (sub_I 12,448-12,912 B per TU, Release+LTO macOS). Give each table one definition:
constant tables (constexpr arrays, sorted for lookup) where the type allows, else `inline` variables
or a .cc. Results stay identical for every input.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, a job tagged `--for A1`
- A2 Every language's soundex output is unchanged: a test over a word list (at least 10,000 words
  per language, including accented and non-letter input) compares the new code with the old
  header's output, values compared (a job)
- A3 [artifact] The journal's Result gives before and after sizes of one including object file's
  static initializer and the library's per-TU cost (`size -m` or `nm`), at named commits
- A4 [ci] CI is green on every platform

## Contracts

None: no API or behavior change.

## Decisions

- Lead, 2026-11-23: low risk, an exploration batch target; the lead reads the diff, no review.

## May land early

Nothing: one change.

## Tasks

None: one contributor (exploration, economical, medium).
