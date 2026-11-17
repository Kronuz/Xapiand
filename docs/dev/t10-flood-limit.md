# T10 flood row: what its p99 limit measures

Status: exploration (explore/t10-flood), not landed.

## Finding

`harness/lua_bounds.py` times a light GET beside 16 clients re-sending a worst-case Lua call, and failed it at p99 over a constant 100 ms. The mechanism is by design: one call is cut at the per-call cap, `Limits::max_call_work` = 1.6 x 10^7 units, but a library call (the quadratic `find`) does not yield inside that stretch (`docs/mechanisms/lua-bounds.md`; one call holds a reactor 19 to 26 ms on the dev host, `docs/status/phase7c-x4-r1.md`). A light GET that lands on a reactor mid-stretch waits for the stretch, so its p99 is about one stretch, and a stretch is as long as the host's cost per unit makes it.

The 100 ms is a constant fixed on the dev host (p99 58 ms, max 88 ms at the time), with no relation to the host running the row. The cost per unit is not constant:

| host | one flood call | light GET p50 / p99 beside the flood | p99 / one call |
|---|---|---|---|
| macOS 16 cores, node on 1 reactor (hold t10-flood-p1) | 22.3 ms alone | 43.2 / 89.1 ms | 4.0 |
| macOS 16 cores, node on 2 reactors (this exploration, hold t10-flood-p2) | 22.6 ms alone | 0.42 / 28.1 ms | 1.2 |
| FreeBSD CI VM, 2 vCPU, run 43960604601 (control) | about 41 ms (294 calls in 6 s on 2 vCPU) | 39.5 / 63.6 ms | about 1.6 |
| the same row, a second VM in the same run | about 54 ms (224 calls in 6 s) | 51.4 / 122.8 ms | about 2.3 |

On the VM the 16 clients and the probe also share 2 vCPUs, and the VM's speed moves between runs by 30% (41 vs 54 ms a call). A fixed 100 ms sits at 2 to 2.5 stretches there, so run-to-run speed decides the verdict. Nothing starves, the reactor count decides: on 2 reactors the light p50 is 0.4 ms and the p99 1.2 stretches; on 1 reactor it is 43 ms and 4.0 stretches, the FreeBSD shape (p50 39 to 51 ms), because the GET queues behind the other flooders' stretches on its reactor. The VM's 2 vCPUs also host the 16 clients and the probe.

## Why a GET waits behind 7 flood calls (FreeBSD run 44027025303: call 21.5 ms alone, p99 155 ms = 7.2 calls)

The flood requests are classed light, so they share the light FIFO with the GET (`docs/design/loop-scheduling.md` D1, D2). A request turns heavy at its 8th spent slice (`REQUEST_LIGHT_SLICES`); a flood request spends one slice, because its one worst-case call is a single unyielded stretch that `checkpoint()` finds past the slice once, however long it ran (22 ms is 110 slices of 200 us, counted as 1). The design's rejected alternative, "accumulated CPU instead of whole slices", assumed every turn ends within a slice; the per-call cap (1.6 x 10^7 units, about 22 ms) lets a call break that. So no flood request ever reaches the heavy lane, and the "one heavy turn per pass" bound never applies: a GET waits behind each flood request queued ahead of it on its reactor, one stretch each. Flooders per reactor is the queue depth.

Measured (hold t10-sweep, 1 reactor, flood clients 1 / 2 / 4 / 8 / 16, one call 22.4 ms per call of throughput; the run shared the machine with 12 light holds): GET p50 0.4 / 20.5 / 20.9 / 44.0 / 44.1 ms, p99 22.5 / 24.3 / 46.7 / 87.4 / 133.5 ms. With 2 clients the GET waits one call, with 16 up to 6; on 2 reactors, 16 clients: p99 122.5 ms (5.4 calls of 22.6 ms). The wait grows with the clients per reactor, so a fixed multiple of one call cannot bound it. The FreeBSD VM (2 reactors, 16 clients, 8 per reactor) reached 7.2 of at most 8 queued ahead; CPU steal widens each stretch but does not explain the count (the macOS runs show the same growth with no steal).

## Limit that follows the rule

`(ceil(clients / reactors) + 1) x one call alone`, the reactor count read from the node's own `xapiand_reactor_lag` loops: 9 x 21.5 = 193 ms for the FreeBSD run (155 passes). This bounds the current scheduler, not the intended one. The scheduler fix is to count an over-slice turn as `ceil(turn / REQUEST_SLICE)` slices (or its overrun against the budget), so a flood request turns heavy after one call and a GET waits for one heavy turn per pass (about 2 stretches); the limit then becomes `3 x one call` with no per-reactor term. That changes D1 of loop-scheduling and is the light-share quest's to decide.

## Proof

Sweep: `.scratch/t10/sweep.py`, log `.scratch/t10/sweep.log`. The limit's code was checked on macOS (Release `Xapiand:33d69428a`, 1 reactor: p99 89 ms against 17 x 22.3 = 379 ms; 2 reactors: 28 ms against 203 ms). FreeBSD needs CI.
