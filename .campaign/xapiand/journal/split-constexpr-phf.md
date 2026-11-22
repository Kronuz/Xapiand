# split-constexpr-phf: journal

State: master merged at 58653a7; A2 job reused 261121-130255-fe1094 (same command and tree; ci.yml-only merge); ready for review once lead reruns gate with landed checker
Quest: .campaign/xapiand/quests/split-constexpr-phf.md (the lead's; never edit it)

## Doing

Result is "no split": `phf.hh` stays whole. Layout note committed at constexpr-phf:c5efe76, branch
pushed. Waiting on build-test (A1) and the corrected A3 job; both queued behind a busy machine.

## Next

1. Read the jobs; paste `campaign brief`'s Evidence table; set State "ready for review".

## Blocked on

Nothing. master changed `.github/workflows/ci.yml` since the branch: the lead's `--merge-main` at landing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130253-74ad7d |
| A2 | 261121-130255-fe1094: split-check-sources tool, 0 errors, 0 warnings, 2 notes (the two files over 1,000 lines) |
| A3 | 261121-131354-2a6f84 (done): diffs `ctest -N` test lines at base constexpr-phf:6f97931 and head |
| A4 | `docs/source-layout.md` at constexpr-phf:c5efe76: no new files; names `phf.hh` (1,484 lines) kept whole and why, and the gperf output in bench/ as generated |
| A5 | CI at head after the master merge |
| A6 | 261121-130301-172ca6: no new headers; selfcontained.sh over `phf.hh` at head, 1 ok, 0 failed (c++17) |
| A7 | the lead's read |

Jobs examined:
- 261121-130253-14506d (split gate, landed cpp-libraries tool) failed. All 10 errors and 12
  warnings are lines of `docs/source-layout.md`, a Markdown file that the landed tool reads as
  C++. No C/C++ file changed. The split-check-sources tool (fe1094) skips non-C/C++ files and
  passes. The lead reruns the gate once split-check-sources lands. `campaign verify --for A1`
  tagged this job A1 too, which is why the brief lists it under A1.
- 261121-130258-05145f (A3, first try) failed on my own command. ctest's "could not find
  executable" warnings carry each build directory's path, so the diff never isolated the test
  names. 2a6f84 replaces it and diffs only the `Test #` and `Total Tests` lines.

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
