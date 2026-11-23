# stox-no-libc: strict-stox parses every double without libc strtod

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status stox-no-libc`.

## Outcome

Hunt item "strtod abort on macOS" (cpp-libraries `docs/fuzz-2026-11.md` at cpp-libraries:ff5eacf): a
209-byte input makes Apple libc's `strtod` SIGABRT. strict-stox:d3eaf32 decides decimal text with
trailing bytes without libc, but the same text with a leading space or `+` (`starts_decimal` sends it
to `STRICT_STOX_STRTOD`) and every `stold` still reach Apple's `strtod`/`strtold`. After this quest no
strict-stox entry point calls libc's `strtod` or `strtold` for any input, on any platform, with the
same results as today on every input libc handles.

## Acceptance criteria

- A1 The suite passes (`build-test`, Debug so asserts run): a job tagged `--for A1`
- A2 The 209-byte artifact with a leading space, with a leading `+`, and through `stold`, returns the
  same value or error as the other entry points, with the `counted_strtod` hook counting zero libc
  calls (a test with values); it fails without the fix (a `--fails-without` job)
- A3 Results equal libc's on every input libc does not abort on: a differential test over the fuzz
  corpus and a random sample of at least 1,000,000 decimal strings, values and errors compared
- A4 The fuzz harness runs 15 CPU-minutes clean on macOS at the head (a job)

## Contracts

No API change. Values, errors and `used` counts stay as documented in `strict_stox.hh`.

## Decisions

- Owner, 2026-11-16: macOS-only libc bug, prod is not macOS; low priority, no security review.
- Lead, 2026-11-23: normal risk; the correctness review may be waived at landing if A2-A4 hold.

## May land early

Nothing: one change.

## Tasks

None: one contributor (implementation, economical, medium).
