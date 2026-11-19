# crash-d-rows: harness: crash check prints every wrong row (land explore/crash-d-wrong)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status crash-d-rows`.

## Outcome

harness/engine_crash_check.sh prints every wrong row in scenarios B and D (its listing line and its raw hit), so the next FreeBSD crash-D failure (hunt :194, 1 wrong row of 4501 at a465288dc) says whether it is a torn document or a malformed listing line. The change exists as Xapiand:e4884d358 on explore/crash-d-wrong.

## Acceptance criteria

- A1 The crash check passes at the head (a light job running harness/engine_crash_check.sh scenarios B and D on this host)
- A2 A deliberately injected wrong row (a mutant job) makes the check fail and print that row with its listing line and raw hit
- A3 CI is green on all platforms

## Contracts

None beyond the fixed behavior.

## Decisions

- From hunt.md rows; filed by the lead 2026-11-19.

## May land early

None.
