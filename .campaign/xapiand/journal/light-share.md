# light-share: journal

State: waiting on jobs 261119-030739-c88e70 261119-030741-9184b7 261119-030742-53715a (E1a, E1b, E2: the fixed patch with the share always binding)
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

1. Read E1a c88e70 and E1b 9184b7 (`light_share_report.py` and `light_share_turns.py` print in each log): T5 arms
   within the limit, T4 lag, heavy searches at off/4@0/6@0/8@0 on the fixed patch, and heavy turns' CPU/wall under a
   binding share (decides "What bounds T4"). Read E2 53715a: T10 flood p99 and answered at off/4@0/6@0. Fold both into
   `docs/design/light-share.md` and the Evidence below; confirm or change the recommendation.
2. Correctness review F6: its fix is ineffective (the gate never opens under real light traffic; design note "Why the
   fixed share does not bind"). The gate's signal moves to Xapiand (light requests in flight) in light-share-task;
   this quest's cap evidence stands on the always-binding arms. Tell the reviewer before `campaign accept`.
3. Lead: merge master into the branch, `campaign verify light-share` at the head (A5), push.

## Blocked on

Owner: the heavy-share cap. Decision request (also in `docs/design/light-share.md`, "Decision: the cap"):

> **Decision request: the heavy-share cap.** No value is clearly best. On a share that always binds (18 mix arms, 10
> T10 arms, 10 T12 arms; 16 CPUs, 16 loops), only share 4 meets both T5 and T10, it costs the most heavy throughput,
> and no share meets T4.
>
> 1. **Share 4.** T5 in 4 of 4 arms (light p99 6.5 ms vs limit 7.3-9.5; p50 3.42 to 1.42 ms); T10 17.1 ms (bound
>    about 20). Costs under saturation: heavy searches -36% in the mix (-38% heavy only), worst heavy p99 +22% (149 to
>    182 ms), T10 flood throughput -42%, server CPU 267 to 191 s per phase (about 3.6 of 16 cores idle while heavy
>    clients wait). T12 RESTORE and QPS flat. T4 1.92 ms, 1.9x its bound.
> 2. **Share 6.** T10 18.0 ms (met); T5 in 1 of 4 arms (p99 median 19.3 ms, bimodal 7.7-30.3). Costs: heavy searches
>    -25%, flood -17%, heavy p99 +6%. T4 2.05 ms.
> 3. **Share 8.** T5 in 2 of 4 (median 9.0 ms); T10 21.2 ms (misses by 6%). Costs: heavy searches -15%, flood -4%,
>    heavy p99 -9% (136 ms). T4 2.56 ms.
> 4. **No cap.** No cost; T5 33.2 ms (about 4x its limit), T10 46.3 ms, T4 5.12 ms, all missed.
>
> Recommendation: share 4, landed (light-share-task) only with a gate driven by light requests in flight, which
> Xapiand's HTTP server signals, so heavy-only load is never capped and the cost above is paid only while light
> requests are present; T4 goes to its own quest (the long heavy turns, see "What bounds T4"). Share 5 is unmeasured
> and may meet T5 for less. Conditional on E1/E2 reproducing share 4 on the fixed patch.
>
> Deferring costs: x7-gate stays red on T4, T5 and T10; loaded light p99 stays about 33 ms (4x its bound) and flood
> p99 about 46 ms; light-share-task cannot start (it needs the value and the gate's contract). It costs no heavy
> throughput.

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

Per-share tables with light p50/p99/max, T4 lag, heavy searches and p99, restored docs, server CPU, T10 and T12:
`docs/design/light-share.md` "Results". Medians over arms, exclusive jobs.

T4/T5 mix, share always binds (first patch; 135313-0d4d30, 184044-457a2d, 184045-c76864):

| Share | Light p50 / p99 / max, ms | T5 within | T4 lag p99 | Heavy searches | Heavy p99 | Server CPU |
|---|---|---|---|---|---|---|
| off | 3.42 / 33.2 / 106 | 0 of 4 | 5.12 | 30,934 | 149 | 267 s |
| 12 | 3.11 / 31.2 / 93 | 0 of 2 | 4.61 | -0.4% | 150 | 266 s |
| 8 | 2.04 / 9.0 / 53 | 2 of 4 | 2.56 | -15% | 136 | 242 s |
| 6 | 1.74 / 19.3 / 77 | 1 of 4 | 2.05 | -25% | 158 | 219 s |
| 4 | 1.42 / 6.5 / 107 | 4 of 4 | 1.92 | -36% | 182 | 191 s |

Fixed patch, 10 ms window (201525-201177, 201527-d6e961, 201528-06e41d, 201530-ea3869, 201534-abeb06; 28 arms): the
share does not bind. Light p99 off 32.4 / 8 35.1 / 6 43.2 / 4 29.1 ms, 0 arms within T5; heavy searches within 3% of
off; T4 4.1-5.1 ms. Heavy only (201531-b611b8, 201533-91c121): 4 matches off (33,942 vs 33,476 searches, lag 4.10);
4@0 binds (20,592, -38%; lag 1.79 ms; restored 80,000 to 140,000). Per heavy turn, unbound (201534-abeb06): wall
p50/p99 0.25/1.43 ms, CPU p50/p99 0.25/0.72 ms; turns over 1.024 ms are 1.6% and 0.34-0.39 CPU/wall.

T10 flood p99 (answered): first patch off 46.3 (1,553), 12 28.2, 8 21.2 (-4%), 6 18.0 (-17%), 4 17.1 ms (-42%);
fixed 10 ms off 44.0, 8 44.2, 6 45.1, 4 44.0 ms (flat). T12: RESTORE 9,259-9,546 docs/s and QPS 3,993-4,552 at every
share in both patches, inside off's 3,532-4,564.

| Criterion | Evidence |
|---|---|
| A1 | MET: `docs/design/light-share.md`: the wait is CPU on the loops; a binding share fixes T5/T10, not T4; the fixed gate does not bind |
| A2 | NOT MET. T5: share 4 (binding) 4 of 4 arms, prototype only. T4: no share (plateau 1.79-2.05 ms vs 1.024). No landed code (R3) |
| A3 | MET ON THE PROTOTYPE at shares 6 and 4 (binding, 18.0 and 17.1 ms); not at landed code (R3); fixed patch at 10 ms misses (44 ms) |
| A4 | MET ON THE PROTOTYPE: T12 RESTORE and QPS inside off's band at every share, both patches (abe252, 135316-88308e, 184049-f10605); the mix's heavy searches are not A4's measure but cost 36% at share 4 |
| A5 | NOT MET at the head: suite last passed at 37f06f732607 (261118-184002-4c0fa6); since then harness and docs only; master not merged |

Pending: E1a 261119-030739-c88e70, E1b 261119-030741-9184b7 (mix, off/4@0/6@0/8@0, 3 arms each, per-turn wall vs
CPU); E2 261119-030742-53715a (T10, off/4@0/6@0, 2 arms each). Exclusive, at f1154e7b0.

Superseded: 022842-883d3a, 022854-f0e080, 112144-650d76, 112147-f7cdd0, 112151-1f9d36, 112154-12be10 (noisy, timed
out, or ran without build-ls).

## Result

The loaded light wait is CPU on the 16 loops, not the pool or shard turns. A cap on loops running a heavy turn fixes T5
and T10 at share 4 (6.5 ms; 17.1 ms) for 36% of heavy searches and 42% of flood throughput under saturation, T12
flat; 6 fixes T10 only; 12 does nothing. No share reaches T4 (1.79-2.05 ms). The review's F6 gate, as built, never
opens under real light traffic, so the fixed patch at its default window measures as off; its signal belongs in
Xapiand. The long heavy turns are two thirds off-CPU when unbound. Design note: `docs/design/light-share.md`.

## Rulings I made

- R1: the diagnosis uses one patched build and toggles the share by environment (`LS_HEAVY_SHARE` unset is
  today's drain), so arms differ only in the share; A2/A3's before/after run unpatched master against the landed code.
- R2: the diagnosis stands on the exclusive reruns only (135313/135314/135315/135316); every earlier run was noisy,
  invalid or measured nothing.
- R3: A2-A4's before/after at landed code cannot exist before light-share-task lands; this quest's review covers the
  diagnosis, the harnesses and the design note.
- R4: the cap decision stands on always-binding arms (first patch, and the fixed patch's `N@0`), since the fixed
  patch's 10 ms gate does not bind under the mix; the gate's signal is a landing question (light-share-task).
- R5: exclusive jobs are capped at 10 minutes, so E1 is two 6-arm jobs; E2 drops share 8 (it misses T10 already).
