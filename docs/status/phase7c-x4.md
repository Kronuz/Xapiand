> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-11-02) the X4 developer's ledger (phase 7c step X4: Lua runs in a coroutine that yields, D10); the four blockers of the security review are fixed and proved, awaiting the security reviewer's re-check; not pushed.

# phase7c X4 (build): Lua in a yielding coroutine

Branch `phase7c/x4-lua` in `Xapiand-wt/x4` (local only, from master `Xapiand:d5a2e746c`). Design: `docs/design/phase7c.md` D10, T10; binding
review items R1, C11, C12, C13 (`docs/reviews/phase7c-design.md`); the architect's probes `Xapiand-wt/phase7c-design/.scratch/review-7c/recheck1/`.

## State

Steps K1 to K5 done (below). The security review (`docs/reviews/phase7c-x4-security.md`, REJECT, four blockers) is folded in: X4-S1 to X4-S4, N1 and N3 are fixed in four commits, each with a regression test built from the reviewer's probe, a mutant, and the probes rerun on the fixed Release binary (table under "Security fixes"). Unit tests, docs E2E and the harness are recorded there. Not pushed. Waiting for the security reviewer's re-check with its probes.

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

## Security fixes (review `docs/reviews/phase7c-x4-security.md`)

Mechanism: `docs/mechanisms/lua-bounds.md`; patch list: `docs/dev/patches.md`; design: D10 and T10.

