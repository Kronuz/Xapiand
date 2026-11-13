# x7-loop-stalls: HTTP loops stay under the 1 ms edge under mixed load, and RESTORE regains pre-7c speed

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-loop-stalls`.

## Outcome

x7-gate's timing rows at Xapiand:df5eb9e06 (20,000 accounts, 3 ABBA pairs against pre-7c 142b2ca77) miss: T4, every HTTP loop's p99 24.6-28.7 ms against the 1,024 us edge (pre-7c under it; job 261111-171041-c9ab68); T5, loaded light p99 76.8 ms against 8.93 ms; T10's 16-client flood p99 50.9 ms against about 20 ms (a82ab4); T12, RESTORE at 0.288x pre-7c (06e893, c39d8d) while QPS is 2.6x. With no offload pool, engine work runs on the loops. This quest finds what holds each loop past the edge and what slows RESTORE, and fixes it so the four rows pass.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] The cause of each miss, from profiles under the T4/T5 mix and under RESTORE at master: which work holds a loop past 1 ms (call stacks, durations, counts) and where RESTORE's time goes against pre-7c: `docs/perf/x7-loop-stalls.md`
- A2 [artifact] T4 and T5 measured at the fix with x7-gate's procedure, misses stated with their magnitude (the targets move to x7-loop-fix-c): `docs/perf/x7-loop-stalls.md`: "At the fix: x7-gate's rows"
- A3 [artifact] T10's flood p99 and T12's RESTORE measured at the fix, misses stated (the targets move to x7-loop-fix-c and a T10 quest): `docs/perf/x7-loop-stalls.md`: "At the fix: x7-gate's rows"
- A4 The fix's tests pass (the build-test check), with a test for any new yield or handoff that fails without it: jobs

## Contracts

None yet; A1 names any API the fix changes.

## Decisions

- Lead, 11-11: profiling (A1) runs on master with `--commit master` jobs and no code change; the file set widens to the code the fix needs once A1 names it (src/** is held by restore-mixed-sign-integers until it lands). The offload pool stays gone (7c's design); the fix makes engine work yield or bounds it, it does not bring a pool back.

- Lead, 11-11 (A1 RESTORE half, Xapiand:f1e29b97e): RESTORE's prepare (`finalise_write`, `Schema::index`) leaves the loop as an index chore on the one host `WorkPool` (phase7c.md section 2's mechanism, no new pool or threads); only the apply stays in the shard's turn. phase7c.md's list of chore kinds gains it, with one design review of that amendment before it lands. The record is compressed once, at finalise (fix 2). Parallel apply over the default shards (fix 3) only if 1 and 2 leave T12 short.
- Lead, 11-12 (A2/A3 at Xapiand:0072ba93e, jobs 261112-012855-b34e3c, 261112-012857-a77bf8): T4
  2.56-5.12 ms (gate 24.6-28.7), T5 22.7-27.1 ms (gate 76.8), T10 45.4 ms (gate 50.9), T12 RESTORE
  0.57-0.59x (gate 0.288x), QPS 2.69x. A2 and A3 stay unmet: the jobs ran, the rows miss. Next: E3
  (261112-020117-4c8576) names the remaining 2.5-5 ms turns; T12's remaining gap is the serial apply
  (2.6 of pre-7c's 5.8-6.1 cores busy): fix (c), per-shard parallel apply, is designed as a section of
  the note and gets one concurrency design review before it is built. T10 is the Lua per-call cap
  (~20 ms, two calls ahead of a GET): its own quest.
- Lead, 11-12: the fix so far (hits checkpoint, compress once, prepare as chores, the prospect pin)
  may land early as an increment after one correctness review of it (concurrency lens questions
  included); it is a strict improvement on every row.
- Lead, 11-12: the tool lands no increment that holds the quest's whole code, so the fix lands as this quest and the A2/A3 targets move to x7-loop-fix-c (fix (c) with one concurrency design review, after E3) and a T10 Lua quest; A2/A3 here record the measured misses.

## May land early

The profile note (A1); the fix at Xapiand:0072ba93e or later as an increment once its review is accepted (Decisions, 11-12).
