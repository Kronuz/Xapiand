# metrics-fd-count: Count open descriptors without scanning to the fd limit

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status metrics-fd-count`.

## Outcome

`GET /:metrics` counts the node's open file descriptors by listing them (/dev/fd on macOS, /proc/self/fd on Linux, kinfo_getfile or /dev/fd on FreeBSD) instead of polling every number up to the fd limit, so a scrape no longer holds the loop for 52-56 ms (explore/restore-stall, docs/perf/restore-stall.md; src/manager.cc:1315).

## Acceptance criteria

- A1 The count equals the old scan's count on a node with a known number of open descriptors (open N files, compare); a test fails if the count is off by one: the `build-test` check plus a test named in the journal
- A2 A scrape's loop turn is under 3 ms on macOS (before: 52-56 ms), under `quiet strict`, with the probe from explore/restore-stall: a job
- A3 The docs E2E passes: the `e2e` check

## Decisions

- Lead, 11-16: list, don't scan; a cached gauge from a chore only if listing is still slow.

## May land early

None.
