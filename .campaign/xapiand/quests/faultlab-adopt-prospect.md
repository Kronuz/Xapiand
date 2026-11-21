# faultlab-adopt-prospect: prospect's crash tests run on faultlab's faultfs (migration step 2)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-adopt-prospect`.

## Outcome

Migration step 2 of cpp-libraries `docs/design/faultlab.md`: prospect's crash and fault tests use
faultlab's faultfs (`faultlab/faultfs.h`, `kronuz::faultlab`), fetched at a pinned commit, instead
of including strata's `core/faultfs.hh`. The 17 tests and `tools/parity_terms.cc` change include and
namespace only, and every seed keeps the state it rebuilt (faultlab F3), so no test's coverage moves.
After it lands, strata's step 3 can delete `core/faultfs.hh`.

## Acceptance criteria

- A1 The suite passes with faultlab's faultfs, and every crash test runs the same seeds and cut
  points as on main with the same pass counts: the suite job, plus a job that prints each crash
  test's seed and check counts at main and at the branch, identical
- A2 Nothing in prospect includes strata's faultfs: a job running
  `! git grep -n -E 'core/faultfs\.hh|strata::faultfs' -- ':!docs/reviews' ':!docs/status'`
- A3 [ci] CI is green on every platform at the landed commit

## Contracts

Consumes faultlab (faultfs.h, scenario.h) at a pinned commit; changes no prospect API.

## Decisions

- Owner, 2026-11-20: adopt faultlab; the two in-process crash models become one.
- Lead: a seed whose state moves is a finding (faultlab F3), never silently re-baselined.

## May land early

None needed: one landing.

## Tasks

None: one contributor (maintenance, economical, medium).
