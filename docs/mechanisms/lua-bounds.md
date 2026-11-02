> Status: CURRENT (2026-11-02) how a Lua script call is bounded, as built on `phase7c/x4-lua` (phase 7c step X4); the design is `docs/design/phase7c.md` D10. Security-reviewed once (`docs/reviews/phase7c-x4-security.md`); the four blockers it found are fixed here.

# Lua bounds

## What

A script arrives in a client's request, so a call must neither pin the reactor that runs it nor exhaust the node. Every call has a
set of meters; the first to cross its bound ends the call, and the request answers 4xx. The bound is on work done, not on calls made.

## How

```
 script call (a coroutine on the reactor)           host meters (Call)              ends the call with
 ----------------------------------------           ------------------              ------------------
 VM instructions, counted by the hook ───────────►  instructions  (10^8)            400 instruction limit
 library + VM work, charged by the patch ─────────►  work          (5 x 10^8)        400 work limit
 allocations, in lua_alloc ──────────────────────►  live bytes    (64 MiB)          400 memory limit
 matcher steps in one API call ──────────────────►  steps         (64 x subject)    400 step budget
 hook periods inside a callback that cannot yield ►  unyielded     (10^6 instr.)     400 callback limit
 result conversion back to MsgPack ──────────────►  bytes / units  (16 MiB / 2^18 units) 413 result too large
```

1. **Work is a meter, in units of about a nanosecond.** `contrib/lua/xapiand-limits.patch` makes the library call the host
   (`lua_xcharge`, `lua_xnote`) with what it touches: a plain `find` charges the bytes `memchr` scans and `memcmp` compares as it
   scans, the matcher charges 4 units a step, `rep` / `sub` / `upper` / `reverse` / `format` / `..` / string compare / `utf8.*` /
   `table.concat` / `unpack` / `insert` / `remove` / `move` charge their lengths, and a tail call, a return or `...` of many values
   charges the move. `docs/dev/patches.md` lists every site. A call that has spent `Limits::slice_work` (200,000 units) since it
   last yielded arms the count hook to fire at its next instruction, so it yields after about 0.2 ms of C work instead of after
   10,000 instructions; the instruction count itself still caps the call. D10's earlier claim, that code with no Lua
   instructions is bounded by the memory cap's sizes, was false for time: 1,400 plain finds over a 31 MiB string ran 3.5 s
   between two hook calls.
2. **A condemned call cannot be caught.** The first meter to cross its bound sets `Call::reason`, arms the hook to fire at every
   following instruction, and (in the VM and the library) raises the sentinel. The base's `pcall` and `xpcall` ask the host
   (`condemned()`) after every error and rethrow when it says yes, whatever value the error carries; `operator()` decides what
   ended the call from `reason`, not from the error's value. A to-be-closed variable could replace the in-flight error with a
   string from its handler, which is why the language no longer has them: `OP_TBC` and `OP_TFORPREP` refuse a closable value, and
   `setmetatable` refuses `__gc`, `__close` and `__mode` (the last is also ignored by the collector, since a script can add the
   field after `setmetatable`).
3. **Functions the host cannot charge are gone.** `string.pack`, `unpack`, `packsize` and `dump` are not in the base; `table.sort`
   is a merge sort in Lua (comparisons are instructions, so it counts and yields); `print` is a Lua function that hands one
   string to a host function that logs at most 1 KiB a line and 4 KiB a call, with a marker.
4. **Stack-proportional work.** `LUAI_MAXSTACK` is 200,000 slots, and the sites that move `n` stack values charge `2n`
   (`...`, a tail call, a return, `SETLIST`, `unpack`). A chain of `f(1, ...)` calls grows its argument list by one per call and
   copies it each time: it reaches the work cap at about 16,000 arguments.
5. **The result is converted under a budget.** `lua_to_msgpack` charges string and key bytes against 16 MiB (the buffered body cap,
   `BUFFERED_BODY_CAP`) and value units against 262,144 (`Limits::max_result_nodes`; the host spends about 500 bytes on a scalar and about 4 KiB on a small
   object, so a scalar is 1 unit, an array 2 and an object 8: the budget is about 128 MB). A table reached twice counts twice; a dropped entry costs a unit. Past either budget the request
   answers `413`. Depth is capped at 200.
6. **Scripts are cached apart.** A local script is keyed `L<name>` and a foreign one `F<reference>`, so a client-chosen name never
   is a foreign key. A cached foreign script is trusted for `Processor::foreign_revalidation()` (one second), then its document is
   read again; the cached processor stays only if the script body and params are what it was built from.
7. **A script's own error is a 400.** `ScriptError` carries the message, cut at 1 KiB with a marker.

## Why

The first design bounded instructions and memory, which a loop of library calls over a large string sidesteps: the instruction hook
cannot fire inside a C call, so time between two hook calls was unbounded by anything. Painless (Elasticsearch) charges its
regexes by a complexity factor of the input length and counts loop iterations; this charges every library call by what it touches
and the interpreter's bulk operations by their lengths, then lets one cap and one yield rule serve all of them.

## Where it is tested

`tests/test_lua.cc` (`test_a_close_handler_cannot_outlive_a_bound`, `test_library_calls_are_charged_by_what_they_touch`,
`test_a_slow_search_ends_at_the_work_cap`, `test_the_matcher_is_charged_by_work_not_by_entries`, `test_a_vararg_chain_ends_at_the_work_cap`,
`test_weak_tables_are_refused_and_never_weak`, `test_a_result_past_its_byte_budget_is_refused`,
`test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference`), `tests/fuzz_lua.cc`, the drift test, and on a live node
`harness/lua_bounds.py`.

## Limits that remain

A single library call is uninterruptible: the most one makes is bounded by the memory cap (a 31 MiB string), about 0.1 s for
`utf8.len` and `upper`. The conversion back to MsgPack runs on the reactor without a yield, about 6 microseconds a value, so
a result at its budget holds the loop for up to about 1.5 s. `next` on a table drained from the front scans its array part on every
call. These are follow-ups.
