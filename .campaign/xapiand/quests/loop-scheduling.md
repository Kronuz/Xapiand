# loop-scheduling: HTTP loop threads keep running under the T4/T5 mix

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status loop-scheduling`.

## Outcome

With x7-loop-fix-c's fix (c) in place, a light request or a loop's lag tick on an HTTP loop no longer waits behind a heavy request's turns on the same loop, so x7-gate's T4 (p99 loop lag within the 1,024 us edge on every loop) and T5 (loaded light p99 within its bounds) pass, and RESTORE (T12) stays at pre-7c. D79: the census (docs/design/loop-scheduling.md, E1/E3) shows thread placement cannot get there; the mechanism is inside the loop: light resumptions and timers ahead of heavy turns, or admission of at most N heavy requests per loop, or both, chosen by measurement. It changes phase7c.md D2's contract (every resumption in the ordinary FIFO; no attained-service counter), and the design says what replaces it and how a request is classed light or heavy without a user-visible knob unless the numbers need one. Linux and macOS alike.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] A design note keeps the census (E1/E3, and E2's per-core ABBA as the parked option), names the in-loop mechanism, the classing of light and heavy, the replacement for phase7c.md D2's FIFO contract, starvation bounds for heavy requests, and the files it changes, settled by one concurrency design review before code: `docs/design/loop-scheduling.md`
- A2 T4 and T5 with fix (c) and this quest together are measured on macOS and the Linux VM and recorded, a miss stated as a miss (D80): exclusive jobs of at most 10 minutes each (D59) on a merge of both branches, before and after, honest magnitude
- A3 T12's RESTORE stays at or above pre-7c, and heavy-search throughput is reported against base: one exclusive job of at most 10 minutes
- A4 The build-test check passes, with a test for each ordering or admission rule that fails without it (a light resumption overtakes heavy turns; a heavy request still progresses within its bound): jobs

## Contracts

Consumes x7-loop-fix-c's fix (c) (lands right before this quest, D67). No API, format or wire change. Thread counts or priorities that become options are named in the design and default to today's behaviour unless the measurement says otherwise.

## Decisions

- Owner, 11-12: launch the design now rather than after the quota reset.
- Lead, 11-12: the file set is the design note until the design review settles the mechanism; the lead then widens it to the files the design names. Measurement jobs run on a scratch merge of this branch and quest/xapiand/x7-loop-fix-c.
- Lead: x7-loop-fix-c and this quest land back to back (D67).
- Owner, 11-12 (D80): land the lane with one loop per physical core; macOS T4/T5 recorded as misses; x7-gate's T4/T5 rerun with the load generator off the server's CPUs.
- Owner, 11-12 (D79): in-loop priority now; one loop per physical core parked until E2 (261112-154306-4d90d5) shows its 0.81x heavy-search cost is worth it.

## May land early

The design note (A1).
