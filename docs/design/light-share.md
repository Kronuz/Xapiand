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

Two prototypes, one host (16 CPUs, 16 loops), exclusive holds, arms interleaved within each job and differing only
in the environment. A share that always binds (the first patch, 2b8c66958; and the fixed patch with
`LS_LIGHT_WINDOW_MS=0`, written `N@0`) measures the cap. The fixed patch at its default 10 ms window measures the gate.
Medians over arms; ranges in parentheses. Light is the light query mix (`light.query`); its T5 limit is 2x the arm's
unloaded p99 + 1 ms (7.3-9.5 ms). T4 is the worst HTTP loop's lag p99 (bound 1.024 ms, 0.5 ms histogram buckets).

### T4/T5 mix, share always binds (first patch)

Jobs 261118-135313-0d4d30, 184044-457a2d, 184045-c76864: 18 arms.

| Share (arms) | Light p50 / p99 / max, ms | T5 arms within | T4 lag p99, ms | Heavy searches | Worst heavy p99, ms | Restored docs | Server CPU, s |
|---|---|---|---|---|---|---|---|
| off (4) | 3.42 / 33.2 (17.7-58.2) / 106 | 0 of 4 | 5.12 (4.10-7.17) | 30,934 | 149 | 80,000 | 267 |
| 12 (2) | 3.11 / 31.2 (26.3-36.0) / 93 | 0 of 2 | 4.61 | 30,808 (-0.4%) | 150 | 80,000 | 266 |
| 8 (4) | 2.04 / 9.0 (8.1-16.3) / 53 | 2 of 4 | 2.56 (2.05-2.56) | 26,313 (-15%) | 136 | 100,000 | 242 |
| 6 (4) | 1.74 / 19.3 (7.7-30.3) / 77 | 1 of 4 | 2.05 | 23,068 (-25%) | 158 | 120,000 | 219 |
| 4 (4) | 1.42 / 6.5 (6.1-6.9) / 107 | 4 of 4 | 1.92 (1.79-2.05) | 19,682 (-36%) | 182 | 140,000 | 191 |

Wall versus CPU per turn: not recorded by the first patch (E1 below records it on the fixed patch).

### T4/T5 mix, fixed patch, share always binds (E1)

Jobs 261119-030739-c88e70, 030741-9184b7 (`N@0`, `TURNS=1`): 12 arms. Per heavy turn on the 16 HTTP loops, loaded
phase: wall p99, the share of turns over 1.024 ms, and those long turns' CPU/wall.

| Share (arms) | Light p50 / p99 / max, ms | T5 arms within | T4 lag p99, ms | Heavy searches | Worst heavy p99, ms | Server CPU, s | Heavy turns; wall p99; CPU/wall | Turns over 1.024 ms; their CPU/wall |
|---|---|---|---|---|---|---|---|---|
| off (3) | 3.21 / 34.9 (26.1-35.5) / 100 | 0 of 3 | 3.58 (3.58-5.12) | 32,504 | 137 | 278 | 580,000; 1.14 ms; 0.90 | 1.49%; 0.41 |
| 8@0 (3) | 1.94 / 7.7 (7.0-9.9) / 61 | 2 of 3 | 2.05 (2.05-2.56) | 26,515 (-18%) | 132 | 244 | 427,000; 0.86 ms; 0.96 | 0.61%; 0.76 |
| 6@0 (3) | 1.71 / 12.8 (6.1-23.6) / 81 | 1 of 3 | 2.05 (1.79-2.05) | 23,351 (-28%) | 153 | 220 | 336,000; 0.86 ms; 0.97 | 0.71%; 0.81 |
| 4@0 (3) | 1.40 / 12.4 (6.1-14.8) / 112 | 1 of 3 | 1.79 (1.54-1.79) | 19,830 (-39%) | 183 | 192 | 248,000; 0.72 ms; 0.98 | 0.33%; 0.93 |

Restored docs per phase: 80,000 / 100,000 / 120,000 / 140,000. Both patches pooled (7 arms each, always binding),
T5 is met in off 0, 8 4, 6 2 and 4 5 of 7 arms, light p99 medians 34.9 / 8.4 / 12.8 / 6.5 ms.

### T4/T5 mix, fixed patch, 10 ms light window

Jobs 261118-201525-201177, 201527-d6e961, 201528-06e41d, 201530-ea3869, 201534-abeb06: 28 arms. No arm is
distinguishable from off: the share does not bind (next section).

| Share (arms) | Light p50 / p99 / max, ms | T5 arms within | T4 lag p99, ms | Heavy searches | Worst heavy p99, ms | Server CPU, s |
|---|---|---|---|---|---|---|
| off (8) | 3.32 / 32.4 (11.6-39.7) / 99 | 0 of 8 | 4.10 (3.07-5.12) | 31,630 | 146 | 272 |
| 8 (6) | 3.31 / 35.1 (24.1-45.7) / 144 | 0 of 6 | 4.61 | 31,165 (-1.5%) | 151 | 270 |
| 6 (6) | 3.45 / 43.2 (28.4-95.4) / 139 | 0 of 6 | 5.12 | 30,652 (-3%) | 171 | 262 |
| 4 (8) | 3.36 / 29.1 (23.8-57.2) / 138 | 0 of 8 | 4.61 | 31,143 (-1.5%) | 149 | 268 |

