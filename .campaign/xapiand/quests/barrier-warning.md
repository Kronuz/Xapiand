# barrier-warning: Xapiand logs the barrier warning and has a full-flush switch

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status barrier-warning`.

## Outcome

On storage where the ordering barrier is not a flush (non-Apple storage on macOS), Xapiand logs a
warning naming the volume and the switch, and a switch turns on full-flush ordering; Linux and
FreeBSD are unchanged (owner 2026-10-29, D10; lead 2026-10-30, D16).

## Acceptance criteria

- A1 The warning is logged once per volume with the switch's name: a test in the `build-test` check
- A2 The switch `--full-flush-ordering` (off by default, D10) reaches completion's `PosixIOOptions::full_flush_ordering` on every IO Xapiand builds (`src/engine/strata/shard.cc` acquire and `src/database/pool.cc` resolve) and is documented with the other options: a test and the docs E2E (`e2e` check)
- A3 Xapiand pins the strata commit of `barrier-warning-strata` on strata's main: the `build-test` check
- A4 [artifact] Changelog `[Unreleased]` names the warning and the switch: `docs/src/content/docs/changelog.md`

## Contracts

Consumes `barrier-warning-strata`'s option and callback.

## Decisions

- `--after barrier-warning-strata`. The switch's name is this quest's call; the default stays the
  ordering barrier (D10).

## May land early

None.
