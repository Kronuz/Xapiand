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
- A2 T4 and T5 pass at the fix: one exclusive job of at most 10 minutes, x7-gate's procedure (D59)
- A3 T10's flood p99 and T12's RESTORE meet their targets at the fix: one exclusive job each
- A4 The fix's tests pass (the build-test check), with a test for any new yield or handoff that fails without it: jobs

## Contracts

None yet; A1 names any API the fix changes.

## Decisions

- Lead, 11-11: profiling (A1) runs on master with `--commit master` jobs and no code change; the file set widens to the code the fix needs once A1 names it (src/** is held by restore-mixed-sign-integers until it lands). The offload pool stays gone (7c's design); the fix makes engine work yield or bounds it, it does not bring a pool back.

- Lead, 11-11 (A1 RESTORE half, Xapiand:f1e29b97e): RESTORE's prepare (`finalise_write`, `Schema::index`) leaves the loop as an index chore on the one host `WorkPool` (phase7c.md section 2's mechanism, no new pool or threads); only the apply stays in the shard's turn. phase7c.md's list of chore kinds gains it, with one design review of that amendment before it lands. The record is compressed once, at finalise (fix 2). Parallel apply over the default shards (fix 3) only if 1 and 2 leave T12 short.
## May land early

The profile note (A1).
