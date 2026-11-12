# xapiand-strata-bump: Xapiand pins the strata its prospect pins, past the lazy-segment and barrier changes

Risk: high (durability: the engine under every index). The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status xapiand-strata-bump`.

## Outcome

Xapiand pins strata 5d742b2 while its prospect pin (b20e7c5, x7-loop-stalls) pins strata 94480d5:
pins_check A3 and the CMakeLists strata comment no longer hold. Xapiand moves to the strata main that
carries cold-tier-step2 (lazy segments by default, strata:6d4e1164), the fuzz_postings memory fix
(strata:71750940) and barrier-warning-strata (completion:e49f288), with prospect at the commit that
pins the same strata (after prospect-completion-opens). Xapiand's own IO wrappers forward
completion's four read-only opens (hunt row). Damaged-segment behaviour now surfaces at first read,
not at open: Xapiand's error paths and tests follow.

## Acceptance criteria

- A1 Xapiand's strata and prospect pins agree with prospect's own strata pin: pins_check in the build-test check
- A2 The suite passes at the new pins, with Xapiand's IO wrappers forwarding the read-only opens: the build-test check
- A3 A damaged segment is reported to the client as corrupt, and repairing it clears the error on a later request; nodes open eagerly (`defer_io` forces eager segments, strata index.hh:603; `verify_fields()` maps every segment), so the error surfaces at open: a test, with a `--fails-without` job
- A5 After sync-at-first-commit, a reopened shard's `committed_revision` (shard.cc:255) and the WAL header and replay keyed on it (wal.cc:83,161) stay correct when the open no longer syncs: a test reopening after an unsynced tail edit (design review F7)
- A4 Linux CI (ASan+UBSan, TSan) green at the landed commit: CI

## Contracts

Consumes strata's and completion's public APIs.

## Decisions

- Lead, 11-12: from x7-loop-stalls' pin bump (prospect b20e7c5 pins a newer strata) and the hunt rows
  on the strata pin past 71750940 and the IO wrappers.

- Lead, 11-12: A3 restated to where Xapiand surfaces the damage (open, not first read); making nodes lazy is a hunt row, judged on per-open cost.

## May land early

Nothing.
