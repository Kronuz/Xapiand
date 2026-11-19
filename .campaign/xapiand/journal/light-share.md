# light-share: journal

State: waiting on jobs 261118-184044-457a2d 261118-184045-c76864 261118-184047-e27ef6 261118-184048-415e50 261118-184049-f10605 (sweep; suite 261118-184002-4c0fa6 too)
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

1. Read the sweep (X1c 184044-457a2d and X1d 184045-c76864: T4/T5 at 6, 4, plus off and 8; X2c 184047-e27ef6 and
   X2d 184048-415e50: T10 at 6, 4; X3b 184049-f10605: T12 at off, 6, 4, 8). Write the table off/12/8/6/4 against each
   bound in `docs/design/light-share.md` and the Evidence table.
2. If a share meets A2 and A3 at a stated A4 cost: pick it. If none: say what bounds T4 (loop CPU from heavy turns,
   or something a share cannot reach), for the owner. Then "ready for review".

## Blocked on

Nothing. Lead ruled (2026-11-18): if X1-X3 show the share wins, the Kronuz/task change is its own quest
(light-share-task, repo ../task, own review); this quest then bumps the pin (CMakeLists.txt's pin line, added to
the file set after test-glob lands) and sets the slot count in `src/manager.cc`.

## Reading so far (session 2, master merged at 34710b05b2da, pushed)

The previous jobs did not all finish: X1 done; X2 hit its 10 min bound (exit 124, result invalid, only arms 1-2
ran); X3 never ran (it was `--after` X2). All three ran strict beside 8-15 other agents' light holds (X1's unloaded
query p99 is 441 ms in arm 1 and 1,073 ms in arm 6, both `off`: a 2.4x A/A spread), so none of it is a clean number.

X1 (ms, medians of 2-3 arms; T4 bound 1.024 ms of loop lag, T5 bound 2x unloaded + 1 ms):
off: loaded query p99 1185, search 1285, worst loop lag p99 3.07, heavy searches 9,717.
12: 1193 / 1297 / 2.05 / 7,944 (-18% heavy). 8: 1170 / 1248 / 1.79 / 5,678 (-42% heavy).
The share cuts worst-loop lag p99 by 33-42% (still 1.8-2x the 1.024 ms bound) and leaves light p99 unchanged
(within +-2%, far inside the A/A spread) while costing 18-42% of heavy searches. Not a win on X1.
X2 (T10, 4 loops): flood p99 0.85 ms for `off` and 12 alike (the 48 ms of the gate does not reproduce here); the
only failure is a 140.9 ms max of one light GET in `off` (limit 50), while 12 shows 37 and 266 ms maxima: noise.

Reruns 650d76, f7cdd0, 1f9d36, 12be10 measured nothing: submitted without `--slot`, they ran in a fresh checkout
without `build-ls` (stall_load/lua_bounds tracebacks, "build-ls/bin/xapiand: No such file"); the harnesses hid it
(X1b exited 0 with empty medians). Fixed: `light_share_ab.sh`, `light_share_t12.sh`, the new `light_share_t10.sh` and
`light_share_report.py` exit nonzero on a failed arm, a missing binary or no rows. Resubmitted with `--slot
light-share` (the slot that holds build-ls) at 85d17ff346b2: X1b 261118-135313-0d4d30, X2a 261118-135314-eb475b
(off 12 off), X2b 261118-135315-63667c (8 off 8), X3 261118-135316-88308e. Suite d26d97 passed at 34710b05b2da (A5
evidence; harness-only commits since). Superseded list: X1b 261118-112144-650d76 (off 8 12 off 8 12), X2 261118-112147-f7cdd0
(off 12 off) and 261118-112151-1f9d36 (8 off 8), X3 261118-112154-12be10 (T12: off 12 8 off), suite
261118-112158-d26d97. Provisional verdict from X1: the share does not win; the verdict stands on the reruns.

## Messages

- Lead (2026-11-18, task change and heavy cost): recorded above. "Report heavy throughput and heavy p99 beside light
  p99 at each share": `harness/light_share_report.py` now prints `heavy_p99` (worst heavy kind's loaded p99) beside
  heavy searches; rerun it locally on X1's `$JOB_OUT/ls-*.json` (the job runs the report at its older commit).
  A share that starves heavy work is not a win: the pick weighs heavy searches, heavy p99 and T12 together.

## Evidence

Exclusive jobs, medians (full table in `docs/design/light-share.md`); arms off / share 12 / share 8:

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/light-share.md`: the wait is CPU on the loops, not the pool or shard turns; share 8 chosen |
| A2 | NOT MET (the tool's "met" is only exit 0). X1b 261118-135313-0d4d30 (6 arms): T5 loaded light p99 33.2 / 31.2 / 12.2 ms vs limit 8.6-9.0 (one of two share-8 arms passes); T4 lag p99 5.12 / 4.61 / 2.30 ms vs 1.024. Improved, bounds NOT yet met; before/after at the landed code pending |
| A3 | At the bound only at share 8 (not yet shown to hold). X2a 261118-135314-eb475b (off 12 off) + X2b 261118-135315-63667c (8 off 8): flood p99 46.3 / 28.2 / 21.2 ms vs about 20 (at the bound, 54% down) |
| A4 | X3 261118-135316-88308e: RESTORE 9,318 / 9,303 / 9,328 docs/s (flat); QPS 3,925 / 3,993 / 4,612 (inside or above the A/A band); heavy searches -0.4% / -11%, heavy p99 149 / 150 / 130 ms |
| A5 | suite 261118-112158-d26d97 passed at 34710b05b2da; rerun at the head 261118-184002-4c0fa6; CI: branch pushed |

Earlier jobs (022842-883d3a, 022854-f0e080, 112144-650d76, 112147-f7cdd0, 112151-1f9d36, 112154-12be10) are
superseded: noisy, timed out, or ran without build-ls. The X1 numbers from before are not used.

## Result

The loaded light wait is CPU on the 16 loops, not the pool or shard turns. A process-wide cap of 8 loops running a
heavy turn at once cuts loaded light p99 63%, worst loop lag 55% and T10 flood p99 54%, at 11% of heavy searches and
no RESTORE or QPS cost; a cap of 12 does nothing. T4 (2.2x over) and T5 (12.2 vs 9.0 ms) are not yet at their bounds;
a share between 4 and 8 is the next measurement. The generic part belongs in Kronuz/task (follow-up quest
light-share-task). Design note: `docs/design/light-share.md`.

## Rulings I made

- R1: the diagnosis uses one patched build and toggles the share by environment (`LS_HEAVY_SHARE` unset is
  today's drain), so arms differ only in the share; A2/A3's before/after run unpatched master against the landed code.
- R2: the diagnosis stands on the exclusive reruns only (135313/135314/135315/135316); every earlier run was noisy,
  invalid or measured nothing.
- R3: A2-A4's before/after at landed code cannot exist before light-share-task lands; this quest's review covers the
  diagnosis, the harnesses and the design note.
