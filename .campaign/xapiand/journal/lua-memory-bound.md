# lua-memory-bound: journal

State: waiting on jobs 261116-113249-880538 261116-113252-d65b43 (vararg A/B); A5 CI 43940511521 43940511755
Quest: .campaign/xapiand/quests/lua-memory-bound.md (the lead's; never edit it)

## Doing

Round 4 (11:35). The TSan HANG was TSan slowness: fuzz_lua 60 1 passes at master's src (14c2ac) and at the head
(929a79). Under TSan the vararg chain to the work cap takes 6,525 ms alone and 19,828 ms for three interleaved, and
fuzz_lua runs it 7 times under one 60 s alarm. Plain build: 253 / 838 ms (c035c3).
Is the head slower than master on that work-capped path? Strict A/B, test_lua x5, each printing the vararg and
trivial-call times: master's src plus only the timing test (local branch probe/lua-memory-bound-master-vararg,
449030e0c) 880538 vs head d65b43.
A5: rows branch quest/xapiand/lua-memory-bound-rows = merge of this branch (6d024683d) and platform-rows
(978b707a3) at 38c81f547, pushed; the push ran arm64 43940511521, FreeBSD 43940511755, CI 43940511723.
Dropped: the local 2-reactor T10 row (6970f9 and e87a30 failed at master lua_bounds.py's port+1 foreign node,
fixed on platform-rows; its loops answered 204). A4 at node level does not reproduce on macOS (master b19ec0:
all 204); the unit mutant ce0a1e is A4's fails-without.

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
| A1 | before 261116-101116-e8fd26 (master: 1,162 MiB after 20 scripts, FAIL); after 261116-101130-d8dfc8 (200 scripts: 290 MiB, +62 MiB over the second half); control 261116-111156-44ae75 (same requests, no garbage: 65 MiB, +20 MiB). Lua keeps 24 KB a script (A2's test); the remaining ~0.4 MiB a script over the control is the allocator holding freed 1 MiB blocks, not live Lua bytes |
| A2 | test_lua 261116-111134-c035c3, 897 checks (`test_the_script_cache_default_is_one_and_fits_the_budget`: 100 scripts hold 2,324 KiB, 23,800 bytes each) |
| A3 | T10 row at defaults 261116-101136-539595 (pass) |
| A4 | fails without: unit mutant 261116-101214-ce0a1e; passes: test_lua 261116-111134-c035c3; node, 4 loops on 1 reactor: 261116-101133-813db6 (all 204; master b19ec0 also all 204 on macOS) |
| A5 | rows branch at 38c81f547: arm64 43940511521, FreeBSD 43940511755 (running) |
| A6 | quest branch CI 43917520640 and FreeBSD 43917519747 green at dd27793e6; head 6d024683d pushed; `campaign verify` not run |
| TSan | test_lua passes (c83216, 929a79); fuzz_lua 60 1 passes at master 14c2ac and head 929a79 |
| Cost | trivial call 14.6 us master / 14.1 us head (ce0a1e / 2bc205); vararg chain A/B 880538 / d65b43 (queued) |

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
