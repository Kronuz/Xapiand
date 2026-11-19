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

Exclusive, one build, arms differ only in the share (`harness/light_share_ab.sh`, `light_share_t10.sh`,
`light_share_t12.sh`). T4/T5 rows are medians over all exclusive arms (4 for off, 8 and 4, 6; 2 for 12; jobs
261118-135313-0d4d30, 184044-457a2d, 184045-c76864); T10 over its arms (135314-eb475b, 135315-63667c, 184047-e27ef6,
184048-415e50; 12 has one arm); T12 over 184049-f10605 and 135316-88308e.

| Measure (bound) | off | 12 | 8 | 6 | 4 |
|---|---|---|---|---|---|
| T5 loaded light query p99, ms (limit 7.3-9.5 per arm) | 33.2 | 31.2 | 9.1 | 19.3 | 6.5 |
| T5 arms within limit | 0 of 4 | 0 of 2 | 2 of 4 | 1 of 4 | 4 of 4 |
| T4 worst loop lag p99, ms (1.024) | 5.12 | 4.61 | 2.56 | 2.05 | 1.92 (1.79-2.05) |
| T10 flood p99, ms (about 20) | 46.3 | 28.2 | 21.2 | 18.0 | 17.1 |
| Heavy searches per loaded phase | 30,935 | 30,808 | 26,313 | 23,069 | 19,683 |
| vs off | | -0.4% | -15% | -25% | -36% |
| Worst heavy kind's loaded p99, ms | 149 | 150 | 136 | 158 | 182 |
| T12 RESTORE docs/s | 9,325 | 9,303 | 9,366 | 9,259 | 9,420 |
| T12 QPS (off alone spans 3,532-4,564) | 4,318 | 3,993 | 4,176 | 4,146 | 4,249 |

Reading.

1. A share of 12 does nothing measurable. T5 and T10 improve as the share falls; share 4 meets both (T5 in all 4
   arms, T10 at 17.1-17.7 ms).
2. No share meets T4. Worst loop lag p99 plateaus at 1.8-2.05 ms from share 6 down: shrinking the share from 6 to 4
   costs 15% more heavy searches and moves lag by one histogram bucket.
3. The cost of share 4: 36% of heavy searches in the T4/T5 mix (heavy clients saturate every loop), the worst heavy
   p99 up 22% (149 to 182 ms) and the probe pair p99 52 to 80 ms; RESTORE throughput and QPS are inside off's own
   spread.

What bounds T4. A share limits how many loops run heavy turns at once; it cannot shorten a turn that is running. A
heavy turn that ends at a checkpoint is 0.25-0.35 ms (`loop-scheduling.md`, the lane's wait bounds), a timer is
seen one pass late (two turns, about 0.7 ms), but a resume after I/O, a chore or a body read runs to its next
checkpoint or suspension, and un-checkpointed sections (response serialization) bound a turn as today. The
worst loop's lag p99 of 1.8-2.05 ms, with only 4 of 16 loops allowed a heavy turn and CPU free for the rest, is the
length of those turns plus the light work queued ahead, not CPU. This is inferred from the plateau and the census; it
is not profiled per turn. T4 needs those sections checkpointed (the stall quests own them), not a smaller share.

## Pick

Share 4 is the only one that meets T5 and T10; it leaves T4 at about 1.9x its bound and costs 36% of heavy searches
in the saturated mix. Share 6 meets T10 but T5 in one of four arms. The pick is the owner's: heavy throughput against
T5/T10 bounds (see the journal).

## Landing

The share is a property of Kronuz/task's scheduler (`task_asio.hh`'s `drain()`): the generic part, a process-wide
count of heavy slots shared by schedulers, belongs there (follow-up quest light-share-task, its own review). Xapiand
sets the slot count in `src/manager.cc` and bumps the pin. The environment knob is the prototype's; the landed
mechanism takes the count from the manager (no knob).

Caveats. The runs are one build on one 16-CPU host with 2-4 arms each; the A/A spread of `off` is 30% on QPS and
about 3x on light p99, so only the effects at shares 8-4 above exceed it. `restored` per loaded phase is 80,000 at
`off` and 12, 100,000 at 8, 120,000 at 6 and 140,000 at 4 (not explained).
