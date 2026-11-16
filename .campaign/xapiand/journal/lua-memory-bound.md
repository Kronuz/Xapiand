# lua-memory-bound: journal

State: waiting on jobs 261116-101214-ce0a1e 261116-101116-e8fd26 261116-101119-b19ec0 261116-101152-fa5074 261116-101126-2bc205 261116-101130-d8dfc8 261116-101133-813db6 261116-101136-539595 261116-101139-e87a30 261116-101225-c83216
Quest: .campaign/xapiand/quests/lua-memory-bound.md (the lead's; never edit it)

## Doing

All jobs queued behind full light slots (none started as of 09:35). Branch pushed at dd27793e6; CI (ci.yml
43917520640, FreeBSD 43917519747) running. Rows branch not pushed yet: waits for test_lua to pass locally.
Merge tree of HEAD + quest/xapiand/platform-rows is clean (git merge-tree).

## Next

0. Fill the TBD numbers in docs/mechanisms/lua-memory.md from test_lua's prints (state bytes, trivial-call cost).
1. Read the before jobs: A4 test fails on master, lua_mem_seq grows, lua_mem_same answers 400s.
2. After jobs at the head: test_lua (A2, A4), lua_mem_seq (A1), lua_mem_same, TSan test_lua (light).
3. Write docs/mechanisms/lua-memory.md with the measured numbers (state bytes, trivial-call cost before/after).
4. Push the branch; A5: push a rows branch (this branch merged with quest/xapiand/platform-rows) so arm64.yml
   and freebsd.yml run on it; record the run ids.
5. `campaign verify lua-memory-bound` (A6 suite).

## Blocked on

Nothing.

## Design (why this mechanism)

One mechanism, "one call per state, collected after its call":

1. A call runs on its script's cached state when it is free, else on a state of its own (compiled from the
   kept source, closed after the call). One state runs one call at a time, so the allocator's meter is that
   call's bytes: each concurrent call gets the whole 64 MiB (A4).
2. After its call (ok, error, bound or cancelled) the state collects: it finishes the GC cycle in progress
   and runs one whole cycle, a `LUA_GCSTEP` at a time with a budget checkpoint between steps, so 60 MiB of
   small-object garbage does not hold the reactor. The cached state keeps its script, not the call (S1).
3. A thread's cache is also capped in bytes, at one call's cap (64 MiB), counting each cached state's settled
   bytes plus its source, so large scripts cannot fill 100 slots x 16 MiB. Oldest first.

Rejected: per-call accounting in one shared state (a freed block has no owner: Lua frees a block during
another call's GC step, so a per-block header or tag would be needed, plus a sweep of the old call's
garbage anyway); a byte-sized cache alone (leaves A4's sharing); always a fresh state per call (compiling
the base each call, about the cost of a call or more); a pool of idle states per script (memory grows with
peak concurrency, and the cache would have to count and trim states inside a script).

## Evidence

Resubmitted 10:12 after the first round was skipped (the mutant had no inverted exit; `--fails-without`
expects exit 0 when the test fails). Mutant: 261116-101214-ce0a1e. Before (master src 2e30d05c6): A1 e8fd26,
lua_mem_same b19ec0, T10 x2 fa5074. After: test_lua 2bc205 (A2, A4), A1 d8dfc8, lua_mem_same 813db6,
T10 defaults 539595 (A3), T10 x2 e87a30. TSan c83216 (150 min, light). The ids in the table below are stale.

| Criterion | Evidence |
|---|---|
| A1 | before 261116-092038-52142f (master src); after 261116-092016-425ef9 (both queued) |
| A2 | test_the_script_cache_default_is_one_and_fits_the_budget in 261116-091512-ce4041 (queued) |
| A3 | T10 row at defaults 261116-092023-4668af (queued) |
| A4 | fails on master: 261116-091202-d7bb0a (--fails-without); passes: 261116-091512-ce4041; node: lua_mem_same before 261116-092041-9abe61 / after 261116-092020-f5728f; T10 on 2 reactors before 261116-092044-f3b45f / after 261116-092027-f463a6 (all queued) |
| TSan | 261116-091556-27c491 test_lua + fuzz_lua 60 s (queued) |
| A5 | to run |
| A6 | to run |

## Result

<pending>

## Rulings I made

1. The cache's byte budget is one call's cap (`Limits::max_bytes`, 64 MiB a thread), not a new option: the
   idle cache of a thread never holds more than one call may. Node bound on idle states: script threads x
   64 MiB (1 GiB at 16 reactors), whatever scripts clients PUT.
2. `--scripts-cache-size` keeps its meaning (a count) and its default 100, now in one place (`opts_t`); the
   CLI reads it from there (N5).
3. A memory refusal still condemns the call at the first refused allocation (before Lua's emergency GC can
   retry). Letting the retry succeed would give a call its full cap of live bytes under churn, but each
   refusal would run an uncharged, uninterruptible full GC of up to 64 MiB: a reactor stall a script could
   repeat. Out of scope; noted for the lead.
4. Compilation stays unmetered (X4-N5, pre-existing); its bytes count against the cache once settled.
