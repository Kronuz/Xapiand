# lua-memory-bound: journal

State: waiting on jobs 261116-111128-14c2ac 261116-111131-929a79 261116-111134-c035c3 261116-111137-6970f9 261116-111156-44ae75
Quest: .campaign/xapiand/quests/lua-memory-bound.md (the lead's; never edit it)

## Doing

Round 3 (11:12): TSan HANG investigation and T10 x2 rerun queued.

1. TSan fuzz_lua 60 1 HANGed (alarm 60 s) on the vararg chain (job c83216). Native cost of the same script and the
   TSan cost are now reported by `test_report_the_cost_of_a_vararg_chain`. Comparison: TSan fuzz_lua 60 1 at
   master's src 14c2ac vs head 929a79. The collect loop is now bounded anyway: past 2^16 basic steps it finishes
   with one full collection (fa6136633). Reasoning: the chain is a tail call, so its stack stays small; it ends at
   the work cap (5x10^8 units); fuzz_lua runs it 7 times per input under one 60 s alarm.
2. T10 on 2 reactors (e87a30) failed only in its last phase, "the foreign-script node did not come up". All four
   library-call loops answered 204. Master's lua_bounds.py starts that node on `args.port + 1`, which can be a
   client socket of the flood phase; platform-rows' lua_bounds.py fixes it with free_port(). The master-src
   comparison fa5074 did not run T10 at all (exit 2: 2e30d05c6's lua_mem.sh has no t10 probe). Rerun: 6970f9.
3. A4 at node level does not reproduce on macOS: master on 1 reactor answered [204, 204, 204, 204] three rounds
   (b19ec0). A4's fails-without is the unit mutant ce0a1e.

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

| Criterion | Evidence |
|---|---|
| A1 | before 261116-101116-e8fd26 (master: 1,162 MiB after 20 scripts, FAIL); after 261116-101130-d8dfc8 (290 MiB after 200, second half +62 MiB); control 261116-111156-44ae75 (queued) |
| A2 | test_lua 261116-101126-2bc205 (`test_the_script_cache_default_is_one_and_fits_the_budget`: 100 scripts hold 2,324 KiB, 23,800 bytes each); rerun c035c3 queued |
| A3 | T10 row at defaults 261116-101136-539595 (pass) |
| A4 | fails without: unit mutant 261116-101214-ce0a1e; passes: test_lua 261116-101126-2bc205; node 4 loops on 1 reactor 261116-101133-813db6 (all 204; master b19ec0 also all 204 on macOS); T10 x2 261116-101139-e87a30 (loops 204, foreign-node startup failed, rerun 6970f9) |
| A5 | not run: the rows branch waits for the TSan answer |
| A6 | CI 43917520640 and FreeBSD 43917519747 green at dd27793e6; `campaign verify` not run |
| TSan | 261116-101225-c83216: test_lua 893 checks pass; fuzz_lua HANG on the vararg chain (investigating: 14c2ac master, 929a79 head) |

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
