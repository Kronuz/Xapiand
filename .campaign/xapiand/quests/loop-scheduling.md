# loop-scheduling: HTTP loop threads keep running under the T4/T5 mix

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status loop-scheduling`.

## Outcome

With x7-loop-fix-c's fix (c) in place, Xapiand's HTTP loop threads are not descheduled mid-handler under the T4/T5 mixed load, so x7-gate's T4 (p99 loop lag within the 1,024 us edge on every loop) and T5 (loaded light p99 within its bounds) pass, and RESTORE (T12) stays at pre-7c. Today fix (c) leaves T5 light p99 at 48.3/52.8 ms (D67), and E4 (job 261112-042106-305a1a, docs/perf/x7-loop-stalls.md) shows loop threads at 0.85 cpu/wall and long handlers at 0.45: the loops are runnable but not running. Candidate causes the design must measure, not assume: more runnable threads than CPUs (loops, workers, strata/prospect executors, WAL), loop threads not favoured over workers (macOS QoS, Linux nice or SCHED policy), and long turns (fix 3). The fix applies on Linux and macOS alike (cross-platform).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] A design note names the measured cause (thread census under the T4/T5 mix: runnable threads per CPU, per-class cpu/wall, on Linux and macOS), the chosen mechanism and the files it changes, and is settled by one concurrency design review before code: `docs/design/loop-scheduling.md`
- A2 T4 and T5 pass with fix (c) and this quest together: exclusive jobs of at most 10 minutes each (D59) on a merge of both branches, before and after, honest magnitude
- A3 T12's RESTORE stays at or above pre-7c with both: one exclusive job of at most 10 minutes
- A4 The build-test check passes, with a test for each new scheduling rule that fails without it: jobs

## Contracts

Consumes x7-loop-fix-c's fix (c) (lands right before this quest, D67). No API, format or wire change. Thread counts or priorities that become options are named in the design and default to today's behaviour unless the measurement says otherwise.

## Decisions

- Owner, 11-12: launch the design now rather than after the quota reset.
- Lead, 11-12: the file set is the design note until the design review settles the mechanism; the lead then widens it to the files the design names. Measurement jobs run on a scratch merge of this branch and quest/xapiand/x7-loop-fix-c.
- Lead: x7-loop-fix-c and this quest land back to back (D67).

## May land early

The design note (A1).
