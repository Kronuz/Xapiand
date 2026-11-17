# T10 flood row: what its p99 limit measures

Status: the script call runs in the heavy lane (`docs/design/loop-scheduling.md` D7); the flood row's limit is derived from that rule.

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

## Why the fix is a lane, not an accounting change

Charging an over-slice turn as `ceil(turn / slice)` slices cannot help: a flood request runs its one call in its first light turn and ends, so the charge lands after the stretch. The count hook cannot cut the stretch either. It fires only between VM instructions, and the flood call (`s:find('.-b')`) is one C call of about 3.2 x 10^6 matcher steps with none inside; a library call is yieldable only through `lua_yieldk` with a continuation, and the matcher's `.-` expansion has no restart point. So a call enters the heavy lane before its first turn (D7), and the request returns to the lane it had when the call ends.

## Limit that follows the rule

A light GET waits for the stretch in progress and, at each of its hops through the loop (its read, its database chore), for at most one heavy turn per pass (D3): k = 2 heavy turns beside the one in progress, so `(k + 1) x one flood call timed alone on the node`, with no term in the number of clients or reactors. `harness/lua_bounds.py` states it (`FLOOD_STRETCHES = 2`) in the T10 row and in `--cost --check`.

## Proof

`lua_bounds.py --cost` on a Release build, one reactor, macOS 16 cores, strict holds (master `f65ec4854`, the fix `1036ca25a`; one call alone 20.8 and 22.2 ms, limit 62.5 and 66.5 ms). The light GET's p50 / p99 in ms beside N clients flooding the per-call cap:

| clients | master | fix |
|---|---|---|
| 1 | 17.9 / 22.8 | 18.2 / 26.3 |
| 2 | 17.9 / 22.5 | 18.5 / 28.2 |
| 4 | 33.7 / 47.2 | 17.1 / 45.1 |
| 8 | 38.0 / 68.3 | 17.1 / 36.0 |
| 16 | 39.1 / 85.7 | 17.0 / 38.7 |

At master the p99 grows with the clients per reactor and passes the limit at 8 and 16 clients; at the fix the p50 stays one stretch and every row is within the limit. GETs served in the window at 16 clients: 111 at master, 285 at the fix. The cost of the lane on a scripted write that does nothing is in "Cost" below.

## Cost

A tiny scripted write (`_doc.x = (_doc.x or 0) + 1`, no commit, 2,000 serial writes on a keep-alive connection) alone and beside 8 GET-only clients: COST_TABLE

## Exploration measurements

The exploration's rows before the fix, kept for the record (hold t10-sweep, 1 reactor, flood clients 1 / 2 / 4 / 8 / 16, one call 22.4 ms): GET p50 0.4 / 20.5 / 20.9 / 44.0 / 44.1 ms, p99 22.5 / 24.3 / 46.7 / 87.4 / 133.5 ms; FreeBSD run 44027025303 (2 reactors, 16 clients): p99 155 ms with one call 21.5 ms alone, 7.2 calls, the limit of 128.8 that the earlier multiple had derived.
