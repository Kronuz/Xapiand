# light-share: journal

State: waiting on jobs 261118-200446-6e671a (build-ls) then, after it: sweep 201177 d6e961 06e41d ea3869 (T4/T5, 6 interleaved arms per share); heavy-only b611b8 91c121; settle T4 abeb06 (per-turn wall vs CPU); T10 ef5844 4c99e0 46c66e d4e3d3; T12 abe252
Quest: .campaign/xapiand/quests/light-share.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Diagnosis (A1) by prototype. Hypothesis from the record: with lanes on master, a light handle waits at most one
heavy turn on its loop, so what is left is CPU: under the T4/T5 mix all 16 loops are runnable 92-99% of the time
(census E1, `docs/design/loop-scheduling.md` "Thread placement"), and the restore-cap probe found the work pool
~1 of 16 busy (PLAN P1 update), so the pool is not where light waits. Test: cap how many loops may run a heavy
turn at once (a process-wide share, `LS_HEAVY_SHARE`), leaving CPUs for light work, loop wake-ups and the client.

- Prototype: `harness/light_share_task.patch` on Kronuz/task at Xapiand's pin (task_asio.hh `drain()`: a heavy
  entry needs a slot; no slot parks the loop; a finished heavy turn hands its slot to the first parked loop).
- Build: `harness/light_share_build.sh build-ls` (slot `light-share`), job 261118-022831-d00944 (light).
- X1 261118-022842-883d3a (strict): T4/T5 mix (`harness/light_share_ab.sh`), arms off 12 8 8 12 off.
- X2 261118-022854-f0e080 (strict): T10 flood (`lua_bounds.py --loops 4`), arms off 12 8 12 off.
- X3 261118-023246-6d98d7 (strict): T12 loadtest (`harness/light_share_t12.sh`), arms off 12 8 8 12 off.

## Next

Review CHANGES (`.campaign/xapiand/reviews/light-share-correctness.md`), lead's order:
1. F1/F2/F6/F7 are in `harness/light_share_task.patch` (7ef26e7cca32): parked loop serves light entries, heavy head
   asks for a slot first, the share binds only while light waited within LS_LIGHT_WINDOW_MS (10 ms), grant posted
   under the lock, destructor returns a slot. Probes `harness/light_share_probe.sh`: job 31c214 must pass (exit 0),
   job 0b8e42 runs the v1 patch (2b8c66958) and must fail (--fails-without; read its log: parked, bypass, heavyonly
   each FAIL, not a build error). 6e671a rebuilds build-ls with the new patch.
2. After the build: sweep 8/6/4/off, 6 arms each interleaved (order from seed 20261118: `6 off 4 8 8 6`,
   `4 off off 6 8 4`, `8 off 6 4 4 off`, `6 8 4 off 6 8`), 4 jobs of `light_share_ab.sh`; heavy-only A4 row
   (`-- --phase loaded --rate 0`, arms off 4 4@0, x3); T10 and T12 at off/8/6/4; the settle job (`TURNS=1`, arms 4 off,
   `harness/light_share_turns.py`). Then the table with each arm's position, p99 and p99.9, RESTORE; F3-F5, F6.
3. F8 (Debug `Counted` bypasses the share, `src/engine/host_asio.h`) and F9 (demoted admission cap) are
   light-share-task's: noted in the design note, outside this file set.

## Blocked on

Owner decision (does not stop the review): A2's T4 (1.024 ms) is not reachable by any share; the choice is the share.
1. Share 4: T5 in 4 of 4 arms (6.5 ms vs limit 7.3-9.5), T10 17.1 ms (bound 20); costs 36% of heavy searches in
   the saturated mix, worst heavy p99 +22%, RESTORE and QPS flat; T4 still 1.9 ms. Recommended, with T4 as its own
   quest.
2. Share 6: T10 18.0 ms, but T5 in 1 of 4 arms; costs 25% of heavy searches.
3. Share 8: T5 in 2 of 4, T10 21.2 ms (misses); costs 15%.
4. Stay at off: no cost, T4/T5/T10 all missed.
T4 needs the un-checkpointed heavy sections (post-I/O resumes, response serialization) cut to checkpoints (stall
quests), since lag plateaus at 1.8-2.05 ms from share 6 down. That is inferred from the plateau and the census, not
profiled per turn.

## Review fixes: probe evidence

