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
   S's cached state free? ──yes──► run on it ──────────────────────────────┐
          │ no                                                             ├─► grew ≥ 64 KiB? ─► collect in slices
          ├─ S ≤ 16 KiB ─► compile a state of its own ─► run on it ────────┘      (then: its own state closed;
          └─ S > 16 KiB ─► wait for the cached state ─► run on it                  the cached one: trim the cache)
 compile of a new S ─► cache insert ─► drop oldest while count > --scripts-cache-size or bytes > 64 MiB
```

1. **One call per state.** A `Processor` keeps its script's source and one cached state. A call takes that state when no
   other call holds it. Otherwise a script of up to 16 KiB compiles a state of its own, closed when the call ends (about a
   millisecond at most: a table constructor, the slowest source to compile, takes about 53 ms a MiB), and a larger
   script's call waits its turn for the cached state, so a name-only request cannot make the reactor compile a large
   script. The allocator meters one state, so it meters one call: each call has the whole cap.
2. **A state keeps no call.** A call that grew its state by 64 KiB or more collects it however it ends (a result, a script
   error, a bound, a cancellation): the cycle in progress, then whole cycles until one frees under 1 KiB, one
   `LUA_GCSTEP` at a time with a checkpoint between steps. Each cycle halves a string table the call grew, so the state
   returns to its settled bytes: the base, the compiled script and the registry. Lua's sweep starts by freeing the run of
   dead objects at the head of its object list in one go (`sweeptolive`), and the newest objects are the call's; so a
   fresh live table goes at the head first, and the sweep frees about 51,000 objects a step. A cancelled request's
   collection runs on in slices of its own. A call that grew its state by less leaves that garbage to Lua's own
   incremental collection or to the next collection; a trivial call (a few KiB) collects nothing.
3. **The cache is capped in bytes.** A thread's cache charges each script its settled bytes, the 64 KiB of garbage calls
   may leave on it, its source (kept to compile states of a call's own) and a foreign script's stored params (kept
   serialized; a local script's params come with each call, so its cache entry keeps none). It drops the least
   recently used scripts while the count passes `--scripts-cache-size` (default 100, `opts_t`) or the bytes pass one
   call's cap (64 MiB), after each insert and after each collection of a cached state. An ordinary script is charged
   about 90 KB (100 of them, 9 MiB), so the count binds first; scripts of several MiB (a body up to the 16 MiB body cap,
   its constants in the state) hit the byte cap.

The node's Lua memory is then at most: script threads x 64 MiB of cached scripts, plus 64 MiB for each call in
flight, plus what is converted into a state before its call's meter starts (`_doc`, `_old_doc`, the params) and
what compiling takes (both unmetered). Calls in flight are bounded by admission (requests per reactor), not by a
Lua-wide budget.

## Why

Per-call accounting inside one shared state needs an owner for every block, and Lua frees blocks during whichever
call's collection step runs, so each block would carry a tag, and the old call's garbage would still need a sweep.
A state per call measures the call by construction. A pool of idle states per script would keep memory at peak
concurrency; a state of the call's own is closed instead. A full `lua_gc(LUA_GCCOLLECT)` at call end would be
simpler but runs to its end on the reactor (tens of milliseconds over a million small objects); the stepped cycle
yields between steps like the call did. Freeing on a worker thread instead would leave the reactor's CPU free, but
needs the host's pool in the Lua layer, which unit tests and the CLI do not have; the sliced sweep is one path
everywhere, and its CPU is the call's own, spent like the call's. Waiting for the cached state serializes only
large scripts' calls: every script waiting would put a schema script's long call ahead of every other write of
that index on the reactor, against the interleaving of calls (D10).

Measured (macOS x86_64, RelWithDebInfo):

1. `lua_mem_seq.py` at the defaults (16 reactors), 200 distinct scripts each leaving 56 MiB of garbage: before, RSS
   reached 1,162 MiB after 20 scripts (job 261116-101116-e8fd26); after, 290 MiB after 200, about 0.6 MiB a script
   in the second half (job 261116-101130-d8dfc8). Each request also makes a new index; `--control`, the same
   requests without the garbage, ends at 65 MiB, +0.2 MiB a script (job 261116-111156-44ae75). Lua keeps about
   24 KB a script, so the remaining 0.4 MiB a script is the allocator keeping freed blocks, not live Lua bytes.
2. A trivial call (`_doc.x = _doc.x * 2`, end to end): 14.6 us before, 14.1 us after: the collection of a small
   state costs nothing measurable.
3. Twelve scripts of 8 MiB: the cache keeps 48 MiB of them, with or without a count bound.

## Where it is tested

`tests/test_lua.cc`: `test_concurrent_calls_of_one_script_each_get_the_whole_byte_cap` (fails on the shared state),
`test_a_call_leaves_no_garbage_in_its_state`, `test_a_call_of_its_own_state_leaves_the_cached_one_as_it_was`,
`test_a_cancelled_call_leaves_no_garbage`, `test_a_threads_cached_scripts_hold_at_most_one_calls_cap`,
`test_the_script_cache_default_is_one_and_fits_the_budget`, `test_a_cached_script_keeps_no_call_params`,
`test_a_call_leaves_its_string_table_as_it_was`, `test_calls_keep_a_threads_cached_scripts_within_the_budget`,
`test_overlapping_calls_of_a_large_script_take_turns`, `test_the_after_call_collection_frees_in_slices`. On a live
node: `harness/lua_mem_seq.py` (the security review's probe: 200 distinct scripts, RSS must level off) and
`harness/lua_mem_same.py` (T10's library-call loops on one reactor); `harness/lua_mem.sh` builds and runs either.

## Limits that remain

A memory refusal condemns the call at the first refused allocation, before Lua's emergency collection could free
the call's own garbage; a call that churns garbage near the cap ends earlier than its live bytes alone would. Letting
the retry through would run an uncharged full collection per refusal. Compilation and the conversion of `_doc`, `_old_doc` and the params into the state are unmetered (X4-N5, N1). On glibc,
freed blocks may stay in the process's heap rather than return to the system, so RSS can sit above the live bound.
