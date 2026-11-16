> Status: CURRENT (2026-11-16) how a Lua call's memory is bounded per call and how a node's idle Lua memory is bounded (quest lua-memory-bound; x7-gate security S1, X4-N2, N5). The per-call meters themselves are `lua-bounds.md`.

# Lua memory

## What

A call's bytes are capped at `Limits::max_bytes` (64 MiB). Two things made that cap weaker than it reads:

1. Calls of one script on one thread ran as coroutines of one `lua_State` and shared its meter, so where fewer reactors
   than calls ran, two calls holding 36 MiB each failed the 64 MiB cap together (T10's library-call loops answered
   `[204, 204, 400, 400]` on arm64 and FreeBSD).
2. A call's garbage stayed in its script's cached state: 57 MiB per distinct script, about 100 GiB at 100 cached states
   per thread and 16 threads, reached by one client's sequential PUTs.

## How

```
 call of script S on thread T
   S's cached state free? ──yes──► run on it ─────────┐
          │ no                                        ├─► collect (GC steps, checkpoint between) ─► state keeps S only
          └──────────► compile a state of its own ─► run on it ─┘                                  (its own state: closed)
 compile of a new S ─► cache insert ─► drop oldest while count > --scripts-cache-size or bytes > 64 MiB
```

1. **One call per state.** A `Processor` keeps its script's source and one cached state. A call takes that state when no
   other call holds it, and otherwise compiles a state of its own, closed when the call ends. The allocator meters one
   state, so it meters one call: each concurrent call has the whole cap. Compiling a state costs the base chunk and the
   script, paid only by a call that overlaps another call of the same script on the same thread.
2. **A state keeps no call.** After a call ends, however it ends (a result, a script error, a bound, a cancellation), its
   state finishes the collection cycle in progress and runs one whole cycle, one `LUA_GCSTEP` at a time with a budget
   checkpoint between steps, so a large heap of small garbage does not hold the reactor. Nothing the call made is
   reachable then (its thread and environment are unreferenced), so the second cycle frees it all. A cancellation ends
   the slices, not the collection. The state's live bytes afterwards are its "settled" bytes: the base, the compiled
   script, and the registry.
3. **The cache is capped in bytes.** A thread's cache charges each script its settled bytes plus its source (kept to
   compile states of a call's own), and drops the least recently used scripts while the count passes
   `--scripts-cache-size` (default 100, `opts_t`) or the bytes pass one call's cap (64 MiB). An ordinary script settles
   at TBD bytes, so the count binds first; scripts of several MiB (a body up to the 16 MiB body cap, its constants
   in the state) hit the byte cap.

The node's Lua memory is then at most: script threads x 64 MiB of cached scripts, plus 64 MiB for each call in
flight. Calls in flight are bounded by admission (requests per reactor), not by a Lua-wide budget.

## Why

Per-call accounting inside one shared state needs an owner for every block, and Lua frees blocks during whichever
call's collection step runs, so each block would carry a tag, and the old call's garbage would still need a sweep.
A state per call measures the call by construction. A pool of idle states per script would keep memory at peak
concurrency; a state of the call's own is closed instead. A full `lua_gc(LUA_GCCOLLECT)` at call end would be
simpler but runs to its end on the reactor (tens of milliseconds over a million small objects); the stepped cycle
yields between steps like the call did.

Measured: TBD.

## Where it is tested

`tests/test_lua.cc`: `test_concurrent_calls_of_one_script_each_get_the_whole_byte_cap` (fails on the shared state),
`test_a_call_leaves_no_garbage_in_its_state`, `test_a_call_of_its_own_state_leaves_the_cached_one_as_it_was`,
`test_a_cancelled_call_leaves_no_garbage`, `test_a_threads_cached_scripts_hold_at_most_one_calls_cap`,
`test_the_script_cache_default_is_one_and_fits_the_budget`. On a live node: `harness/lua_mem_seq.py` (the security
review's probe: 200 distinct scripts, RSS must level off) and `harness/lua_mem_same.py` (T10's library-call loops on
one reactor); `harness/lua_mem.sh` builds and runs either.

## Limits that remain

A memory refusal condemns the call at the first refused allocation, before Lua's emergency collection could free
the call's own garbage; a call that churns garbage near the cap ends earlier than its live bytes alone would. Letting
the retry through would run an uncharged full collection per refusal. Compilation is unmetered (X4-N5). On glibc,
freed blocks may stay in the process's heap rather than return to the system, so RSS can sit above the live bound.
