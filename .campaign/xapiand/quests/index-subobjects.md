# index-subobjects: strata's basic_index: mechanisms with their own state become member classes (index-restructure's step 2)

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status index-subobjects`.

## Outcome

After index-restructure moved basic_index's member functions out of its class body into one header per
mechanism, the class still holds every mechanism's state as its own data members. Where a mechanism's
state and invariants stand apart, they become a member class basic_index owns, with its own unit
tests: the first candidate, from index-restructure's journal, is a `file_ledger` that takes `files_`,
`on_disk_`, `posted_`, `inflight_` and `unlinked_`; then the turn log's cohort state, the checkpoint
window and replay, each only where it wins. Each extraction is judged on maintenance: fewer members a
reader must hold in mind, and an invariant stated in one place. No size targets, and no behavior
changes.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, plus CI on every platform with sanitizers
- A2 Each extracted subobject has unit tests of its own invariant under `test/index/`, plus a mutant per
  invariant that a named test kills (`--fails-without`; the log shows a complete build, then the named
  test failing)
- A3 No behavior changes: the turn-log, group and simulation suites pass unchanged (`ctest -R
  '^(standalone_turn_log|group_|simulation_)'`), and the journal names, per extraction, which
  invariant moved and where it is now stated
- A4 [artifact] `docs/mechanisms/index-layout.md` says which mechanism lives in which member class and
  why each one stays inside basic_index or leaves it; `docs/source-layout.md` lists any new header

## Contracts

`strata:index`'s public API stays the same. Callers include `strata/core/index.hh` and see the same
names.

## Decisions

- Lead, 2026-11-24: split from index-restructure, whose step 1 (out-of-class definitions, proved by the
  `members` check and an fnsizes diff) lands alone as that quest; this quest is its step 2 and carries
  the review (correctness) that index-restructure's decisions planned to cover both steps. Starts from
  the main step 1 lands on.

## May land early

Each extraction, on its own proof (A1, A2, A3 for it), with the review at the last one.
