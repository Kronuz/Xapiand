# faultlab-adopt-strata: strata's crash tests run on faultlab's faultfs (migration step 3)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-adopt-strata`.

## Outcome

Migration step 3 of cpp-libraries `docs/design/faultlab.md`: strata's tests and benches use
faultlab's faultfs (`faultlab/faultfs.h`, `kronuz::faultlab`), fetched at a pinned commit, and
strata deletes `include/strata/core/faultfs.hh` and its `test/faultfs_test.cc`, so the crash model
has one implementation. The two strata-only members map as faultlab-strata-gaps' table says.

## Acceptance criteria

- A1 The suite passes with faultlab's faultfs, and every crash test runs the same seeds and cut
  points as on main with the same pass counts: the suite job, plus a job that prints each crash
  test's seed and check counts at main and at the branch. A count that moves is explained in the
  journal by a faultlab fix since strata's copy (faultlab:fcbe2a9 and the rest), never re-baselined
  silently
- A2 Nothing in strata includes its own faultfs: a job running
  `! git grep -n -E 'core/faultfs\.hh|strata::faultfs' -- ':!docs/reviews' ':!docs/status'`
- A3 [ci] CI is green on every platform at the landed commit

## Contracts

Consumes faultlab (faultfs.h, scenario.h) at a pinned commit; changes no strata API.

## Decisions

- Owner, 2026-11-20: adopt faultlab; the in-process crash models become one.
- Lead: a seed whose state moves is a finding (faultlab F3), never silently re-baselined.

## May land early

None needed: one landing.
