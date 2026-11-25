# strata-applied-term: strata: the index records the term of the entry it applied last

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strata-applied-term`.

## Outcome

strata's index records, with its applied position, the term of the log entry at it:
`apply(const transaction&, files::position at, std::uint64_t term)` and `applied_term()`. The manifest
log keeps `applied_term` after `applied_count` (an edit carries it only when it changes); install
adopts it from the source generation; a standalone or logged index writes 0. Exact contract: strata
docs/design/journal-binding.md, Contracts, at the strata-journal-behind branch.

## Acceptance criteria

- A1 Value tests: the term survives commit, reopen, crash recovery of the manifest log, a merge, and
  an install (adopted from the source); a term change writes one scalar, an unchanged term none; an
  old manifest without the scalar opens with term 0. A mutant that drops the scalar on reopen is
  killed (`--fails-without`).
- A2 The suite passes: the `build-test` check, and CI on every platform.
- A3 [artifact] `docs/mechanisms/applied-term.md`; `CHANGELOG.md` names the API and the manifest change.

## Contracts

Provides `strata:applied-term`; consumed by strata-journal-behind. A manifest-log change: formats
stay open until release (PLAN D184 keeps the freeze held until this lands).

## Decisions

- Owner, 2026-11-25 (PLAN D182-D184): part of the journal binding.
- Lead: starts after rocksdb-parity's task 7 lands as an increment (both touch include/strata/core).

## May land early

Nothing.

## Tasks

<The steps that build the feature, one agent each: `T1 <what> (files; assignment, tier, effort;
after T0)`. Each merges into the quest branch on its own tests.>

## Review

One `durability` review, strong tier at high effort: is the term written in the same manifest edit
as the position it names on every path (commit, flush, merge, checkpoint, install), so no crash can
leave a position with another entry's term?
