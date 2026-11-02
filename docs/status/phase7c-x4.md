> Status: CURRENT (2026-11-01) the X4 developer's ledger (phase 7c step X4: Lua runs in a coroutine that yields, D10); building; not pushed.

# phase7c X4 (build): Lua in a yielding coroutine

Branch `phase7c/x4-lua` in `Xapiand-wt/x4` (local only, from master `Xapiand:d5a2e746c`). Design: `docs/design/phase7c.md` D10, T10; binding
review items R1, C11, C12, C13 (`docs/reviews/phase7c-design.md`); the architect's probes `Xapiand-wt/phase7c-design/.scratch/review-7c/recheck1/`.

## State

Steps K1 to K5 done and committed; the build is proved on Release, Debug and ASan+UBSan (unit, docs E2E), TSan (script and pool tests), with a mutant per rule. Not pushed; no review yet (this changes how untrusted scripts run, so
a security review of the sandbox is worth the lead's one review: see "Open for review").

## Done criteria

- [x] **K1. Isolation.** `tests/test_lua.cc` `test_interleaved_calls_keep_their_own_doc_and_globals` is the architect's L4/L5 probe: calls A and B of one script take turns (budget of 1 ns, `inline_scheduler`),
  B writes `_G.leak`, `string.leak`, `table.leak`, `math.leak`; A ends with its own `_doc`, sees none of them, and `_G == _ENV`. `test_the_calls_really_interleaved` shows the alternation,
  `test_writes_do_not_outlive_their_call` that the next call on the same processor starts clean, `test_the_environment_is_closed` that the metatables and `load`/`dofile`/`collectgarbage` are shut.
  `fuzz_lua` checks, on random scripts, that three interleaved calls end exactly as three sequential ones.
- [x] **K2. Runaway scripts.** Unit tests (`test_lua`) and live (`harness/lua_bounds.py`, Release node): every case answers **400** and the node serves after it.

  | Case | Ends at | Live |
  |---|---|---|
  | `while true do end`; the same inside a `pcall`/`xpcall` loop | instruction cap 10^8 (`quick()` 2x10^6 in unit tests, the default in one) | 400 in 0.56 s, 0.58 s |
  | `gsub` callback, `__lt` run by the native sort, each also under `pcall` | 100 hook calls without a yield point | 400 in 0.12 s, 0.17 s |
  | sort comparator that never returns | instruction cap (the comparator is Lua, so it yields) | 400 in 0.59 s |
  | `rep('a*', 12) .. 'b'` on 200 bytes; quadratic `find`, `gmatch`, `gsub` on 20,000 bytes; the exponential one under a `pcall` loop | matcher step budget (the patch; drift test) | 400 in 0.12 s each |
  | `string.rep('x', 2^30)`, table growth, `pcall(rep)` loop | 64 MiB byte cap (8 MiB in unit tests) | 400 in 0.12 s, 1.1 s |

  A finite 100,000-element `table.sort` with a comparator completes inside the default caps (C11); legit `gmatch`/`gsub` over 1,000,000 bytes pass. `test_the_state_serves_the_next_call_after_every_bound`.
  The patch: `contrib/lua/lstrlib-step-budget.patch`, applied at configure time to the hash-pinned tarball; `ctest -R test_lua_patch_drift` fails when the tarball, the patch or the compiled `lstrlib.c` changes
  (checked: an unpatched file and a one-byte change both fail it). Documented in `docs/dev/patches.md`.
- [x] **K3. Yielding.** `test_a_long_script_lets_other_work_run`: 5x10^6 instructions run as about 500 demoted slices; 200 turns of other work finish first, with the script unfinished. Live
  (`lua_bounds.py`, Release, 16-CPU host under other load): light `GET` p50/p99/max **0.73/1.26/2.39 ms alone** and **0.66/1.08/2.49 ms beside four concurrent scripts** of 8x10^7 instructions
  (each answered 204 in about 1.5 s). `.scratch/lua_bounds_rel.json` holds the numbers and the loops' lag deltas. The lead measures T4/T10 on `main`.
- [x] **K4.** `grep -rn sync_wait src/lua src/script.*` is empty. `harness/async_check.sh --strict --extern strata=...`: exit 0, 243 hits (190 allowed, 40 owed, 13 compiled out), **0 unclassified**, no stale row
  (the three Lua rows are removed). Mutant: `engine::sync_wait` back in `compile` makes it exit 1 (`unclassified 1`).
- [x] **K5. Proof** (at the commit on the last line of "Done"):
  - Unit tests 29 of 29 on **Release**, **Debug** and **ASan+UBSan** (`detect_leaks=0`), each rebuilt at the head. TSan: `test_lua`, `test_lua_patch_drift`, `test_pool`, `test_engine_host` 4 of 4, 0 reports.
  - Docs E2E (newman 6.2.3, `harness/engine_e2e.sh`): **641 of 641** (371 requests) on the Release, Debug and ASan servers (Lua examples included). A TSan server run was queued behind the host's disk hold (under 20 GiB free) and not done; TSan covers the unit targets only.
  - `fuzz_lua` (random scripts, interleaved against sequential): 6,077 scripts on Release and 2,954 under ASan+UBSan in 60 s, no violation.
  - **Mutants**, one per rule, each killed:

  | | Rule | Mutant | Killed by |
  |---|---|---|---|
  | M1 | `_G` is the env (C13) | `env._G = base` | the probe's `_G == _ENV` and `glob` checks |
  | M2 | library tables per call (C13) | `env.string = real string` | `sleak`, `tleak`, `mleak`, `sb`, `tb` (it **survived** the first probe, whose B wrote after A had read; reordered) |
  | M3 | one env per call (R1) | env shared | `mine`/`seen`, `glob`, `before` |
  | M4 | the hook yields | no `lua_yield` | `resumes > 4`, the other-work test, the cancel test |
  | M5 | instruction cap | counter never grows | watchdog (`while true`), rc 3 |
  | M6 | non-yieldable bound | no bound | the callback message check (it ends at the instruction cap instead) |
  | M7 | byte cap | allocator never refuses | `rep.limit`, the message check, watchdog |
  | M8 | `pcall` cannot swallow a bound | wrapper catches all | watchdog |
  | M9 | comparators yield (C11) | native `table.sort` | the 100,000-element sort |
  | M10 | matcher step budget | patch reverted | watchdog on the exponential pattern; the drift test |
  | M11 | budget per API call (C12) | budget reset in `reprepstate` | `quadratic`, `gmatch`, `gsub` limit checks; the drift test |
  | M12 | conversion depth bound | cap 10^8 | the cycle test crashes (rc 139) |
  | M13 | thread anchored (C13) | `lua_pop` instead of `luaL_ref` | ASan abort in `test_lua` (rc 134) |
  | M14 | the matcher's error is fatal | catchable | watchdog (a `pcall(string.find)` loop) |
  | M15 | no `sync_wait` (K4) | `engine::sync_wait` in `compile` | `async_check.sh --strict` exit 1 |

## Design as built

- **Environment** (`src/lua/processor.cc`, `base_chunk`): a script compiles once as `local _ENV = ...; <body>`. A call builds an environment table over a shared
  base (`env_mt = {__index = base, __metatable = false}`): `_G` is the environment itself (C13), `string`, `table`, `math` and `utf8` are per-call empty
  proxies over per-state copies of the libraries (a write stays in the call), the string metatable is locked, and `load`, `loadfile`, `dofile`, `collectgarbage`,
  `require`, `io`, `os` and `debug` do not exist. The registry's globals table is emptied after the base is built, so nothing hands out the real globals.
  Each call runs in its own `lua_newthread`, anchored by a registry slot released when the call ends (C13).
- **Slices** (D10): `lua_sethook(co, hook, LUA_MASKCOUNT, 10000)`; the hook yields (`lua_yield(L, 0)`) whenever `lua_isyieldable`, and the call awaits the
  request's `task::budget::checkpoint()` between resumes. A budget of null runs the call to its end. Single-document writes give their script a budget
  without the cancel token (an admitted write completes, T9); RESTORE uses the request's budget through `DocIndexer`.
- **Bounds**: instructions (hook calls x 10,000, cap 10^8); 100 hook calls in a row with no yield point (a `gsub` callback, an `__lt` run by the native
  sort); live bytes of the state's `lua_Alloc` (64 MiB, charged while a script runs, shared by interleaved calls on that script); the matcher's step
  budget (`contrib/lua/lstrlib-step-budget.patch`, set in `prepstate` so it spans an API call, C12). A hook-raised bound raises a sentinel table that the
  base's `pcall` and `xpcall` rethrow; the matcher's error and "not enough memory" are rethrown too, so a script cannot loop around a caught bound. All four end the
  call with `lua::ScriptLimitError` (a `ClientError`, 400). Any other script error is logged and leaves the document as it was (unchanged behaviour).
- **`table.sort`** is a stable merge sort in Lua when a comparator is given (C11), so comparators yield; without a comparator the native sort runs.
- **Caches**: each thread has its own LRU of compiled scripts (`thread_local`), so the per-script locks are gone. A foreign script's document is read with
  an await in `Processor::compile`; `Script::process_script` (called from the synchronous schema code) compiles a local script and leaves a foreign one to be
  compiled, and its document read, at its first run. A script saved under an existing name replaces the cached one (the old `emplace` kept the stale one).
- **Conversion** between documents and tables uses the raw Lua API (no sol2, which is no longer fetched), reads tables raw (a script's metamethods never run outside
  the hook), and is bounded in depth (200) and nodes (`max_bytes / 16`), so a cycle or a table that shares its parts is a 400-class refusal, not a crash.
- **Not done / stated**: the byte cap is a state's live bytes, so interleaved calls on one script share it (a hog can starve a sibling call for its life);
  `process_script` no longer checks at schema time that a foreign script exists.

## Done

- `Xapiand:22d6ff437` Lua pattern matcher step budget: patch, pinned tarball hash, drift test, `docs/dev/patches.md`.
- `Xapiand:52dfd2040` Lua in a yielding coroutine with its own environment; thread-local caches; awaited foreign read; raw conversion; no sol2; `test_lua`, `fuzz_lua`.
- `Xapiand:162d1c369` `harness/lua_bounds.py`, async allowlist rows removed, scripting guide and changelog.
- `Xapiand:95cb14ba3` `__gc` refused to scripts (a finalizer runs with hooks off); the interleave probe and the persistence test made to bite (M2 survived the first version).
- This ledger and the final proof: the commit that adds it.

## Open for review

- A script's time in C code is bounded per call (64 MiB) but not per hook interval: a loop of non-allocating O(n) calls on a large string (`string.find(big, 'x', 1, true)`, `utf8.len`, string comparison) spends its time between two hook
  calls. Allocating ones are bounded by the byte cap and the instruction cap together. A fix needs a work counter charged by those calls (or a smaller cap on any one string); I left it as D10 states it.
- `Script::process_script` (synchronous schema code, X5's area) no longer reads a foreign script's document to check it exists; the error moves to the first run. Making it a coroutine means making the schema's dispatch
  coroutines, which belongs to X5; no file outside `src/lua`, `src/script.*`, their callers (`database/handler.cc`, `server/search_views.cc`: a `task::budget` for single-document writes), CMake and the harness changed.
- The byte cap is the state's live bytes, shared by interleaved calls of one script; a hog can fail a sibling call.
- Removed from scripts: `load`, `loadfile`, `dofile`, `collectgarbage`, `__gc`; a changelog entry says so.
- Host: `quiet check` showed under 20 GiB free for most of this step, which held every build for minutes; x3's build directories were deleted underneath my builds (I now carry my own copy of the fetched sources in `.scratch/deps-rel`).

## Next

The lead's review, then the lead's T4/T10 measurement (`harness/lua_bounds.py --loops 4`, or `stall_load.py` extended with it). X5 should call `Processor::compile` (a task) if its schema dispatch becomes coroutines.

## Blocked on

Nothing.
