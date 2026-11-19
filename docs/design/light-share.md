# Light share: a cap on heavy turns across loops

> Status: MEASURED PROTOTYPE (quest light-share, A1). The prototype is `harness/light_share_task.patch` on
> Kronuz/task at Xapiand's pin; nothing is built on the quest branch. The landing mechanism is a Kronuz/task
> change (quest light-share-task).

Question. With lanes in place (`docs/design/loop-scheduling.md`) a light handle waits at most one heavy turn on its
own loop, yet T4 (worst HTTP loop lag p99 at most 1,024 us), T5 (loaded light p99 at most 2x unloaded + 1 ms) and
T10 (flood p99 near 20 ms) still miss. Where does the wait go?

## Where the wait goes

Not the work pool (about 1 of 16 workers busy under the mix, PLAN P1 update) and not a shard turn queue (a light
handle waits one heavy turn at most). It is CPU: under the T4/T5 mix all 16 loops are runnable 92-99% of the time
(loop-scheduling "Thread placement", E1), so a loop's wake-up, a light handle and the client wait for a core behind
heavy turns running on the other loops.

Test: cap how many loops may run a heavy turn at once. A loop whose next handle is heavy and finds no slot parks; a
finishing heavy turn hands its slot to the first parked loop. Light handles never take a slot. The cap is the
process-wide `LS_HEAVY_SHARE` (unset is today's drain), read once. 16 CPUs, 16 loops.

## Results

Exclusive, the same build, arms differ only in the share (`harness/light_share_ab.sh`, `light_share_t10.sh`,
`light_share_t12.sh`; jobs 261118-135313-0d4d30, 135314-eb475b, 135315-63667c, 135316-88308e). Medians of 2-3 arms
for `off` (today's drain), a share of 12 and a share of 8.

| Measure (bound) | off | share 12 | share 8 |
|---|---|---|---|
| T5 loaded light query p99 (limit 8.6-9.0 ms) | 33.2 ms (25.7-40.7) | 31.2 (26.3-36.0) | 12.2 (8.1-16.3) |
| T4 worst loop lag p99 (1.024 ms) | 5.12 ms | 4.61 | 2.30 (2.05-2.56) |
| T10 flood p99 (about 20 ms) | 46.3 ms (44.7-53.1) | 28.2 (one arm) | 21.2 (20.7-21.6) |
| Heavy searches per loaded phase | 30,935 | 30,808 (-0.4%) | 27,408 (-11%) |
| Worst heavy kind's loaded p99 | 149 ms | 150 | 130 |
| T12 RESTORE docs/s | 9,318 | 9,303 | 9,328 |
| T12 QPS (A/A band 3,532-4,318) | 3,925 | 3,993 | 4,612 |

Reading.

1. A share of 12 does nothing measurable (inside the A/A spread everywhere).
2. A share of 8 cuts loaded light p99 by 63% (one of two arms meets T5's limit), worst loop lag by 55% (still 2.2x
   T4's bound), and T10's flood p99 by 54% (at the bound, max 26-31 ms). It costs 11% of heavy searches and none of
   RESTORE throughput or QPS; the worst heavy p99 improves.
3. T4 and T5 are not yet at their bounds at 8. The sweep between 4 and 8 is the next measurement, run against the
   landed mechanism (A2/A3's before/after are taken then, unpatched master against the landed code).

## Landing

The share is a property of Kronuz/task's scheduler (`task_asio.hh`'s `drain()`): the generic part, a process-wide
count of heavy slots shared by schedulers, belongs there (follow-up quest light-share-task, its own review). Xapiand
sets the slot count in `src/manager.cc` and bumps the pin. The environment knob is the prototype's; the landed
mechanism takes the count from the manager (no knob).

Caveats. The runs are one build on one 16-CPU host with 2-3 arms each; the A/A spread of `off` is 20% on QPS and
about 1.6x on light p99, so only the share-8 effects above exceed it. `restored` is 100,000 docs per loaded phase at
share 8 against 80,000 at `off` and 12 (not understood; heavy throughput is judged by the rows above).
