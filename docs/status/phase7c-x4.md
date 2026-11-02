> Status: CURRENT (2026-11-01) the X4 developer's ledger (phase 7c step X4: Lua runs in a coroutine that yields, D10); building; not pushed.

# phase7c X4 (build): Lua in a yielding coroutine

Branch `phase7c/x4-lua` in `Xapiand-wt/x4` (local only, from master `Xapiand:d5a2e746c`). Design: `docs/design/phase7c.md` D10, T10; binding
review items R1, C11, C12, C13 (`docs/reviews/phase7c-design.md`); the architect's probes `Xapiand-wt/phase7c-design/.scratch/review-7c/recheck1/`.

## State

Starting: design read, code written (see Done), nothing built yet.

## Done criteria

- [ ] **K1. Isolation.** Two interleaved calls of one script keep their own `_doc` and globals, including writes through `_G` and into `string`.
- [ ] **K2. Runaway scripts.** Infinite loop (instruction cap); comparator runaway (non-yieldable bound); exponential pattern (matcher step budget, patch plus drift test); memory hog (memory cap); each a 4xx; the reactor keeps serving.
- [ ] **K3. Yielding.** A long script yields to the reactor; other requests are served meanwhile.
- [ ] **K4.** No `sync_wait` in `src/lua`; `async_check.sh --strict` reports 0.
- [ ] **K5. Proof.** Unit tests on Release, Debug, ASan+UBSan; TSan on the script tests; docs E2E 641/641; a mutant per rule.

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

(none yet)

## Next

Build `test_lua` on Release, then the K steps in order.

## Blocked on

Nothing.
