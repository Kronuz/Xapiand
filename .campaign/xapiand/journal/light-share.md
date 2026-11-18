# light-share: journal

State: waiting on jobs 261118-112144-650d76 261118-112147-f7cdd0 261118-112151-1f9d36 261118-112154-12be10 261118-112158-d26d97
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

1. Read X1b/X2/X3 (below) against A2-A4; decide whether the share wins (see "Reading so far").
2. If it wins: write `docs/design/light-share.md` (A1) with the share and numbers, and record here that the generic
   part belongs in Kronuz/task as the follow-up quest light-share-task; then bump the pin and set the slot count in
   `src/manager.cc` (after that quest lands). If it does not win: write A1 as the negative finding and say so.
3. Check suite 261118-112158-d26d97 (A5), then write "ready for review" in State.

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

Rerun, exclusive (waits for the A4 hold): X1b 261118-112144-650d76 (off 8 12 off 8 12), X2 261118-112147-f7cdd0
(off 12 off) and 261118-112151-1f9d36 (8 off 8), X3 261118-112154-12be10 (T12: off 12 8 off), suite
261118-112158-d26d97. Provisional verdict from X1: the share does not win; the verdict stands on the reruns.

## Messages

- Lead (2026-11-18, task change and heavy cost): recorded above. "Report heavy throughput and heavy p99 beside light
  p99 at each share": `harness/light_share_report.py` now prints `heavy_p99` (worst heavy kind's loaded p99) beside
  heavy searches; rerun it locally on X1's `$JOB_OUT/ls-*.json` (the job runs the report at its older commit).
  A share that starves heavy work is not a win: the pick weighs heavy searches, heavy p99 and T12 together.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <pending: docs/design/light-share.md> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |
| A5 | <no evidence yet> |

## Result

<pending>

## Rulings I made

- R1: the diagnosis uses one patched build and toggles the share by environment (`LS_HEAVY_SHARE` unset is
  today's drain), so arms differ only in the share; A2/A3's before/after run unpatched master against the landed code.