- 261118-200443-31c214 (exit 0, TSan clean): task asio/lanes/deadline_asio tests with the share off, 1, 2; `cap`
  (share 1 and 3: max concurrent heavy 1 and 3, no slot leaked); `parked` ran the light handle while the slot was
  held (F1); `bypass` heavy head ran after 0 of 1000 light turns (F2); `heavyonly` 4 of 4 overlapped with share 2 (F6);
  `gate` exactly 2 overlapped while light waits.
- 261118-200453-0b8e42 (`--fails-without`, v1 patch from 2b8c66958, exit 1 as required): `parked` FAIL (light waited
  until the holder released), `bypass` FAIL (heavy head ran after 1000 of 1000 light turns), `heavyonly` FAIL (2 of 4
  overlapped). The task tests pass under v1 and TSan reports nothing.

## Messages

- Lead (2026-11-18, task change and heavy cost): recorded above. "Report heavy throughput and heavy p99 beside light
  p99 at each share": `harness/light_share_report.py` now prints `heavy_p99` (worst heavy kind's loaded p99) beside
  heavy searches; rerun it locally on X1's `$JOB_OUT/ls-*.json` (the job runs the report at its older commit).
  A share that starves heavy work is not a win: the pick weighs heavy searches, heavy p99 and T12 together.

## Evidence

Exclusive jobs; arms off / 12 / 8 / 6 / 4 (table with bounds in `docs/design/light-share.md`):

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/light-share.md`: the wait is CPU on the loops plus un-checkpointed heavy turns, not the pool or shard turns |
| A2 | NOT MET (the tool's "met" is exit 0 only). X1b 135313-0d4d30, X1c 184044-457a2d, X1d 184045-c76864. T5 loaded light p99 33.2 / 31.2 / 9.1 / 19.3 / 6.5 ms vs limit 7.3-9.5: share 4 meets it in 4 of 4 arms, 8 in 2 of 4, 6 in 1 of 4. T4 lag p99 5.12 / 4.61 / 2.56 / 2.05 / 1.92 ms vs 1.024: no share meets it |
| A3 | X2a 135314-eb475b, X2b 135315-63667c, X2c 184047-e27ef6, X2d 184048-415e50. Flood p99 46.3 / 28.2 / 21.2 / 18.0 / 17.1 ms vs about 20: met at shares 6 and 4, missed at 8 |
| A4 | X3 135316-88308e, X3b 184049-f10605. RESTORE 9,325 / 9,303 / 9,366 / 9,259 / 9,420 docs/s (flat); QPS 4,318 / 3,993 / 4,176 / 4,146 / 4,249 (inside off's 3,532-4,564 spread). Heavy searches per loaded phase 30,935 / 30,808 / 26,313 / 23,069 / 19,683 (0 / -0.4 / -15 / -25 / -36%); heavy p99 149 / 150 / 136 / 158 / 182 ms |
| A5 | suites 261118-112158-d26d97 (34710b05b2da) and 261118-184002-4c0fa6 (37f06f732607) passed; CI: branch pushed |

Earlier jobs (022842-883d3a, 022854-f0e080, 112144-650d76, 112147-f7cdd0, 112151-1f9d36, 112154-12be10) are
superseded: noisy, timed out, or ran without build-ls.

## Result

The loaded light wait is CPU on the 16 loops, not the pool or shard turns. Capping loops that run a heavy turn
fixes T5 and T10 at share 4 (6.5 ms; 17.1 ms) at 36% of heavy searches in the saturated mix; share 6 fixes T10 only
(25%); 12 does nothing. No share reaches T4: lag plateaus at 1.8-2.05 ms, the length of un-checkpointed heavy turns,
which a share cannot shorten. The generic part belongs in Kronuz/task (follow-up quest light-share-task). Design
note: `docs/design/light-share.md`.

## Rulings I made

- R1: the diagnosis uses one patched build and toggles the share by environment (`LS_HEAVY_SHARE` unset is
  today's drain), so arms differ only in the share; A2/A3's before/after run unpatched master against the landed code.
- R2: the diagnosis stands on the exclusive reruns only (135313/135314/135315/135316); every earlier run was noisy,
  invalid or measured nothing.
- R3: A2-A4's before/after at landed code cannot exist before light-share-task lands; this quest's review covers the
  diagnosis, the harnesses and the design note.
