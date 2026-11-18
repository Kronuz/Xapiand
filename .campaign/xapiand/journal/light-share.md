# light-share: journal

State: waiting on jobs 261118-022831-d00944 261118-022842-883d3a 261118-022854-f0e080
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

## Next

1. Read X1/X2 (`harness/light_share_report.py`); pick the share; T12 (RESTORE, loadtest QPS) at that share.
2. Write `docs/design/light-share.md` (A1): where the wait goes, the share chosen, its numbers.
3. The landing mechanism lives in Kronuz/task (outside the file set): see Blocked on.

## Blocked on

Nothing yet. Expected: the heavy share is a Kronuz/task change (`task_asio.hh`), outside this file set; Xapiand
then sets the share in `src/manager.cc`. The lead decides whether this quest may change Kronuz/task.

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
