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

`lua_bounds.py --cost` on a Release build, one reactor, macOS 16 cores, exclusive holds (master `f65ec4854`, the fix at the branch head). One flood call alone took 19.1 ms at master and 21.9 ms at the fix, so the limits are 57.2 and 65.7 ms. The light GET's p50 / p99 in ms beside N clients flooding the per-call cap (jobs afc6c2 and 9ae194):

| clients | master | fix |
|---|---|---|
| 1 | 14.8 / 19.1 | 18.6 / 27.6 |
| 2 | 15.2 / 20.0 | 18.1 / 26.7 |
| 4 | 26.9 / 43.4 | 16.3 / 34.7 |
| 8 | 32.1 / 60.9 | 17.2 / 37.3 |
| 16 | 32.8 / 67.8 | 17.2 / 38.2 |

At master the p99 grows with the clients per reactor and passes its limit at 8 and 16 clients; at the fix the p50 stays at one stretch, the p99 stays flat from 2 clients up, and every row is within the limit. GETs served in the window at 16 clients: 132 at master, 275 at the fix. With one or two flooders the GET waits a little longer at the fix (p99 27.6 against 19.1 ms): it waits for the stretch in progress and for the heavy turn the drainer runs, where at master the one flooder's stretch alone was in the way.

## Cost

A tiny scripted write (`_doc.x = (_doc.x or 0) + 1`, no commit, 2,000 serial writes on a keep-alive connection) alone and beside 8 GET-only clients, one reactor. The fix and master alternate in the order M F F M M F M M, then the two sweep runs (exclusive holds, jobs 016422, 79af24, a10581, e8623e, 739b28, ccb986, 0ea9e1, ea75e8, afc6c2, 9ae194; the master runs include an A/A pair). Median and range over the runs, in ms:

| row | master (6 runs) | fix (4 runs) |
|---|---|---|
| alone, p50 | 0.649 (0.619 to 0.721) | 0.653 (0.641 to 0.686) |
| alone, p99 | 1.40 (1.27 to 1.48) | 1.35 (1.31 to 1.45) |
| beside 8 GET-only clients, p50 | 2.24 (2.20 to 2.49) | 2.25 (2.23 to 2.33) |
| beside 8 GET-only clients, p99 | 4.54 (4.32 to 6.35) | 4.39 (4.16 to 6.00) |

The fix is inside master's own spread on every row: the heavy-lane hop costs a tiny script nothing measurable, since an idle heavy lane runs at once (the drainer's post is the next handler of the same loop). Earlier runs of the same rows under strict holds, beside the suite's build, showed the fix at p99 65 and 73 ms alone; they were contaminated (master's p99 was stable in the same runs), and the exclusive runs do not reproduce them.

## Exploration measurements

The exploration's rows before the fix, kept for the record (hold t10-sweep, 1 reactor, flood clients 1 / 2 / 4 / 8 / 16, one call 22.4 ms): GET p50 0.4 / 20.5 / 20.9 / 44.0 / 44.1 ms, p99 22.5 / 24.3 / 46.7 / 87.4 / 133.5 ms; FreeBSD run 44027025303 (2 reactors, 16 clients): p99 155 ms with one call 21.5 ms alone, 7.2 calls, the limit of 128.8 that the earlier multiple had derived.
