# faultlab-journal-oracle: faultlab's sector test stops fetching journal's memory_io.h (before migration step 8)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-journal-oracle`.

## Outcome

faultlab's `test/sector_test.cc` compares faultfs's sector states against journal's MemoryIO,
fetched as `journal_memory_io` at journal:a412175 (faultlab `CMakeLists.txt`). Migration step 8
deletes `memory_io.h` from journal, so the old model must leave faultlab's build first (Detent's
lead, msg-193): the test keeps checking the same sequences against MemoryIO's states, recorded
once at journal:a412175 into a committed fixture, and faultlab fetches nothing from journal.

## Acceptance criteria

- A1 The sector test checks every sequence it checks on main, each against the state set MemoryIO
  gives at journal:a412175, and passes: the suite job, plus a job at main that regenerates the
  fixture from the live MemoryIO and diffs it against the committed one, identical
- A2 faultlab's default build fetches nothing from journal: a job that configures with default
  options and fails if any `_deps` entry names journal, then runs
  `! git grep -n -E 'journal_memory_io|journal/memory_io' -- ':!docs/reviews' ':!test/fixtures'`
  (the opt-in generator under test/fixtures includes MemoryIO's header by design; lead, 11-21)
- A3 [ci] CI is green on every platform

## Contracts

Changes no faultlab API; test only.

## Decisions

- Lead, 2026-11-20: the parity evidence stays, as a recorded fixture; the deleted model does not
  live on as a pinned dependency.

## May land early

Nothing: one landing.
