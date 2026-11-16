# lua-memory-bound review: security

Commit: Xapiand:fa6136633 (the last code commit on quest/xapiand/lua-memory-bound; 29f56b59a..41214ae7e change
harness, docs and the journal only)
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Attacker: an HTTP client that may PUT documents with `_script` (inline body, `_name`, `_params`, or a
`_foreign` stored script), the x7-gate S1 model. It gains node memory held after its requests end, or reactor
time other clients' requests wait for.

## Evidence I relied on

- test_lua 261116-111134-c035c3 at fa6136633 (passes, 897 checks) and the unit mutant 261116-101214-ce0a1e
  (A4 fails without the change): one state per call gives each concurrent call the whole cap.
- TSan test_lua and fuzz_lua 261116-111131-929a79 at fa6136633: no race (the reactor is single-threaded per
  state; TSan covers the thread-local cache and meters).
- A1's seq probe 261116-101130-d8dfc8 and control 261116-111156-44ae75: the x7 S1 shape (garbage left in
  cached states) levels off. My findings are other routes to the same held memory, not a rerun of S1.

## What I checked myself

Probes are `.scratch/review-lua-memory-bound-security/` in the home checkout. `patch.sh` swaps
tests/test_lua.cc's TEST_MAIN for a probe file, so probes use the suite's helpers; `build_plain.sh`
(RelWithDebInfo, asserts on, the quest slot) and `build_asan.sh` (llvm@22 ASan, own slot and .deps) build
`xapiand_test_lua`; `PROBES=<file>` picks the file. A failing CHECK in a probe is a finding.

1. Q1, what outlives a call (`probes.inc` R0/R1, `probes2.inc` R0; jobs 261116-114930-361614 and
   261116-115429-175f61). Ten scripts park 8 MiB in `_G`, the library proxies, the string metatable, a
   metatable that gains `__gc` after `setmetatable`, an error object, `_old_doc`, and a closure stored in a
   proxy. Each state is 198 bytes over idle afterwards, the same after a second call. `coroutine`, `debug`
   and `load` are nil, `getmetatable(string)` is false, TBC variables are refused in the VM (patch OP_TBC),
   weak modes are ignored by the collector (patch lgc.c traversetable), and a `__gc` added after
   `setmetatable` never marks the object. No script-reachable place survives the call. The state's string
   table does (S2).
2. Q2, attribution. `lua_alloc`'s `ud` is the `detail::State`; `busy` gives the cached state to one call
   at a time, and the call keeps it through its collect (Release is destroyed after `co_await collect`,
   processor.cc:867-884). Two interleaved calls never share a meter, so no call can free another's bytes.
   Reading plus c035c3/ce0a1e; no separate probe.
3. Q3, lifetimes under ASan (jobs 261116-114932-952936 for `probes.inc`, 261116-120004-a6b491 for
   `probes2.inc`): cancel during the run of three overlapping calls (two on states of their own) at 3, 50,
   400 and 1,500 resumes (L2); cancel at each of the first 12 resumes after the script ends, which lands in
   the run's tail and in the collect (L1, round 2: cancelled at 0 and 1); cache eviction by count and a
   changed script under the same name while a call on the old Processor is suspended, with the caller
   holding the `shared_ptr` as handler.cc:473 does (L3). No ASan report. The caller's `shared_ptr` outlives
   the call's frame and `own` outlives `run`'s frame, so a suspended call never touches a closed state. A
   frame is destroyed unresumed only at scheduler teardown (`destroy_all`), which leaves the cached state's
   garbage until its next call (no memory error).
4. Q4, reactor stalls (`probes.inc` R4, `probes2.inc` R4a, `probes_r4a_master.inc`; strict jobs
   361614, 175f61 and 261116-115432-323d6a at the branch point 5800937cf). A script that ends holding
   800,000 short strings in 800 tables: S4.
