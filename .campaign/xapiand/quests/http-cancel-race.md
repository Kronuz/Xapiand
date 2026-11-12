# http-cancel-race: A cancel that lands between a readiness wake and its handler stops the read

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status http-cancel-race`.

## Outcome

A reader's cancel is never lost: when the cancel token fires after the readiness `async_wait` completed but before its handler runs, and `readable()` then finds nothing (`EAGAIN`, a spurious wake), the handler sees the cancel and ends the read instead of re-arming and waiting past its deadline or shutdown (perf-watch-peek review N1, `http_asio.h:612-617`). `resume_waiter()` uses task's inline-resume bound instead of a second copy of the rule, or names the dependency (N5). The watch-peek note's A/A column reads in the same direction as after/before (N2), and its series rotate per round (N4).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 A test forces the window (cancel requested after the wait completes, before the handler; the read finds `EAGAIN`) and shows the read ends within its deadline: the suite, plus a `--fails-without` job where the fix removed makes that test fail on its own check
- A2 The suite passes, and CI is green on every platform: the build-test job
- A3 [artifact] `docs/perf/watch-peek.md` prints A/A in the after/before direction and states the per-round rotation: `docs/perf/watch-peek.md`

## Contracts

Consumes task (`task_asio.hh` resume helper) read-only. No API change to http's reader.

## Decisions

Lead: a flag the re-wait checks is the reviewer's suggested fix; any equivalent that closes the window is fine.

## May land early

None.