Wall versus CPU per heavy turn (job 201534-abeb06, `harness/light_share_turns.py`, the 16 HTTP loops, loaded phase),
the same at off and 4 (the share did not bind): about 570,000 turns per arm; wall p50 0.25 ms, p99 1.43 ms, p99.9
2.3-2.9 ms; CPU p50 0.25 ms, p99 0.72 ms, p99.9 1.72 ms; CPU/wall 0.87-0.88 overall. The 1.6-1.7% of turns over
1.024 ms wall are 0.34-0.39 CPU/wall: two thirds of a long turn is spent off the CPU. Light handles run outside
`drain()` and have no per-turn record.

### Heavy only (no light clients)

Jobs 261118-201531-b611b8, 201533-91c121, `--phase loaded --rate 0`: 12 arms.

| Arm (arms) | T4 lag p99, ms | Heavy searches | Worst heavy p99, ms | Restored docs | Server CPU, s |
|---|---|---|---|---|---|
| off (4) | 4.10 (4.10-5.12) | 33,476 | 134 | 80,000 | 277 |
| 4, 10 ms window (4) | 4.10 (3.58-5.12) | 33,942 (+1%) | 127 | 80,000 | 288 |
| 4@0 (4) | 1.79 (1.79-1.79) | 20,592 (-38%) | 165 | 140,000 | 193 |

### T10 flood (`lua_bounds.py --loops 4`)

Light GET beside 16 clients flooding the per-call cap; "answered" is the flood's own throughput.

| Patch, share (arms) | Light p50 / p99 / max, ms | Flood answered |
|---|---|---|
| first, off (3) | 0.38 / 46.3 (44.7-53.1) / 50 | 1,553 |
| first, 12 (1) | 0.36 / 28.2 / 32 | 1,488 |
| first, 8 (2) | 0.34 / 21.2 (20.7-21.6) / 29 | 1,489 (-4%) |
| first, 6 (2) | 0.33 / 18.0 (17.7-18.3) / 23 | 1,284 (-17%) |
| first, 4 (2) | 0.35 / 17.1 (17.0-17.1) / 21 | 903 (-42%) |
| fixed `N@0`, off (2) | 0.37 / 46.0 (45.5-46.5) / 48 | 1,601 |
| fixed `N@0`, 6@0 (2) | 0.32 / 17.8 (17.8-17.9) / 25 | 1,291 (-19%) |
| fixed `N@0`, 4@0 (2) | 0.35 / 17.2 (17.0-17.4) / 21 | 946 (-41%) |
| fixed 10 ms, off (2) | 0.32 / 44.0 (41.9-46.0) / 51 | 1,657 |
| fixed 10 ms, 8 (2) | 0.36 / 44.2 (44.2-44.2) / 56 | 1,609 |
| fixed 10 ms, 6 (2) | 0.36 / 45.1 (44.0-46.3) / 51 | 1,609 |
| fixed 10 ms, 4 (2) | 0.35 / 44.0 (43.1-44.9) / 48 | 1,673 |

Jobs: first 261118-135314-eb475b, 135315-63667c, 184047-e27ef6, 184048-415e50; fixed `N@0` 261119-030742-53715a
(E2); fixed 10 ms 261118-201535-ef5844, 201536-4c99e0, 201538-46c66e, 201539-d4e3d3.

### T12 loadtest (heavy throughput, A4)

| Share | RESTORE docs/s, first / fixed | QPS, first / fixed |
|---|---|---|
| off | 9,325 (9,312-9,431) / 9,342 | 4,318 (3,532-4,564) / 4,256 |
| 12 | 9,303 / - | 3,993 / - |
| 8 | 9,366 / 9,464 | 4,176 / 4,414 |
| 6 | 9,259 / 9,488 | 4,146 / 4,552 |
| 4 | 9,420 / 9,546 | 4,249 / 4,314 |

Jobs: first 261118-135316-88308e, 184049-f10605; fixed 261118-201541-abe252 (2 arms per share). Every share is inside
off's own spread (QPS 3,532-4,564, 26%) in both patches: T12 does not separate the arms.

## Reading

1. A share of 12 does nothing measurable. A binding share of 4 or 6 meets T10 on both patches (17.0-17.9 ms); 8
   misses it by 6% (21.2 ms, first patch).
