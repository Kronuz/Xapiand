# catalog-post-lifetime: Catalog delivery never posts to a torn-down scheduler

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status catalog-post-lifetime`.

## Outcome

When an acquire's frame is destroyed while the catalog is delivering to it, the catalog never posts
to that frame's scheduler after the frame's owner may have destroyed it. Today `deliver`
(include/strata/core/catalog.hh:1159) marks the waiter `delivered` and then calls `sched.post(h)`
(1162-1164); `abandon` (1211-1220) sees `delivered` and returns without waiting, so a teardown that
destroys the io_context right after leaves the post writing freed memory. Found as a flaky ASan
use-after-free in Xapiand's test_pool (CI run 44672373841); cause and reproducer on Xapiand branch
explore/pool-uaf (Xapiand:7b89dc9f3, note Xapiand:b80ebd83e), reproduced 5/5 by job
261119-210411-ddceb2.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 A strata test reproduces the race deterministically (a hand-driven scheduler holds the post at
  the window; handshakes, no sleeps) and passes at the fix under ASan: its job, plus a fails-without
  job at the unfixed catalog.
- A2 `abandon` never returns while a delivery to that waiter is still posting, and a delivery never
  touches the waiter or its scheduler after `abandon` returned: the A1 test, plus a mutant with the
  wait removed that fails it.
- A3 No new data race or lock-order issue: the catalog tests under TSan (check `tsan`).
- A4 The full suite passes (check `build-test`) and CI is green on every platform.
- A5 [artifact] A short how-it-works note, `docs/mechanisms/catalog-delivery.md`, states the waiter
  states and who may touch what in each.

## Contracts

No API change. Callers may destroy a scheduler once every acquire that uses it has returned or been abandoned; the catalog guarantees no post after `abandon` returns.

## Decisions

Lead: the fix is in strata (the catalog keeps using a scheduler borrowed from a frame after the frame is gone), not in Xapiand's pool or the test. The explorer's proposal, a `posting` state between `suspended` and `delivered` that `abandon` waits through, is a starting point, not a ruling; any design meeting A2 without a lock on the hot path is fine. Xapiand picks up the fix with a strata pin bump that also lands the explore/pool-uaf reproducer in tests/test_pool.cc.

## May land early

The A1 reproducer test alone (behind the fix it is expected to fail, so it lands with the fix). Otherwise none.
