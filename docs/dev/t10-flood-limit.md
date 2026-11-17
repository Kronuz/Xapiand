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

## Proposed limit

Derive it from a control in the same run: time the flood call alone on the same node (median of 9), and fail when the flood p99 passes `FLOOD_P99_CALLS` (6) x that. The ratio is 1.2 to 4.0 in the table (1 reactor, the worst case, is 4.0); 6 leaves headroom over it and still fails a light GET that waits behind more than six stretches, which a starving lane would show. `--max-flood-p99-ms` still overrides with a fixed value. The prototype also adds `--processors` to the node start.

The control does not catch a longer cap (the stretch and the limit grow together); the cap itself is pinned by the per-call cases of the same script (400 within their time limits) and by `test_one_call_cannot_hold_the_loop_past_the_call_cap`.

## Proof

See the commit message and the journal report: a Release node of `Xapiand:33d69428a` on macOS: PASS on 2 reactors (28.1 ms p99) and on 1 reactor (89.1 ms p99; a limit of 4 stretches, 89.2 ms, passed by 0.1 ms, hence 6). FreeBSD runs of the prototype need CI (no local FreeBSD VM of ours is free; the two running are strata's).