5. Q5, the cache's count (`probes.inc` R2/R3, `probes2.inc` R2): S1 and S2. `--scripts-cache-size 0` maps
   to no count bound in lru.h (`max_size ? max_size : max`), and the byte cap still binds (the suite's
   `test_a_threads_cached_scripts_hold_at_most_one_calls_cap`); a negative value parses to a huge count with
   the same result.
6. The cost of a state of its own (`probes_r5.inc` R5, strict job 261116-115032-d44115): S3.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | yes | gap | **A cached script keeps its `_params`, and the byte cap does not count them.** `Processor` stores `script_params` (processor.cc:830); `resident_bytes()` is settled bytes plus source (processor.cc:846-848). x7 S1 reproduces with `_params` in place of garbage: sequential PUTs of distinct scripts, each answered 204, each leaving its params in the thread's cache. 8 scripts with 10.6 MiB of serialized `_params` each (under the 16 MiB body cap): the cache counts 181 KiB and holds 500 MiB, 62 MiB a script. At the default 100 scripts a thread that is about 6 GiB a thread, near S1's 100 GiB at 16 reactors. Foreign scripts cache the stored params merged with the caller's too. | job 261116-114930-361614 R3: "cache counts 181 KiB ... the cache held 500 MiB" (heap in use 570 MiB with the cache, 70 MiB after the thread's cache was gone) | Options: (a) a local script's Processor keeps no params: the handler already passes `script.get_params()` to every call (handler.cc:474), so the cached copy is redundant and stale (N2); foreign scripts keep only the stored document's params and charge them; (b) charge `script_params` at its real in-memory size in `resident_bytes()`. Test: R3 passes (the heap the cache holds stays within `cache_budget()`). |
| S2 | yes | defect | **A call grows its cached state's string table, and the cache trims only on insert.** Lua shrinks the string table at most once per cycle (`checkSizes`), so the after-call collect leaves a table grown by the call: 700,000 interned strings leave 2.1 MiB on a 23 KB state (up to about 4 MiB near the cap). `trim` runs only in `remember` (processor.cc:601-608), so calls on scripts already cached take the thread past its 64 MiB with no insert. 100 cached scripts called once each: the cache counts 202 MiB against its 64 MiB budget, 3.2 times over; at 16 reactors about 3.2 GiB, not the note's "script threads x 64 MiB" (lua-memory.md:41). The next insert trims it back to 63 MiB. It also charges the call's leftover to the next call of that state (2 MiB of its 64). | jobs 361614 R1 ("resident 23503 -> 2118543 bytes", the same after 3 calls) and 175f61 R2 ("2296 KiB after compiling, 206889 KiB after one call each, 64158 KiB after the next insert") | Trim the thread's cache after a cached state's collect, when its settled bytes grew. Test: `probes2.inc` R2, `after_calls <= cache_budget()` at the default count. |
| S3 | yes | defect | **A call that overlaps another call of its script compiles the whole script on the reactor, unmetered, from a tiny request.** processor.cc:862 builds a state of its own: the base plus the full source. A 15 MiB table-constructor script compiles in 802 ms and settles at 55.8 MiB; the second of two concurrent calls held its thread 711 ms in one stretch (a 15 MiB string constant: 108 ms, 30 MiB). Once a large script is cached on a thread (one upload, or a `_foreign` stored script), any request naming it (`_name` with no body is a cache hit, processor.cc:594) while another call of it runs pays that compile on the reactor. Before this quest overlapping calls shared the cached state, so such a stall needed a 15 MiB upload per stall; now it needs about 100 bytes. Memory stays near 64 MiB per call (56 MiB compiled + the cap's remainder), so this is a reactor-time amplification, not a memory one. | job 261116-115032-d44115 R5: "compile 802.0 ms, settled 55817 KiB; two calls at once: ok/ok, longest stretch 711.3 ms" | Options: (a) a call takes a state of its own only for a script whose compile is cheap (source or settled bytes under a stated bound); larger scripts wait for the cached state (the per-call cap holds; they queue); (b) keep compiled idle states per script, counted by the byte cap; (c) keep it and carry it beside X4-N5 (compilation unmetered) in road-to-release with this number. Test: R5's longest stretch under the bound chosen, or (c) recorded. |
| S4 | yes | defect | **The after-call collect frees the whole call's garbage in one step, so the slices do not bound the stall.** Lua 5.4.7 enters each sweep with `sweeptolive` (lgc.c:1486-1490, 851-857), which frees every dead object at the head of `allgc` at once. The newest objects are the call's own, all dead in the second cycle, so one `LUA_GCSTEP` frees all of them. The longest stretch of a call that ends holding 800,000 short strings is 85 ms at fa6136633, against 42-57 ms at the branch point. Its three slices after the script's end take 2.7 ms. The cancelled path, which drops the slices, takes the same 84 ms, about the full sweep. With smaller objects up to the cap, about 1.5 M of them, I expect roughly twice that (not measured). The note's "a large heap of small garbage does not hold the reactor" (lua-memory.md:31) does not hold, and `max_steps = 1 << 16` (processor.cc:707) bounds the step count, not time. | jobs 175f61 R4a ("longest 84.69 37.62 9.74 ms ... after it 3 stretches, longest 2.70 ms") vs 323d6a at 5800937cf ("longest 42.03 11.66 4.55 ms"); 361614 R4b ("unsliced collect 84.18 ms"); R4c, the pre-existing emergency collection at a refused allocation: 39.7 ms | Options: (a) bound the first sweep in the patched lgc.c (a counted `sweeptolive`, or start the sweep at the list head without it; since no Lua runs during the after-call collect, objects created mid-sweep are not a concern there, but entersweep also runs inside calls, so the change needs the suite and fuzz_lua); (b) free the call's garbage off the reactor while the state is held busy; (c) keep it, with the stall measured and stated in the note and in T10's carried conditions. Test: R4a's longest stretch at most the branch point's (about 57 ms here) or a stated bound. A cancelled call can keep slicing with a budget without the token (N4). |
| N1 | no | gap | The note's node bound, "plus 64 MiB for each call in flight" (lua-memory.md:41), leaves out the unmetered conversion of `_doc`, `_old_doc` and params into the state (processor.cc:753-761 run before `metered` is set at :780) and compilation (X4-N5). It predates this quest. | reading | State it in the note's "Limits that remain". |
| N2 | no | defect | A cached local script serves later requests with the first request's `_params`. `cached()` matches the body only (processor.cc:594), and the call merges the cached `script_params` under the caller's (processor.cc:755), so keys the second client did not send are visible to its script. It predates this quest (master's compile_local is the same). | reading; master processor.cc:763-771 | S1's option (a) removes it. |
| N3 | no | nit | A call whose script succeeded but whose collect is cancelled reports `cancelled` after `doc` was already replaced (processor.cc:821, then :884). The handler drops a cancelled request, so nothing is written. | reading; L1 round 2: "cancelled 1 ... doc {"n":800}" | Document it, or set `doc` after the collect. |
| N4 | no | nit | On cancellation the collect runs unsliced (processor.cc:879): 84 ms here, the same as S4's step, so it adds nothing once S4 is fixed unless the fix relies on slices. | job 361614 R4b | With S4's fix, keep slicing on a budget without the cancelled token. |

## Not run

- Node-level probes (`lua_mem_seq.py` with `_params`, a flood of name-only requests for S3). Unit probes
  drive the same `Processor` and cache the handler uses. The node numbers would add the allocator's
  retention (A1's 0.4 MiB a script).
- S4's worst case near the cap with the smallest objects, and S4 on Linux and FreeBSD (glibc and jemalloc
  free costs differ). Timings are macOS x86_64 under strict holds beside light jobs.
- TSan beyond 929a79; UBSan; fuzz_lua under ASan.
- A1's seq numbers, A3/A5 (T10 rows) and the docs-only commits after fa6136633: the correctness review's.
