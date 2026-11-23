# hunt-traceback-freebsd: traceback's dump names the dumping thread on FreeBSD

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-traceback-freebsd`.

## Outcome

Hunt row 404. On FreeBSD CI, `traceback_hooks` fails `test/hooks_test.cc:50`,
`CHECK(dump.find("hooks-main") != std::string::npos)`, while line 49 (`total:2`) passes: the dump
counts both registered threads but does not print the main thread's registered name. It has
failed this way since 2026-11-19 (traceback main, run 44570136588 at traceback:47c5736; again in
run 44958843249 at a6690d9). Linux and macOS pass. Find why the dumping thread's name is missing
on FreeBSD (thread identity, registration lookup, the self-signal, or what the dump prints for the
thread doing the dumping) and fix the library, not the test.

## Acceptance criteria

- A1 The suite passes: the `build-test` check
- A2 The fix has a test that fails without it on a platform the job can run, or, if the defect
  shows only on FreeBSD, the journal names the FreeBSD-only mechanism with file:line and CI's
  FreeBSD job shows the failure before and the pass after
- A3 [ci] CI is green on every platform, FreeBSD included

## Contracts

None: traceback's API and output format stay as they are.

## Decisions

- Lead, 2026-11-21: normal risk (signal-handler code). One correctness review, or a waiver if the
  fix is small with A2's proof.

## May land early

None.