| Finding | Before (the review) | After (fixed Release node, the reviewer's probes rerun) | Regression test |
|---|---|---|---|
| X4-S1 closing variable outlives the cap | one call: 100% CPU, never ends; SIGINT: still running after 20 s; N=10 in flight: 0 of 20 fresh GETs answered; `xpcall` shape the same | `p14`: call ends (`call still running=False`, 400 at the instruction cap), SIGINT exits within 20 s; 1 to 39 in flight: 20 of 20 GETs each time, max GET 4 ms; a plain write answers 204; `p2b` (4 at once) GET max 8 ms | `test_a_script_cannot_declare_a_to_be_closed_variable`, `test_a_close_handler_cannot_outlive_a_bound` |
| X4-S2 work the hook cannot see | plain find 1 MiB in 512 KiB: 15.9 s, 204; 31 MiB search loop: GET waited 16.3 s; matcher 128K/64K: 49 s; vararg chain: no end in 120 s; ephemeron chain 300K: one 56 s stall | plain find quad: 400 in 0.15 s; matcher 512K/256K: 400 (190 ms over base); `p7` loops: GET max 1.5 to 2.2 ms (one `gsub` that completes over 8 MiB: 446 ms); `p18`: call ends, GET max 13 ms over 40 s; weak-mode `setmetatable` refused (400 in 0.1 s) | `test_library_calls_are_charged_by_what_they_touch` (21 loops), `test_a_slow_search_ends_at_the_work_cap`, `test_the_matcher_is_charged_by_work_not_by_entries`, `test_a_vararg_chain_ends_at_the_work_cap`, `test_weak_tables_are_refused_and_never_weak`, `test_a_charged_loop_yields_...` |
| X4-S3 result conversion | 1 MiB x 256 refs: 11.5 s, 2.4 GB; 8 MiB x 256: killed at 6 GB; 2M refs of a small object: killed at 6 GB | 1 MiB x 16 to 256 refs, 4 and 8 MiB x 64/256: 413 in 0.2 s, peak RSS under 60 MB; 500,000 and 2,000,000 refs: 413 in 1.4 s (peak 780 MB while walking, cut by the unit weights below) | `test_a_result_past_its_byte_budget_is_refused`, `test_a_result_past_its_node_budget_is_refused` |
| X4-S4 cache key collision | attacker's name replaced/stood in for the victim's foreign script: 8 of 8 (6 of 8 default node); rewrites never seen | `p10`: victim keeps `legit` in both orders; the rewrite is seen (`rewritten`) after the 1 s trust | `test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference`; `lua_bounds.py` foreign section |
| N1 | 8 MiB print x 20 wrote 160 MiB; log lines of 8 and 32 MiB | `p9`: log +0.00 MiB for every case | `test_print_is_bounded_per_call`, huge-message check in `test_a_script_error_is_a_client_error_...` |
| N3 | script errors answered 2xx | 400 with the script's message | same test |

The unit of work is about a nanosecond (`Limits::max_work` 5 x 10^8 per call). A single library call is still uninterruptible: it is bounded by the 31 MiB string the memory cap allows, up to about 0.1 s (`utf8.len`, `upper`) and 0.5 s for a matcher call that completes over 8 to 31 MiB. The conversion back to MsgPack costs about 6 us a value on the reactor with no yield.

Weights: a result is 16 MiB of strings and 262,144 units (scalar 1, array 2, object 8) after the first run showed 100,000 small objects costing 890 MB and 3.9 s.

Mutants (Release `test_lua`, `.scratch/mutants-x4*.txt`; each applied to the built source, rebuilt, restored): killed: OP_TBC and OP_TFORPREP accept a closable value; setmetatable accepts `__close`; lmemfind charges nothing; matcher counts nothing per item; a class member costs nothing; matcher steps not charged to the meter; rep charges nothing; upper/lower/reverse; `..`; string equality; utf8.len; table.concat/insert/unpack; stack-value moves (varargs, tail calls, returns); weak tables honoured; no early yield; native `table.sort` again; result bytes and values not charged; local and foreign keys share a namespace; messages not truncated; script errors swallowed again. **Survived: dropping the condemned flag from `fatal()`, alone and together with the per-instruction raise.** Three layers end a swallowing call (the flag, the hook raising at every instruction once condemned, and the host judging the call's outcome by `reason` after any exit), so no unit test isolates the flag; the language layer (no closable value) and the live `p14` shape are what the tests and probes prove. A flag-only test would need a script that survives the other two layers; follow-up.

Run (this host, other agents' builds running; no benchmark, so no timing here is a measurement): Release `ctest` 29 of 29 and Debug 29 of 29 (at `Xapiand:ee597976d`); ASan+UBSan 28 of 29 at that commit, the one failure being `test_lua`'s whole-run 120 s watchdog on the slow build (695 checks passed standalone); the watchdog is now per call and 600 s, and `test_lua` plus `test_lua_patch_drift` then passed on Release, Debug and ASan+UBSan (2 of 2 each); TSan on the Lua tests (`test_lua`, `test_lua_patch_drift`): 2 of 2, 0 reports (516 s). `fuzz_lua` under ASan+UBSan: 167 scripts in 60 s, no violation (Release 1,438 in 60 s earlier, no violation). Docs E2E (newman 6.2.3, Release): **641 of 641** (371 requests); its first run failed 3 assertions because three `update-api.md` examples used a removed Python-style API (`_doc.hashtags.append(tag)`, `erase`) whose script error used to be swallowed as a 204; the examples are Lua now. `harness/lua_bounds.py` on the Release node: PASS, 30 bound cases at their statuses (400, or 413 for oversized results), 10 concurrent swallowers answer 400 with 20 of 20 fresh GETs answered, graceful stop completes, foreign scripts hold in both orders and the rewrite is seen; light GET p50/p99/max 0.72/1.04/1.30 ms alone, 0.49/1.09/1.99 ms beside 4 long scripts, 0.53/1.61/3.18 ms beside 4 library-call loops (all 4 loops 204).

Follow-ups (not blockers): flag-only unit test; X4-N2 (collect a state's garbage at call end, size `--scripts-cache-size` against threads x 10 x 64 MiB); X4-N5 (compile is synchronous and unmetered); X4-N7 (`lua_call` and the call's table build run unprotected); X4-N8 (`tostring(function)` leaks an address; `math.random` state shared across a script's calls); a matcher call that completes over 31 MiB cannot yield (slice `gsub`/`gmatch` with a continuation); `next` on a table drained from the front scans on every call; conversion in slices; a foreign script's cache key ignores the referrer's params; `string.pack` family could return charged.

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
- Security fixes: `Xapiand:a62c1277a` (X4-S1, X4-S2, N1, N3: the limits patch, the condemned flag, the meter), `Xapiand:9729de23e` (X4-S3), `Xapiand:4700151bf` (X4-S4), `Xapiand:ee597976d` (fuzz, harness, docs), then the commit that adds the per-call watchdog, the `update-api.md` examples, the harness fix and this section.

## Open for review

The security reviewer's re-check with its probes (`.scratch/review-x4/`, rerun results in `.scratch/review-x4-after/out/`). The first-session "open" items are closed by X4-S2 (the loop of non-allocating calls is charged and sliced). Still stated: the byte cap is a state's live bytes, shared by interleaved calls of one script; `Script::process_script` (X5's area) no longer reads a foreign script's document at schema time; `load`, `loadfile`, `dofile`, `collectgarbage`, `string.pack` and family, to-be-closed variables and `__gc`/`__close`/`__mode` are removed from scripts (changelog says so).

## Next

The security re-check, then the lead's T4/T10 measurement (`harness/lua_bounds.py --loops 4`, now with library-call loops). X5 should call `Processor::compile` (a task) if its schema dispatch becomes coroutines.

## Blocked on

Nothing.
