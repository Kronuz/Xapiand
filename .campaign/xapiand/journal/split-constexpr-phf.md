# split-constexpr-phf: journal

State: waiting on jobs 261121-130253-74ad7d 261121-130253-14506d 261121-130255-fe1094 261121-130258-05145f 261121-130301-172ca6
Quest: .campaign/xapiand/quests/split-constexpr-phf.md (the lead's; never edit it)

## Doing

Result is "no split": `phf.hh` stays whole. Layout note committed at constexpr-phf:c5efe76.
Evidence jobs queued (ids in State).

## Next

1. Read the jobs; paste `campaign brief`'s Evidence table; set State "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130253-74ad7d (pending) |
| A2 | split (landed tool) 261121-130253-14506d; split-check-sources tool 261121-130255-fe1094 (pending) |
| A3 | 261121-130258-05145f: configures base constexpr-phf:6f97931 and head, diffs `ctest -N` (pending) |
| A4 | `docs/source-layout.md` at constexpr-phf:c5efe76: no new files; names `phf.hh` (1,484 lines) kept whole and why, and the gperf output in bench/ as generated |
| A6 | No new headers. 261121-130301-172ca6 runs selfcontained.sh over the repo's headers (`phf.hh`) at head (pending) |
| A7 | the lead's read |

## Result

No file split. `phf.hh` (1,484 lines) stays whole; `docs/source-layout.md` (new) says why:
1. It is the whole library and its contract is the one file (README: copying `phf.hh` onto the
   include path works; AGENTS.md: the only header). SPLITTING.md keeps such a library single-file
   unless the owner decides otherwise.
2. Its parts are a ~1,000-line prime table with its two accessors (a table: stays whole), one
   class (~340 lines, members share the tables and the lookup formula), and helpers of 10-40
   lines that would join a neighbor anyway.

Open for the owner: the one recognizable cut is the prime table plus `find_prime`/`prime` into its
own header (leaves ~460 lines of code to read), at the cost of the copy-one-file contract.
Recommendation: keep whole.

## Rulings I made

- No split, by SPLITTING.md's single-file-contract rule; the prime-table cut is left to the owner,
  recorded in the layout note, not taken.
- No open branch edits `phf.hh` (`pins-wave` is at master; the quest branch is the only other).
- `bench/tst-benchmark-stop_words-gperf.h` (1,315 lines) is gperf output: exempt.
- A3 is one job that configures both commits and diffs `ctest -N` (the "Test project" path line
  filtered), rather than two jobs diffed by hand.
- A6: with no new headers, the job checks the existing umbrella `phf.hh` instead, which is a
  superset of the criterion.
