# x7-loop-fix-c: Loops stay under 1 ms under the T4/T5 mix and RESTORE meets pre-7c (fix (c))

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-loop-fix-c`.

## Outcome

x7-gate's loop rows pass at master: every HTTP loop's p99 lag stays within T4's edge under the loaded mix, T5's loaded light p99 meets its bound, and RESTORE (T12) runs at least as fast as pre-7c. At x7-loop-stalls' fix the rows missed (T4 2.56-5.12 ms against 1,024 us; T5 22.7-27.1 ms against bounds of 8.9-10.2 ms; T12 RESTORE 0.57-0.59x pre-7c); its E3 profile puts the remaining loop time in frame decode (23.4%), madvise (13.3%) and aggregation key work (32.9%), and its note designs the parallel apply, fix (c), estimated near 1.3x pre-7c (`docs/perf/x7-loop-stalls.md`). T10's flood row is its own quest (a Lua per-call cap).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] Fix (c)'s design, settled by one concurrency design review before it is built: `docs/perf/x7-loop-stalls.md`
- A2 [artifact] T4 and T5 at the fix are measured and recorded as fix (c)'s column, a miss stated as a miss; the loop-scheduling quest owns passing them: `docs/perf/x7-loop-stalls.md`
- A3 T12's RESTORE meets pre-7c at the fix: one exclusive job of at most 10 minutes
- A4 The fix's tests pass (the build-test check), with a test for each new yield or handoff that fails without it: jobs

## Contracts

Builds on x7-loop-stalls' chores, D1 barriers and D2 schema order (docs/design/phase7c.md D4/D12); keeps them.

## Decisions

- Lead, 11-12: A2 becomes a recorded measurement (job 261112-060352-3a68bf: T4 p99 3.07-6.14 ms vs 1,024 us; T5 light p99 48.3/52.8 ms vs 8.5/9.4, 1.8-2.3x worse than x7-loop-stalls' fix). E4 shows loop threads descheduled mid-handler, which fix (c) cannot address; the loop-scheduling quest owns T4/T5. Whether fix (c) lands with T5 worse goes to the owner with the numbers.
- Lead, 11-12: B1 is closed by code, not argument: a seam before each write in `apply_slice`, a test of the docid-reuse interleaving, and the reviewer's defer-only-the-missing mutant.
- Lead, 11-12: split from x7-loop-stalls when its fix landed; E3's profile is its starting evidence.

## May land early

The design note (A1); any fix step that is a strict improvement on every row, after its correctness review.
