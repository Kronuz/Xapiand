# consensus-c3-rows: C3 rows: a repair at the base with another term is refused; the commit bound has its own row

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-c3-rows`.

## Outcome

Close track-c-c3-gate's review findings F1 and F2 (`.campaign/xapiand/reviews/track-c-c3-gate-correctness.md`): T5's rows cover the base branch of the repair term check (core.h:642, `b.index == base_index_ ? b.term != base_term_`) and the commit bound, each with a mutant in `tools/mutants.sh` that the unit suite kills.

## Acceptance criteria

- A1 After the base is raised (consensus_test.cc:1111), a repair at `{through, term_at(through)+1}` answers `InvalidRepair`; mutant P1 (the base term check removed) fails consensus_test: a test and a `--fails-without` job
- A2 A repair at a boundary in (commit, last_index] is refused, in a row named for the commit bound, and mutant P2 (the commit bound removed) fails that row; a repair while applied < commit answers Busy, since a delivery or batch is outstanding there (contributor finding, 11-18: acceptance in (applied, commit] is unreachable at Core): tests and a `--fails-without` job
- A3 P1 and P2 are in `tools/mutants.sh`, and the build-test check passes

The review's probe is `.scratch/review-track-c-c3-gate-correctness/mutants-extra.sh` in the Xapiand home.

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
