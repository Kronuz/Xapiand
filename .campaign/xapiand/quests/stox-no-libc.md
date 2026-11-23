# stox-no-libc: strict-stox decides every decimal double without libc strtod

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status stox-no-libc`.

## Outcome

Hunt item "strtod abort on macOS" (cpp-libraries `docs/fuzz-2026-11.md` at cpp-libraries:ff5eacf): a
209-byte input makes Apple libc's `strtod` SIGABRT. strict-stox:d3eaf32 decides decimal text with
trailing bytes without libc, but the same text with a leading space or `+` (`starts_decimal` sends it
to `STRICT_STOX_STRTOD`) still reaches Apple's `strtod`. After this quest every decimal text, with
any C-locale white space or sign before it, is decided by `fp_slow` without libc, with the same
results as today on every input libc handles. Hex, inf, nan, non-nearest rounding modes and non-'.'
locales stay with libc. `stold` stays on `strtold` where `long double` is wider than `double`
(x86_64: a separate parser that passes both fixtures); where `long double` has `double`'s format
(`LDBL_MANT_DIG == DBL_MANT_DIG`: Apple arm64, MSVC), `strtold` is `strtod` and aborts on the
artifact (CI run 45251406255, macos-arm64, strict_stox_slow), so `stold` there takes the `stod`
path: the same decisions, the value widened. Lead, 2026-11-23, after the owner asked whether a cap would
do: a re-route through the existing exact path, no platform-specific cap and no new parser.

## Acceptance criteria

- A1 The suite passes (`build-test`, Debug so asserts run): a job tagged `--for A1`
- A2 The 209-byte artifact with a leading space, with a leading `+`, and through `stold`, returns the
  same value or error as the other entry points, with the `counted_strtod` hook counting zero libc
  calls (a test with values); it fails without the fix (a `--fails-without` job)
- A3 Results equal libc's on every input libc does not abort on: a differential test over the fuzz
  corpus and a random sample of at least 1,000,000 decimal strings, values and errors compared
- A4 The fuzz harness runs 15 CPU-minutes clean on macOS at the head (a job)
- A5 [ci] CI is green on every platform, macos-arm64 included: there the suite runs the artifact
  through `stold` with zero libc calls (no local arm64 host; CI is the evidence)
- A6 [artifact] `README.md` and the header's top comment (`strict_stox.hh`, the declined-conversion
  paragraph) say which texts still reach libc after this quest, `stold` by platform

## Contracts

No API change. Values, errors and `used` counts stay as documented in `strict_stox.hh`.

## Decisions

- Owner, 2026-11-16: macOS-only libc bug, prod is not macOS; low priority, no security review.
- Lead, 2026-11-23: normal risk; the correctness review may be waived at landing if A2-A5 hold.
- Lead, 2026-11-23: A5-A6 added after CI's macos-arm64 abort: all local evidence ran on an x86_64
  host, where `strtold` is not `strtod`.

## May land early

Nothing: one change.

## Tasks

None: one contributor (implementation, economical, medium).
