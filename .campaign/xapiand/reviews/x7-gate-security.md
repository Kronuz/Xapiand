# x7-gate review: security

Commit: Xapiand:8fe4fe51a2e1ef5c5f9bc2db7ee259635a55839a
Verdict: CHANGES
Date: 2026-11-13

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: the branch changes no `src` (`git diff master...8fe4fe51a` touches `harness/`, `docs/` and
`.campaign/` only), so this review asks what a client can reach at master's code that the report
scores, and whether the report states it. Both blocking findings are report gaps: the code that
causes them comes from earlier quests and its fix is a later `src` quest. Each fix is a row or
condition in `docs/gates/phase7c.md` (and road-to-release's carried conditions) citing the probe job
below. Rerunning the job at the fixed commit confirms the stated number.

## Evidence I relied on

- The gate's own jobs, as the report cites them: rows 261113-164955-f95555 and Linux
  261113-165001-16b520 (every T10 bound case answers 4xx), t10 261113-164933-460810 (flood p99
  48.04 ms at 16 clients), a16 261113-191709-809e9e / 261113-164951-e5baa9 (SR3 single-client loop
  stalls), T8 in f95555 (in-flight cap 1,000 per reactor, 503s past it). I did not rerun them.
- The correctness review (accepted) for whether the rows are true. This review covers what they
  leave out.

## What I checked myself

Scripts are in `.scratch/review-x7-gate-security/` (home checkout). Every job ran at 8fe4fe51a, built
in-job (`build.sh`: RelWithDebInfo as the timing jobs, source stamp 8833ba980, the same as the
gate's timing binary), on macOS, 16 CPUs.

1. **T10 flood vs client count** (`lua_flood.sh`, strict, 261113-195245-49a606): `lua_bounds.py
   --flood-clients 16/64/256`. Light GET p99 46.38 / 19.68 / 1.16 ms, max 48.18 / 40.06 / 724.41 ms;
   every flood request 400. The light lane holds: p99 does not grow with the client count, and the
   16-client value repeats the gate's 48 ms.
2. **Wide-search starvation** (`wide.sh` + `wide_starve.py`, strict, 261113-195248-848197): 2,000
   one-shard indexes, default reactors (16) and pool. One light GET client is timed alone and beside
   C clients that loop a wide search, by explicit name list and by `/w*/`.
3. **Lua memory across scripts** (`lua_mem.py`, light, 261113-200534-8ec457): 4 and 24 concurrent
   distinct scripts, each building about 56 MiB.
4. **Lua memory held after ended calls** (`lua_mem_seq.py`; light, 261113-200618-9d59ed and
   261113-200800-995e98): one request at a time, each a distinct script that builds about 56 MiB and
   returns 204. Node RSS every 8 requests, at the defaults, `--processors 2`, and `--scripts-cache-size 1`.
5. Static reading: the Lua entry points (`src/database/handler.cc:477` is the only caller of
   `lua::Processor`; its `Limits` are compile-time constants, `src/lua/processor.h:50-63`), the
   wide-search caps (`src/opts.cc:160-169`, `src/database/handler.cc:1414-1435`,
   `src/server/search_views.cc:2341-2360`), the HTTP service's timeouts (`src/manager.cc:925-955`),
   and the harness diff (`gate7c_rows.sh`, `stall_load.py`, `wide_search_check.py`, `lua_bounds.py`).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | yes | gap | **Lua memory has no node-wide bound below host RAM, and the report does not carry it.** T10 scores the per-state 64 MiB cap. But a state's garbage outlives its call: nothing collects it at call end (no `lua_gc` in `src/lua/processor.cc`). Each thread's LRU keeps 100 states (`SCRIPTS_CACHE_SIZE 100`, `src/opts.cc:45`; `opts.h:70`'s 10 is overridden), so the ceiling is script threads x 100 x 64 MiB, about 100 GiB at 16 reactors. One client reaches it with sequential PUTs of distinct scripts, without concurrency, each answered 204. The X4 security review's X4-N2 sized this at threads x **10** x 64 MiB and asked for it to be "fixed or ticketed before a release exposed to untrusted clients" (`docs/reviews/phase7c-x4-security.md:189-194, 241`). Neither road-to-release nor this report carries it. | 261113-200618-9d59ed: RSS grows 57 MiB per sequential distinct script, 11 to 2,735 MiB over 48, linear. 261113-200800-995e98 (`--processors 2`): defaults still linear at 40 scripts (2,278 MiB); with `--scripts-cache-size 1` it levels off near 450-650 MiB, so this is the cache ceiling, not a leak. 261113-200534-8ec457: 24 concurrent distinct scripts hold +1,366 MiB (57 per call), all still held 2 s after the last answer. | In the report, a T10 line or carried condition: "Lua memory after ended calls: script threads x `--scripts-cache-size` (100) x 64 MiB, 57 MiB held per distinct script measured (9d59ed); X4-N2 open". Carry X4-N2 in road-to-release with this ceiling, for a `src` quest (a full collection at call end, or a byte-sized cache) whose test is `lua_mem_seq.py` leveling off at the defaults. |
| S2 | yes | gap | **One client's concurrent wide searches stall every HTTP loop for seconds, and the report gives only single-client values.** SR3's 49-100 ms (2,500 indexes) and 393 ms (10,000) come from one client searching serially. Nothing on the node bounds concurrent wide searches. The walk gate serializes only pattern walks, so an explicit name list bypasses it. Admission counts a wide search as one request (1,000 in flight per reactor), and each search holds its own batch of `pool / reactors` indexes. The default `--max-search-indexes` is 100,000 (`src/opts.cc:160`), 10x the widest search measured. | 261113-195248-848197, over 2,000 indexes: 16 clients by name list: light GET p99 1.00 ms, loops max under 3 ms (fine). **64 clients by name list: light GET p99 86.89 ms, max 1,953.98 ms; worst loop lag in the 1-2 s histogram bucket (http0, http2, http12)**; searches p50 3,463 ms, all 200. 16 and 64 clients by pattern: most answer 429 (802 of 883; 12,598 of 12,748), yet loops still lag into the 0.5-0.66 s buckets and light GET max is 66.6 / 410 ms. | Report the concurrent value beside SR3: "64 concurrent wide searches over 2,000 indexes: light GET max 1.95 s, loop lag 1-2 s (848197); concurrent wide searches have no node-wide bound". Name concurrency in SR3's follow-up, not only the per-name loop. One option is a node-wide cap on indexes pinned by searches at once, with excess searches waiting their turn like the walk gate's. Its test: `wide_starve.py` at 64 clients keeps light GET p99 near its 16-client value. |
| N1 | no | nit | T10's flood bound holds in client count: light p99 46.4 / 19.7 / 1.2 ms at 16 / 64 / 256 flooding clients, all 400. The 724 ms max at 256 is one sample, possibly the Python client's own GIL with 257 threads. | 261113-195245-49a606 | None needed; the report could add that the bound is per reactor, not per client. |
| N2 | no | nit | Nine or more concurrent pattern searches from one client make other clients' pattern searches answer 429 (`--search-walk-queue` 8). This is the documented, intended trade (bounded walk work); explicit names are unaffected. | 848197 (pattern phases); `src/opts.cc:165-169` | None; keep. |
| N3 | no | nit | Plain HTTP sets no header, idle or inactivity timeout (`src/manager.cc:928-947`; only TLS does). So T8's construction, heads sent and bodies withheld, can hold the 1,000-per-reactor in-flight cap indefinitely, and every other client gets 503. `docs/design/transport-security.md:82` accepts this ("as with plain HTTP today"). | static | Optionally a sentence in T8's row; the design already owns the decision. |
| N4 | no | nit | Harness: no CI workflow changed, nothing downloaded, no secrets, and Python runs with `-I` where it reads job output. T6 writes `memory.high` only in the node's own `systemd-run --user --scope` cgroup. `gate7c_rows.sh` with an empty `OUT` would run `rm -rf "/<row>.d"` and write `/rows.tsv` (both fail harmlessly without root). | `harness/gate7c_rows.sh:28,73` | `OUT=${3:?usage: gate7c_rows.sh REL ASAN OUT}`. |
| N5 | no | nit | The script cache has two defaults: `opts.h:70` says 10 and the CLI's `SCRIPTS_CACHE_SIZE` says 100. X4-N2's ceiling was computed from the wrong one. | `src/opts.h:70`, `src/opts.cc:45` | Part of S1's `src` quest: one default. |

## Not run

- Wide searches at 10,000 or 100,000 indexes under concurrency (S2's 2,000-index result suffices to
  show the shape), and which part of a concurrent wide search holds the loop (lag versus one long
  handler is not attributed).
- Lua memory at 16 reactors past 48 scripts (I stopped well below the about 100 GiB ceiling on a
  shared host), and on Linux (glibc may return freed memory differently; the states holding garbage
  do not depend on the allocator).
- RESTORE streaming bodies (the correctness review and the gate's `t2_stored_asan` and
  `t2_restore_stream_asan` cover the body cap). TLS paths and FreeBSD.
- Linux and FreeBSD reruns of any probe; all probes are macOS x86_64.
