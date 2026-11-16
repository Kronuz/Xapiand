# lua-memory-bound: journal

State: round 5, security review fixes (S1-S4, trivial-call cost) at 309df5cf1, master merged (87bc48324); waiting on jobs
Quest: .campaign/xapiand/quests/lua-memory-bound.md (the lead's; never edit it)

## Doing

Round 5 (12:30). Review .campaign/xapiand/reviews/lua-memory-bound-security.md (CHANGES at fa6136633), fixes in
309df5cf1:
- S1: local Processor keeps no params (handler passes them per call; N2 gone); foreign keeps the stored doc's params
  serialized, charged in resident_bytes. Test `test_a_cached_script_keeps_no_call_params`.
- S2: a collect of the cached state trims the thread's cache; collect runs whole cycles until one frees < 1 KiB (each
  halves a grown string table, max 16). Tests `test_a_call_leaves_its_string_table_as_it_was`,
  `test_calls_keep_a_threads_cached_scripts_within_the_budget` (32 x 700k strings).
- S3: overlap of a script > 16 KiB waits (task::async_mutex) for the cached state; <= 16 KiB compiles its own.
  Test `test_overlapping_calls_of_a_large_script_take_turns`.
- S4: collect first sets a fresh live table (registry ref) at the head of allgc, so sweeptolive stops at once and the
  sweep frees ~51k objects a LUA_GCSTEP. Cancel: collect continues in its own 200 us slices (N3, N4).
  Test `test_the_after_call_collection_frees_in_slices` (>= 8 slices after the build; prints the longest).
- Lead's trivial-call finding: collect only when the call grew the state >= 64 KiB; cache charges that allowance.
Jobs: test_lua at head 261116-122901-b5da7b; fails-at-fa mutant (new tests on fa's code, branch
probe/lua-memory-bound-s1s4-at-fa f0a2301ec) 261116-122927-714fb5 (must exit 1 with FAILs in the 5 new tests).
Next: reviewer probe reruns, strict trivial-call timing, rows branch rebuild (A5, FreeBSD flood p99), verify.

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
5. S3: waiting only for scripts over 16 KiB, not for every script. Waiting always is simpler but serializes every
   script's calls on a reactor (D10's interleaving; `test_the_calls_really_interleaved`), so one long call of a
   schema script would hold every other write of that index on its reactor. Up to 16 KiB a compile is about 1 ms at
   worst (table constructor: 53 ms/MiB, R5), above that the stall grows with the script, so those calls queue.
6. S4: sliced on the reactor (a live table at the head of allgc), not off-loop. The lgc.c patch (reviewer's (a)) is
   outside the file set; off-loop needs the host's WorkPool inside src/lua (unit tests and the CLI have none, so
   tests and the reviewer's probes would exercise a different path). The sweep's CPU is the call's own, sliced like
   the call. Revisit off-loop if FreeBSD's flood p99 stays over 100 ms and master's does not.
7. The after-call collect runs only when a call grew its state >= 64 KiB (lead, trivial call 7.6 -> 12.3 us). 64 KiB
   is 1/1024 of the cap, so leftover garbage takes at most that from the next call; the cache charges it, keeping 100
   ordinary scripts under 9 MiB.