2. T5 is not reliably met by any share. Share 4 met it in 4 of 4 arms on the first patch but 1 of 3 on the fixed
   patch (6.1, 12.4, 14.8 ms against 8-9.3); pooled, 4 meets it in 5 of 7, 8 in 4 of 7, 6 in 2 of 7. The order 8
   above 6 says the arms near the limit are noise-limited (off's own p99 spans 17.7-58.2 ms).
3. The cost of share 4 under saturation: heavy searches -36 to -39% in the mix (-38% heavy only), worst heavy p99
   +22 to +34% (to 182-183 ms), the flood's throughput -41 to -42%, and server CPU 267-278 to 191-192 s per phase:
   about 4 of 16 cores idle while heavy clients wait. Share 8 costs -15 to -18% searches, heavy p99 flat (132-136
   ms), flood -4%, CPU -10%. RESTORE in the mix rises 80,000 to 140,000 docs per phase at 4 (it is not capped and
   takes part of the freed CPU; inferred, not traced). T12 does not move.
4. No share meets T4: 1.54-2.05 ms at shares 4-6, and heavy only at 4@0 is 1.79 ms.
5. The fixed patch's gate never opens under real light traffic, so at its default window the share does not bind:
   heavy searches, CPU and light p99 match off in all 28 mix arms and all T10 arms.

### Why the fixed share does not bind

The gate opens when a light handle reaches `demote()` or `drain()` (`light_seen`). Light request handles resume
through `post()` (task_asio.hh, an `asio::post` straight to the executor), never through the demoted queue, so in the
mix only stray demoted light entries open it (the loaded phase's demoted peak is 4). The probe's `gate` case passed
because it posts light work with `post_demoted`. The F6 fix (no cap without light) therefore also removes the cap
with light; the cap measurements above stand on the always-binding arms. A gate that works needs a signal of light
*requests* in flight, which the HTTP server knows and task does not: internal light posts (timers, the lag probe)
run in heavy-only load too, and a post- or delay-based gate would close heavy-only throughput by 38% with no light
to protect.

## What bounds T4

A share limits how many loops run a heavy turn at once; it cannot shorten a turn that is running. Unbound, the heavy
turns over T4's bound are 0.35-0.43 CPU/wall: preempted on a saturated host. Once a share frees cores they become
computation: at 4@0 they are 0.33% of turns and 0.92-0.94 CPU/wall, at 8@0 0.61-0.87% and 0.72-0.78 (E1). What
remains is un-checkpointed CPU inside heavy turns (resumes after I/O, response serialization), measured per turn,
which a share cannot cut. T4 needs those sections checkpointed (the stall quests), and so does T5's tail: light
p99 at share 4 still spans 6.1-14.8 ms.

## Decision: the cap

**Decision request: the heavy-share cap.** No value is clearly best. On a share that always binds (both patches:
7 mix arms per share, 4 T10 arms each at 4 and 6, 10 T12 arms; 16 CPUs, 16 loops), share 4 meets T10 and meets T5
most often, but not reliably, at the highest heavy cost; no share meets T4.

1. **Share 4.** T10 17.0-17.4 ms (bound about 20). T5 in 5 of 7 arms (first patch 4 of 4, fixed patch 1 of 3; light
   p99 median 6.5 ms, up to 14.8, against limits of 7.3-9.5; p50 3.3 to 1.4 ms). Costs under saturation: heavy
   searches -36 to -39%, worst heavy p99 +22 to +34% (to 183 ms), flood throughput -41 to -42%, server CPU -29 to -31%
   (about 4 of 16 cores idle while heavy clients wait). T12 flat. T4 1.54-2.05 ms.
2. **Share 6.** T10 17.7-18.3 ms (met); T5 in 2 of 7 (median 12.8 ms). Costs: searches -25 to -28%, flood -17 to
   -19%, heavy p99 +6 to +12%. T4 1.79-2.05 ms.
3. **Share 8.** T5 in 4 of 7 (median 8.4 ms); T10 21.2 ms (misses by 6%). Costs: searches -15 to -18%, flood -4%,
   heavy p99 flat (132-136 ms), CPU -9 to -12%. T4 2.05-2.56 ms.
4. **No cap.** No cost; T5 34.9 ms (0 of 7), T10 46 ms, T4 3.6-7.2 ms, all missed.

Recommendation: share 4 behind a light-in-flight gate (Xapiand's HTTP server signals light requests in flight, so
heavy-only load is never capped and the cost is paid only while light requests are present), still, as the only
value that meets T10 and the one that meets T5 most often. E2 confirms its T10; E1 weakens its T5 (1 of 3 on the
fixed patch). Land it with T5 and T4 not yet claimed, and give the residual to the stall quests: E1 measured the
long heavy turns under a binding share as CPU (0.92-0.94 CPU/wall), which only checkpoints remove. If 39% of heavy
searches under saturation is too much, share 8 halves the cost and gives up T10.

Deferring costs: x7-gate stays red on T4, T5 and T10; loaded light p99 stays about 35 ms (4x its limit) and flood
p99 about 46 ms; light-share-task cannot start (it needs the value and the gate's contract). It costs no heavy
throughput.

## Landing

The share is a property of Kronuz/task's scheduler (`task_asio.hh`'s `drain()`): the generic part, a process-wide
count of heavy slots shared by schedulers, belongs there (follow-up quest light-share-task, its own review). Xapiand
sets the slot count in `src/manager.cc`, tells the share when light requests are in flight (the gate's signal), and
bumps the pin. The environment knobs are the prototype's.

Caveats. One 16-CPU host; 2-8 arms per share per patch. The A/A spread of off is 26% on QPS and about 3x on light p99 (11.6 to
39.7 ms across 8 fixed-patch arms), so only effects at shares 8-4 with a binding share exceed it.
