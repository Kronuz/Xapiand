# nightly-retry: journal

State: ready for review
Quest: .campaign/xapiand/quests/nightly-retry.md (the lead's; never edit it)

## Doing

Done. Code: cpp-libraries tools/nightly_dispatch.py, tools/test_nightly_dispatch.py (commit cpp-libraries:d382351, local, not pushed).

## Next

1. Review; push; cpp-libraries CI for the A2 CI half.

## Blocked on

The quest `check` (`python3 -m pytest -q tools`) fails on this machine: no pytest installed (jobs 261119-144244-fe093d, 261119-144255-45f1c8). Not a code failure; the lead owns the check. The unittest run of the same file passes.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | unittest job 261119-144336-a6a4a1 passes (RetryTest: two 502s then success, 4xx fails at once, persistent 5xx gives up after 4 attempts, dispatch 5xx retried); mutant RETRIES=1 job 261119-144343-378e22 exits 1 (fails-without) |
| A2 | unittest 45 tests pass (job 261119-144336-a6a4a1), ruff check job 261119-144300-c04dc6; cpp-libraries CI not run (not pushed) |

## Result

Github.call retries HTTP 5xx, timeouts, connection resets: 4 attempts, 2/4/8 s backoff via the clock. `workflow run` retries only a 5xx reply. Standard library only.

## Rulings I made

- Retry lives in Github.call (one place) over a transport; permanent errors fail at once.
- A timeout or reset on `workflow run` is not retried: a run may have started.
- test_poll_error_keeps_waiting now injects a 404 (a 502 is absorbed by the retry).
