# hunt-faultlab-at-mark: faultlab's suite reaches faultfs's at_mark sync path

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-faultlab-at-mark`.

## Outcome

Hunt row 366, review faultlab-strata-gaps N1. faultlab's suite tests the A3 sync ordering at one
point with the file staged there (test/faultfs_test.cc:366-388). The `at_mark` path and a second
ordering point are untested, though the reviewer's probe showed both correct (job
261120-232249-a2f0a5; probe.cc in faultlab-wt/xapiand/faultlab-strata-gaps/.scratch/xapiand/
review-faultlab-strata-gaps-correctness/). Fold those cases into the suite.

## Acceptance criteria

- A1 The suite passes with the new cases: the `build-test` check
- A2 Each new case fails without what it tests: a mutant of faultfs's at_mark sync path that a new
  case must catch, as a job whose log shows that test failing on a tree that compiles

## Contracts

None: tests only.

## Decisions

- Lead, 2026-11-21: low risk; one contributor.

## May land early

None.
