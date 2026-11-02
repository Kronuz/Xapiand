> Status: CURRENT (2026-11-01) the patches Xapiand's build applies to third-party sources; maintained by the X4 developer (phase 7c D10).

# Patches to fetched sources

Xapian is a fork with its own procedure (`xapian-fork.md`). The sources below are fetched pinned and patched at configure time.

## Lua 5.4.7: the matcher's step budget

`contrib/lua/lstrlib-step-budget.patch` (applied by `FetchContent_Declare(lua ... PATCH_COMMAND)` in `CMakeLists.txt`) gives
`lstrlib.c`'s pattern matcher a step budget. Lua bounds the matcher's recursion depth (`MAXCCALLS`, 200) but not its steps, so
`string.find(string.rep('a', 200), string.rep('a*', 12) .. 'b')` runs for years inside one C call that no hook can interrupt.

- `MatchState` gains `steps` and `budget`. `match` counts its entries and raises `pattern too complex (step budget)` past the budget.
- The budget is set in `prepstate`, once per API call, to max(10^5, 8 x subject length) (an estimate). `reprepstate`, which
  `string.find`, `gsub` and `gmatch` call per start position or iteration, leaves it alone: a budget reset there never trips on a quadratic search
  (the architect's C12).
- `lua/processor.cc` makes that error fatal to the script call: its `pcall` and `xpcall` rethrow it, and the call ends with a
  `ScriptLimitError` (a 400).

Drift: `contrib/lua/lua-version.cmake` pins the tarball, `lstrlib.c` before the patch and `lstrlib.c` after it (SHA-256).
`ctest -R test_lua_patch_drift` (`tests/lua_patch_drift.cmake`) unpacks the tarball the build fetched, checks its hash, applies the patch, checks the
patched file's hash and that the build compiled that file. A changed Lua version, tarball or patch fails it. `test_lua`
(`tests/test_lua.cc`) runs the quadratic and the exponential pattern against the compiled library.

To move to another Lua release: update the URL and hashes in `lua-version.cmake`, regenerate the patch against the new `lstrlib.c`
(`diff -u a/src/lstrlib.c b/src/lstrlib.c`), update the patched hash, and run both tests.
